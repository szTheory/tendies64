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

/* The idle digest of the stem id (no save device), captured at the 09-03 tree
 * before the save plan's first edit (the section 11 addition must not move it)
 * and re-captured by the 09-10 fix cycle (STATE-CONTENT-F1): section 8 of the
 * composite family gained the carried RDP tail (combiner block and memory
 * coverage table), so every family digest moved, and nothing else did. */
#define STEM_IDLE_DIGEST \
  "ec4be64f3cb70299e4964b74f866fb753208dbad163a9a01ceb5871a03e3a5d8"

static void check_mid_dma_direction(void);

static void case_storage_blank(void) {
  size_t index;

  check_mid_dma_direction();

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

/* The in-flight PI direction is part of section 11: a digest taken mid-DMA
 * differs from the idle digest, and changing the direction of the active
 * descriptor alone (through direct state) changes it again. */
static void check_mid_dma_direction(void) {
  t64_machine *machine = make_idle_machine(S32K_ID);
  uint8_t idle[T64_DIGEST_SIZE];
  uint8_t mid[T64_DIGEST_SIZE];
  uint8_t flipped[T64_DIGEST_SIZE];
  uint8_t restored[T64_DIGEST_SIZE];

  if (machine == NULL) return;
  digest_of(machine, idle);
  T64_CHECK_INT(bus_w32(machine, PI_DRAM, UINT32_C(0x3800)), T64_STATUS_OK);
  T64_CHECK_INT(bus_w32(machine, PI_CART, UINT32_C(0x08000100)),
                T64_STATUS_OK);
  T64_CHECK_INT(bus_w32(machine, PI_RD_LEN, 15u), T64_STATUS_OK);
  T64_CHECK_INT(machine->pi.active_save_direction, 2u);
  digest_of(machine, mid);
  T64_CHECK(memcmp(idle, mid, T64_DIGEST_SIZE) != 0);
  machine->pi.active_save_direction = T64_PI_DIRECTION_SRAM_TO_RDRAM;
  digest_of(machine, flipped);
  T64_CHECK(memcmp(mid, flipped, T64_DIGEST_SIZE) != 0);
  machine->pi.active_save_direction = T64_PI_DIRECTION_RDRAM_TO_SRAM;
  digest_of(machine, restored);
  T64_CHECK(memcmp(mid, restored, T64_DIGEST_SIZE) == 0);
  t64_machine_destroy(machine);
}

/* ------------------------------------------------------------------------ */
/* sram-dma                                                                  */
/* ------------------------------------------------------------------------ */

static const uint8_t dma_pattern[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
                                        0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB,
                                        0xCC, 0xDD, 0xEE, 0xFF};
#define DMA_DRAM UINT32_C(0x3800)
#define DMA_BACK_DRAM UINT32_C(0x3A00)
#define DMA_CART UINT32_C(0x08000100)
#define SRAM_OFFSET 0x100u

/* Start a PI DMA through the host bus: latch DRAM and cart, then the length
 * register; returns the status of the length write. */
static t64_status pi_start(t64_machine *machine, uint32_t dram, uint32_t cart,
                           uint32_t length_reg, uint32_t raw_length) {
  T64_CHECK_INT(bus_w32(machine, PI_DRAM, dram), T64_STATUS_OK);
  T64_CHECK_INT(bus_w32(machine, PI_CART, cart), T64_STATUS_OK);
  return bus_w32(machine, length_reg, raw_length);
}

static bool storage_is(const t64_machine *machine, size_t offset,
                       const uint8_t *expected, size_t count) {
  return machine->save.bytes != NULL &&
         memcmp(machine->save.bytes + offset, expected, count) == 0;
}

static const uint8_t zero_block[16] = {0};

/* One accepted pattern write at tick `start` (RDRAM to SRAM), settled. */
static void sram_write_pattern(t64_machine *machine) {
  const uint64_t start = machine->tick;

  (void)memcpy(machine->ram + DMA_DRAM, dma_pattern, sizeof dma_pattern);
  T64_CHECK_INT(pi_start(machine, DMA_DRAM, DMA_CART, PI_RD_LEN, 15u),
                T64_STATUS_OK);
  T64_CHECK(advance_guest_to(machine, start + 9u));
  T64_CHECK_INT(bus_value(machine, PI_STATUS), 0u);
  T64_CHECK_INT(bus_w32(machine, PI_STATUS, 2u), T64_STATUS_OK);
}

typedef struct {
  const char *label;
  uint32_t dram;
  uint32_t cart;
  uint32_t raw_length;
} refusal_row;

static void dma_refusal_rows(void) {
  const uint32_t rdram = 0x400000u; /* 4 MiB: installed on the family ids */
  const refusal_row rows[] = {
      {"window overflow", DMA_DRAM, UINT32_C(0x08007FF8), 15u},
      {"odd length", DMA_DRAM, DMA_CART, 14u},
      {"odd cart address", DMA_DRAM, UINT32_C(0x08000101), 15u},
      {"misaligned dram address", UINT32_C(0x3804), DMA_CART, 15u},
      {"dram span beyond installed rdram", rdram - 8u, DMA_CART, 15u},
      {"cart below the window", DMA_DRAM, UINT32_C(0x07FFFFF0), 15u},
  };
  size_t index;

  for (index = 0u; index < sizeof rows / sizeof rows[0]; index++) {
    t64_machine *machine = make_idle_machine(S32K_ID);
    uint8_t snapshot[T64_SAVE_SRAM_BYTES];
    uint8_t before[T64_DIGEST_SIZE];

    if (machine == NULL) continue;
    T64_CHECK_INT(machine->profile.rdram_bytes, rdram);
    sram_write_pattern(machine); /* the accepted baseline, same setup */
    T64_CHECK(storage_is(machine, SRAM_OFFSET, dma_pattern, 16u));
    (void)memcpy(snapshot, machine->save.bytes, sizeof snapshot);
    digest_of(machine, before);
    {
      const t64_status status =
          pi_start(machine, rows[index].dram, rows[index].cart, PI_RD_LEN,
                   rows[index].raw_length);

      if (status != T64_STATUS_BUS_DEVICE_REFUSED) {
        fprintf(stderr, "refusal row '%s': status %u\n", rows[index].label,
                (unsigned)status);
      }
      T64_CHECK_INT(status, T64_STATUS_BUS_DEVICE_REFUSED);
    }
    T64_CHECK_INT(bus_value(machine, PI_STATUS) & 1u, 0u);
    T64_CHECK(memcmp(snapshot, machine->save.bytes, sizeof snapshot) == 0);
    (void)before;
    t64_machine_destroy(machine);
  }
}

/* A CPU store to the PI read-length register of the -s32k id: the authored
 * program loads the PI base into $t0 (lui 0xA460), 7 into $t1 and stores it at
 * offset 8, so the length is 8 and the completion is 1 + 8/2 = 5 ticks after
 * the start. The store outside RDRAM reserves one extra scheduler event beyond
 * the CPU successor (the existing default budget rule), so free room 1 is
 * refused with QUEUE_FULL and room 2 is accepted. */
#define OP_LUI_T0_PI UINT32_C(0x3C08A460)
#define OP_ADDIU_T1_7 UINT32_C(0x24090007)
#define OP_SW_T1_RD_LEN UINT32_C(0xAD090008)
#define STORE_PROGRAM_ADDRESS UINT32_C(0x1000)
#define STORE_QUEUE_LIMIT 16u

static void put_be32(uint8_t *bytes, uint32_t value) {
  bytes[0] = (uint8_t)(value >> 24);
  bytes[1] = (uint8_t)(value >> 16);
  bytes[2] = (uint8_t)(value >> 8);
  bytes[3] = (uint8_t)value;
}

static bool pi_event_tick(const t64_machine *machine, uint64_t *tick_out) {
  size_t index;

  for (index = 0u; index < machine->queue_count; index++) {
    const t64_event *event =
        &machine->events[(machine->queue_head + index) %
                         T64_EVENT_QUEUE_CAPACITY];

    if (event->device == T64_DEVICE_PI) {
      *tick_out = event->tick;
      return true;
    }
  }
  return false;
}

static void dma_cpu_store_row(size_t room) {
  t64_machine *machine = make_idle_machine(S32K_ID);
  const uint32_t program[3] = {OP_LUI_T0_PI, OP_ADDIU_T1_7, OP_SW_T1_RD_LEN};
  uint8_t zero[16] = {0};
  size_t index;
  t64_status status;

  if (machine == NULL) return;
  for (index = 0u; index < 3u; index++) {
    put_be32(machine->ram + STORE_PROGRAM_ADDRESS + 4u * index, program[index]);
  }
  (void)memcpy(machine->ram + DMA_DRAM, dma_pattern, 8u);
  T64_CHECK_INT(bus_w32(machine, PI_DRAM, DMA_DRAM), T64_STATUS_OK);
  T64_CHECK_INT(bus_w32(machine, PI_CART, DMA_CART), T64_STATUS_OK);
  machine->cpu.pc = UINT32_C(0x80001000);
  machine->cpu.pc_full = t64_cpu_canonical_address(machine->cpu.pc);
  for (index = 0u; index < 2u; index++) {
    T64_CHECK_INT(t64_cpu_step(machine, NULL), T64_STATUS_OK);
  }
  T64_CHECK_INT(machine->cpu.retired_instructions, 2u);
  /* Leave only well-formed events: cancel the CPU successors the manual steps
     queued, then fill the queue with far-future synthetic events so exactly
     `room` slots stay free (N6: the capacity edit is on the in-memory copy of
     the profile after create). */
  {
    uint64_t cancelled = 0u;

    while (t64_schedule_cancel(machine, T64_DEVICE_CPU, T64_CPU_EVENT_STEP, 0u,
                               &cancelled) == T64_STATUS_OK) {
    }
  }
  machine->profile.event_queue_capacity = STORE_QUEUE_LIMIT;
  /* The queue keeps the profile's other devices' events; pad it up to
     exactly `room` free slots. */
  T64_CHECK(machine->queue_count + room <= STORE_QUEUE_LIMIT);
  while (machine->queue_count + room < STORE_QUEUE_LIMIT) {
    T64_CHECK_INT(t64_test_schedule_synthetic(machine, UINT64_C(100000),
                                              T64_TEST_EVENT_NOOP, 0u),
                  T64_STATUS_OK);
  }
  T64_CHECK_INT(machine->queue_count, STORE_QUEUE_LIMIT - room);
  status = t64_cpu_step(machine, NULL);
  if (room < 2u) {
    T64_CHECK_INT(status, T64_STATUS_QUEUE_FULL);
    T64_CHECK_INT(machine->cpu.retired_instructions, 2u);
    T64_CHECK(!machine->pi.active_valid);
    T64_CHECK(storage_is(machine, SRAM_OFFSET, zero, 8u));
  } else {
    uint64_t completion = 0u;

    T64_CHECK_INT(status, T64_STATUS_OK);
    T64_CHECK_INT(machine->cpu.retired_instructions, 3u);
    T64_CHECK(machine->pi.active_valid);
    T64_CHECK_INT(machine->pi.active_save_direction,
                  T64_PI_DIRECTION_RDRAM_TO_SRAM);
    T64_CHECK(storage_is(machine, SRAM_OFFSET, zero, 8u));
    T64_CHECK(pi_event_tick(machine, &completion));
    if (completion != 0u) {
      T64_CHECK(advance_guest_to(machine, completion - 1u));
      T64_CHECK(storage_is(machine, SRAM_OFFSET, zero, 8u));
      T64_CHECK(advance_guest_to(machine, completion));
      T64_CHECK(storage_is(machine, SRAM_OFFSET, dma_pattern, 8u));
    }
  }
  t64_machine_destroy(machine);
}

static void case_sram_dma(void) {
  t64_machine *machine = make_idle_machine(S32K_ID);
  size_t index;

  if (machine != NULL) {
    /* Write: RDRAM pattern to the SRAM at cart 0x08000100; N 16 so the
       completion is start + 1 + 16/2 = 9. */
    (void)memcpy(machine->ram + DMA_DRAM, dma_pattern, sizeof dma_pattern);
    T64_CHECK(advance_guest_to(machine, 1000u));
    T64_CHECK_INT(pi_start(machine, DMA_DRAM, DMA_CART, PI_RD_LEN, 15u),
                  T64_STATUS_OK);
    T64_CHECK_INT(bus_value(machine, PI_STATUS), 1u);
    T64_CHECK(advance_guest_to(machine, 1000u + 8u));
    T64_CHECK_INT(bus_value(machine, PI_STATUS), 1u);
    T64_CHECK(storage_is(machine, SRAM_OFFSET, zero_block, 16u));
    T64_CHECK_INT(machine->mi_pending & MI_PI_BIT, 0u);
    /* A source byte changed after the start and before the completion reaches
       the storage: the effect is atomic at the completion event. */
    machine->ram[DMA_DRAM] = 0xA5u;
    T64_CHECK(advance_guest_to(machine, 1000u + 9u));
    T64_CHECK_INT(bus_value(machine, PI_STATUS), 0u);
    T64_CHECK_INT(machine->save.bytes != NULL ? machine->save.bytes[SRAM_OFFSET]
                                              : 0xFFu,
                  0xA5u);
    T64_CHECK(storage_is(machine, SRAM_OFFSET + 1u, dma_pattern + 1, 15u));
    T64_CHECK_INT(machine->mi_pending & MI_PI_BIT, MI_PI_BIT);
    T64_CHECK_INT(bus_w32(machine, PI_STATUS, 2u), T64_STATUS_OK);
    T64_CHECK_INT(machine->mi_pending & MI_PI_BIT, 0u);

    /* Read: the same cart address back to RDRAM with the write-length
       register, effect at the completion event. */
    (void)memset(machine->ram + DMA_BACK_DRAM, 0, 16u);
    T64_CHECK_INT(pi_start(machine, DMA_BACK_DRAM, DMA_CART, PI_WR_LEN, 15u),
                  T64_STATUS_OK);
    T64_CHECK(advance_guest_to(machine, 1000u + 9u + 8u));
    T64_CHECK(memcmp(machine->ram + DMA_BACK_DRAM, zero_block, 16u) == 0);
    T64_CHECK(advance_guest_to(machine, 1000u + 9u + 9u));
    T64_CHECK_INT(machine->ram[DMA_BACK_DRAM], 0xA5u);
    T64_CHECK(memcmp(machine->ram + DMA_BACK_DRAM + 1u, dma_pattern + 1,
                     15u) == 0);
    T64_CHECK_INT(bus_value(machine, PI_STATUS), 0u);
    T64_CHECK_INT(bus_w32(machine, PI_STATUS, 2u), T64_STATUS_OK);

    /* Durations from literals: N 2 is 1 + 1 = 2, N 16 is 9 (above), N 32,768
       is 1 + 16,384 = 16,385. */
    {
      uint64_t now = machine->tick;

      T64_CHECK_INT(pi_start(machine, DMA_DRAM, DMA_CART, PI_RD_LEN, 1u),
                    T64_STATUS_OK);
      T64_CHECK(advance_guest_to(machine, now + 1u));
      T64_CHECK_INT(bus_value(machine, PI_STATUS), 1u);
      T64_CHECK(advance_guest_to(machine, now + 2u));
      T64_CHECK_INT(bus_value(machine, PI_STATUS), 0u);
      T64_CHECK_INT(bus_w32(machine, PI_STATUS, 2u), T64_STATUS_OK);
      now = machine->tick;
      T64_CHECK_INT(pi_start(machine, UINT32_C(0x10000), UINT32_C(0x08000000),
                             PI_RD_LEN, 32767u),
                    T64_STATUS_OK);
      T64_CHECK(advance_guest_to(machine, now + 16384u));
      T64_CHECK_INT(bus_value(machine, PI_STATUS), 1u);
      T64_CHECK(advance_guest_to(machine, now + 16385u));
      T64_CHECK_INT(bus_value(machine, PI_STATUS), 0u);
      T64_CHECK_INT(bus_w32(machine, PI_STATUS, 2u), T64_STATUS_OK);
    }

    /* A second start while busy: sticky error set (status bit 2), the first
       DMA completes unchanged. */
    {
      uint64_t now = machine->tick;
      uint8_t snapshot[16];

      (void)memcpy(machine->ram + DMA_DRAM, dma_pattern, sizeof dma_pattern);
      T64_CHECK_INT(pi_start(machine, DMA_DRAM, UINT32_C(0x08000200),
                             PI_RD_LEN, 15u),
                    T64_STATUS_OK);
      T64_CHECK_INT(pi_start(machine, DMA_DRAM, UINT32_C(0x08000300),
                             PI_RD_LEN, 15u),
                    T64_STATUS_BUS_DEVICE_REFUSED);
      T64_CHECK_INT(bus_value(machine, PI_STATUS), 5u);
      T64_CHECK(advance_guest_to(machine, now + 9u));
      T64_CHECK(storage_is(machine, 0x200u, dma_pattern, 16u));
      T64_CHECK(storage_is(machine, 0x300u, zero_block, 16u));
      (void)memcpy(snapshot, machine->save.bytes + 0x200u, sizeof snapshot);
      T64_CHECK_INT(bus_w32(machine, PI_STATUS, 2u), T64_STATUS_OK);
    }

    /* A controller reset (status write 1) before the completion cancels the
       effect and leaves the storage unchanged. */
    {
      uint64_t now = machine->tick;

      machine->ram[DMA_DRAM] = 0x7Eu;
      T64_CHECK_INT(pi_start(machine, DMA_DRAM, UINT32_C(0x08000400),
                             PI_RD_LEN, 15u),
                    T64_STATUS_OK);
      T64_CHECK_INT(bus_w32(machine, PI_STATUS, 1u), T64_STATUS_OK);
      T64_CHECK(advance_guest_to(machine, now + 9u));
      T64_CHECK(storage_is(machine, 0x400u, zero_block, 16u));
      T64_CHECK_INT(bus_value(machine, PI_STATUS) & 1u, 0u);
      T64_CHECK_INT(bus_w32(machine, PI_STATUS, 2u), T64_STATUS_OK);
    }
    t64_machine_destroy(machine);
  }

  dma_refusal_rows();
  dma_cpu_store_row(1u);
  dma_cpu_store_row(2u);

  /* Resource refusals at their direct-state setups: the status is exact, the
     storage is unchanged, and the digest equals the pre-state once the
     documented read-length latch is restored through direct state (an
     accepted baseline runs first in each setup). */
  for (index = 0u; index < 3u; index++) {
    t64_machine *other = make_idle_machine(S32K_ID);
    uint8_t before[T64_DIGEST_SIZE];
    uint8_t after[T64_DIGEST_SIZE];
    uint8_t snapshot[T64_SAVE_SRAM_BYTES];
    uint32_t latch;
    t64_status want;
    t64_status status;
    uint32_t slot;

    if (other == NULL) continue;
    sram_write_pattern(other);
    T64_CHECK_INT(bus_w32(other, PI_DRAM, DMA_DRAM), T64_STATUS_OK);
    T64_CHECK_INT(bus_w32(other, PI_CART, DMA_CART), T64_STATUS_OK);
    (void)memcpy(snapshot, other->save.bytes, sizeof snapshot);
    latch = other->pi.read_length;
    if (index == 0u) {
      for (slot = 0u; slot < other->profile.event_queue_capacity; slot++) {
        T64_CHECK_INT(t64_test_schedule_synthetic(other, UINT64_C(100000),
                                                  T64_TEST_EVENT_NOOP, 0u),
                      T64_STATUS_OK);
      }
      want = T64_STATUS_QUEUE_FULL;
    } else if (index == 1u) {
      other->sequence_exhausted = true;
      want = T64_STATUS_SEQUENCE_EXHAUSTED;
    } else {
      other->tick = UINT64_MAX - UINT64_C(8);
      want = T64_STATUS_TICK_OVERFLOW;
    }
    digest_of(other, before);
    status = bus_w32(other, PI_RD_LEN, 15u);
    T64_CHECK_INT(status, want);
    other->pi.read_length = latch;
    digest_of(other, after);
    T64_CHECK(memcmp(before, after, T64_DIGEST_SIZE) == 0);
    T64_CHECK(memcmp(snapshot, other->save.bytes, sizeof snapshot) == 0);
    T64_CHECK_INT(bus_value(other, PI_STATUS) & 1u, 0u);
    t64_machine_destroy(other);
  }

  /* Profiles without the SRAM keep latching and refusing the read-length
     start, keep refusing a write-length start with an SRAM cart address, and
     the -s32k id still copies cartridge bytes for ROM DMA (these rows start
     green by design: the new path must not reach them). */
  {
    static const char *const others[] = {STEM_ID, E4K_ID, E16K_ID, AI_ID,
                                         LEGACY_ID};

    for (index = 0u; index < sizeof others / sizeof others[0]; index++) {
      t64_machine *other = make_idle_machine(others[index]);
      uint8_t before[T64_DIGEST_SIZE];
      uint8_t after[T64_DIGEST_SIZE];

      if (other == NULL) continue;
      T64_CHECK_INT(bus_w32(other, PI_DRAM, DMA_DRAM), T64_STATUS_OK);
      T64_CHECK_INT(bus_w32(other, PI_CART, DMA_CART), T64_STATUS_OK);
      T64_CHECK_INT(bus_w32(other, PI_RD_LEN, 15u),
                    T64_STATUS_BUS_DEVICE_REFUSED);
      T64_CHECK_INT(bus_value(other, PI_RD_LEN), 15u);
      T64_CHECK_INT(bus_value(other, PI_STATUS) & 1u, 0u);
      digest_of(other, before);
      T64_CHECK_INT(bus_w32(other, PI_WR_LEN, 15u),
                    T64_STATUS_BUS_DEVICE_REFUSED);
      other->pi.write_length = 0u;
      digest_of(other, after);
      T64_CHECK(memcmp(before, after, T64_DIGEST_SIZE) == 0);
      T64_CHECK_INT(bus_value(other, PI_STATUS) & 1u, 0u);
      t64_machine_destroy(other);
    }
  }
  {
    t64_machine *other = make_idle_machine(S32K_ID);

    if (other != NULL) {
      const uint64_t start = other->tick;

      /* ROM DMA on the -s32k id: the first eight image bytes to RDRAM. */
      (void)memset(other->ram + DMA_BACK_DRAM, 0, 16u);
      T64_CHECK_INT(pi_start(other, DMA_BACK_DRAM, ROM_CART_BASE, PI_WR_LEN,
                             7u),
                    T64_STATUS_OK);
      T64_CHECK(advance_guest_to(other, start + 5u));
      T64_CHECK(other->image_storage.data != NULL &&
                memcmp(other->ram + DMA_BACK_DRAM, other->image_storage.data,
                       8u) == 0);
      T64_CHECK(other->save.bytes != NULL &&
                storage_is(other, 0u, zero_block, 16u));
      t64_machine_destroy(other);
    }
  }
}

/* ------------------------------------------------------------------------ */
/* Channel 4 matrices: shared helpers                                         */
/* ------------------------------------------------------------------------ */

/* The handshake total of a single channel 4 entry that is not one of the
 * three exact triples (timeout on an EEPROM id, absent elsewhere):
 * 2000 n + 188 + 3907. */
#define TIMEOUT_BASE UINT64_C(4095)
#define BYTE_COST UINT64_C(2000)

static uint64_t unanswered_handshake(uint32_t tx_count) {
  return BYTE_COST * tx_count + TIMEOUT_BASE;
}

/* One frame through the SI at the current tick: WR64B, then an RD64B started
 * right after it settles; the execute and completion ticks are asserted
 * against `handshake` inside read_image. */
static void run_txn(t64_machine *machine, const uint8_t frame[64],
                    uint64_t handshake, uint8_t image[64]) {
  write_frame(machine, frame);
  read_image(machine, machine->tick, handshake, image);
}

static const uint8_t *rx_at(const uint8_t image[64], uint32_t tx_count) {
  return image + 6u + tx_count;
}

static bool rx_all(const uint8_t image[64], uint32_t tx_count,
                   uint32_t rx_count, uint8_t value) {
  uint32_t index;

  for (index = 0u; index < rx_count; index++) {
    if (rx_at(image, tx_count)[index] != value) return false;
  }
  return true;
}

/* ------------------------------------------------------------------------ */
/* eeprom-commands                                                           */
/* ------------------------------------------------------------------------ */

typedef struct {
  const char *label;
  uint32_t tx_count;
  uint32_t rx_count;
  uint8_t tx[10];
} command_row;

/* Frames that are not one of the exact triples: command 0xFF, the 0x01 and
 * 0x02 commands, the RTC commands (0x06, 0x07 and 0x08 with their own
 * lengths) and every wrong length of the three valid commands. */
static const command_row unanswered_rows[] = {
    {"command 0xff", 1u, 3u, {0xFF}},
    {"command 0x01", 1u, 3u, {0x01}},
    {"command 0x02", 1u, 3u, {0x02}},
    {"rtc status 0x06", 1u, 3u, {0x06}},
    {"rtc read 0x07", 2u, 9u, {0x07, 0x00}},
    {"rtc write 0x08", 10u, 1u,
     {0x08, 0x00, 1, 2, 3, 4, 5, 6, 7, 8}},
    {"info with rx 4", 1u, 4u, {0x00}},
    {"info with rx 0", 1u, 0u, {0x00}},
    {"info with tx 2", 2u, 3u, {0x00, 0x00}},
    {"read with tx 3", 3u, 8u, {0x04, 0x01, 0x00}},
    {"read with rx 7", 2u, 7u, {0x04, 0x01}},
    {"write with tx 9", 9u, 1u, {0x05, 0x01, 1, 2, 3, 4, 5, 6, 7}},
    {"write with rx 2", 10u, 2u, {0x05, 0x01, 1, 2, 3, 4, 5, 6, 7, 8}},
};

static const uint8_t block_a[8] = {0xA1, 0xA2, 0xA3, 0xA4,
                                   0xA5, 0xA6, 0xA7, 0xA8};

static void check_unanswered_rows(t64_machine *machine, uint8_t flag) {
  size_t index;

  for (index = 0u; index < sizeof unanswered_rows / sizeof unanswered_rows[0];
       index++) {
    const command_row *row = &unanswered_rows[index];
    uint8_t frame[64];
    uint8_t image[64];
    uint8_t snapshot[T64_SAVE_SRAM_BYTES];
    const uint64_t deadline = machine->save.busy_until;
    const size_t size = machine->save.bytes == NULL
                            ? 0u
                            : t64_save_storage_bytes(machine->profile.save_kind);

    if (size != 0u) (void)memcpy(snapshot, machine->save.bytes, size);
    build_frame(frame, row->tx_count, row->rx_count, row->tx);
    run_txn(machine, frame, unanswered_handshake(row->tx_count), image);
    if (image[5] != (uint8_t)(row->rx_count | flag)) {
      fprintf(stderr, "command row '%s': byte 5 is %02x\n", row->label,
              (unsigned)image[5]);
    }
    T64_CHECK_INT(image[4], row->tx_count);
    T64_CHECK_INT(image[5], row->rx_count | flag);
    T64_CHECK(rx_all(image, row->tx_count, row->rx_count, 0xFFu));
    T64_CHECK_INT(machine->save.busy_until, deadline);
    if (size != 0u) {
      T64_CHECK(memcmp(snapshot, machine->save.bytes, size) == 0);
    }
  }
}

static void case_eeprom_commands(void) {
  static const char *const eeprom_ids[] = {E4K_ID, E16K_ID};
  static const char *const no_eeprom_ids[] = {STEM_ID, S32K_ID};
  uint8_t frame[64];
  uint8_t image[64];
  size_t index;

  for (index = 0u; index < 2u; index++) {
    t64_machine *machine = make_idle_machine(eeprom_ids[index]);
    const uint8_t id_byte = index == 0u ? 0x80u : 0xC0u;
    static const uint8_t info_tx[1] = {0x00};
    static const uint8_t read_tx[2] = {0x04, 0x07};
    static const uint8_t write_tx[10] = {0x05, 0x07, 0xA1, 0xA2, 0xA3,
                                         0xA4, 0xA5, 0xA6, 0xA7, 0xA8};
    uint64_t deadline;

    if (machine == NULL) continue;
    /* Info while idle: 00 id 00 and no flag. */
    build_frame(frame, 1u, 3u, info_tx);
    run_txn(machine, frame, H_INFO, image);
    T64_CHECK_INT(image[5], 0x03u);
    T64_CHECK_INT(rx_at(image, 1u)[0], 0x00u);
    T64_CHECK_INT(rx_at(image, 1u)[1], id_byte);
    T64_CHECK_INT(rx_at(image, 1u)[2], 0x00u);
    /* An unwritten block reads eight 0xFF. */
    build_frame(frame, 2u, 8u, read_tx);
    run_txn(machine, frame, H_READ, image);
    T64_CHECK_INT(image[5], 0x08u);
    T64_CHECK(rx_all(image, 2u, 8u, 0xFFu));
    /* Every frame that is not an exact triple answers the timeout flag (0x40)
       with nothing written and the deadline untouched. */
    check_unanswered_rows(machine, 0x40u);
    /* Write then read round-trips eight literal bytes. */
    build_frame(frame, 10u, 1u, write_tx);
    run_txn(machine, frame, H_WRITE, image);
    T64_CHECK_INT(image[5], 0x01u);
    T64_CHECK_INT(rx_at(image, 10u)[0], 0x00u);
    deadline = machine->save.busy_until;
    T64_CHECK(deadline > machine->tick);
    T64_CHECK(advance_guest_to(machine, deadline));
    build_frame(frame, 2u, 8u, read_tx);
    run_txn(machine, frame, H_READ, image);
    T64_CHECK_INT(image[5], 0x08u);
    T64_CHECK(memcmp(rx_at(image, 2u), block_a, 8u) == 0);
    /* TX flag 0x80 on the channel 4 handshake skips the execution: channel 0
       carries a controller state poll (10,876 ticks), channel 4 a flagged
       write; nothing is written, byte 5 keeps its count with no flag. */
    {
      uint8_t flagged[64] = {0};
      const uint64_t before_deadline = machine->save.busy_until;
      uint8_t snapshot[T64_SAVE_EEPROM_16K_BYTES];

      (void)memcpy(snapshot, machine->save.bytes,
                   t64_save_storage_bytes(machine->profile.save_kind));
      flagged[0] = 0x01; flagged[1] = 0x04; flagged[2] = 0x01;
      flagged[3] = 0xFF; flagged[4] = 0xFF; flagged[5] = 0xFF;
      flagged[6] = 0xFF;
      flagged[7] = 0x00; flagged[8] = 0x00; flagged[9] = 0x00;
      flagged[10] = 0x8A; /* TX flag 0x80 with a transmit count of 10 */
      flagged[11] = 0x01;
      (void)memcpy(flagged + 12, write_tx, 10u);
      flagged[22] = 0xFF;
      flagged[23] = 0xFE;
      flagged[63] = 0x01;
      run_txn(machine, flagged, UINT64_C(10876), image);
      T64_CHECK_INT(image[11], 0x01u);
      T64_CHECK_INT(image[22], 0xFFu);
      T64_CHECK_INT(machine->save.busy_until, before_deadline);
      T64_CHECK(memcmp(snapshot, machine->save.bytes,
                       t64_save_storage_bytes(machine->profile.save_kind)) ==
                0);
    }
    /* A valid triple never has a zero receive count: it is classified as a
       timeout (the independent class rule). */
    T64_CHECK_INT(t64_save_channel4_classify(machine, 0x00, 1u, 0u),
                  T64_SAVE_CH4_TIMEOUT);
    T64_CHECK_INT(t64_save_channel4_classify(machine, 0x04, 2u, 8u),
                  T64_SAVE_CH4_VALID);
    t64_machine_destroy(machine);
  }

  /* On the stem and the -s32k ids every one of these frames, including the
     three valid triples, answers the no-device flag (0x80) with the data
     untouched and the storage unchanged. */
  for (index = 0u; index < 2u; index++) {
    t64_machine *machine = make_idle_machine(no_eeprom_ids[index]);
    static const uint8_t info_tx[1] = {0x00};
    static const uint8_t read_tx[2] = {0x04, 0x07};
    static const uint8_t write_tx[10] = {0x05, 0x07, 0xA1, 0xA2, 0xA3,
                                         0xA4, 0xA5, 0xA6, 0xA7, 0xA8};
    uint8_t snapshot[T64_SAVE_SRAM_BYTES];
    const size_t size = machine == NULL || machine->save.bytes == NULL
                            ? 0u
                            : t64_save_storage_bytes(machine->profile.save_kind);

    if (machine == NULL) continue;
    if (size != 0u) (void)memcpy(snapshot, machine->save.bytes, size);
    build_frame(frame, 1u, 3u, info_tx);
    run_txn(machine, frame, unanswered_handshake(1u), image);
    T64_CHECK_INT(image[5], 0x83u);
    T64_CHECK(rx_all(image, 1u, 3u, 0xFFu));
    build_frame(frame, 2u, 8u, read_tx);
    run_txn(machine, frame, unanswered_handshake(2u), image);
    T64_CHECK_INT(image[5], 0x88u);
    T64_CHECK(rx_all(image, 2u, 8u, 0xFFu));
    build_frame(frame, 10u, 1u, write_tx);
    run_txn(machine, frame, unanswered_handshake(10u), image);
    T64_CHECK_INT(image[5], 0x81u);
    T64_CHECK_INT(rx_at(image, 10u)[0], 0xFFu);
    check_unanswered_rows(machine, 0x80u);
    T64_CHECK_INT(machine->save.busy_until, 0u);
    if (size != 0u) {
      T64_CHECK(memcmp(snapshot, machine->save.bytes, size) == 0);
    }
    T64_CHECK_INT(t64_save_channel4_classify(machine, 0x05, 10u, 1u),
                  T64_SAVE_CH4_ABSENT);
    t64_machine_destroy(machine);
  }
}

/* ------------------------------------------------------------------------ */
/* eeprom-blocks                                                             */
/* ------------------------------------------------------------------------ */

/* The pattern of block index i: i, i xor 0xFF, then i + 1 to i + 6, so every
 * index embeds itself and a wrong mask shows at the alias rows. */
static void block_pattern(unsigned index, uint8_t out[8]) {
  unsigned k;

  out[0] = (uint8_t)index;
  out[1] = (uint8_t)(index ^ 0xFFu);
  for (k = 0u; k < 6u; k++) out[2u + k] = (uint8_t)(index + 1u + k);
}

/* Direct calls of the execute function at increasing ticks; the deadline is
 * cleared between writes through direct state (a test shortcut: timed
 * behavior is eeprom-busy). */
static uint8_t direct_write(t64_machine *machine, uint64_t tick,
                            unsigned index, const uint8_t data[8]) {
  uint8_t tx[10];
  uint8_t rx[1] = {0xFF};
  uint8_t flags;

  tx[0] = 0x05;
  tx[1] = (uint8_t)index;
  (void)memcpy(tx + 2, data, 8u);
  machine->save.busy_until = 0u;
  flags = t64_save_channel4_execute(machine, tick, tx, 10u, rx, 1u);
  T64_CHECK_INT(flags, 0u);
  T64_CHECK_INT(rx[0], 0x00u);
  return flags;
}

static void direct_read(t64_machine *machine, uint64_t tick, unsigned index,
                        uint8_t out[8]) {
  uint8_t tx[2] = {0x04, 0};
  uint8_t flags;

  tx[1] = (uint8_t)index;
  machine->save.busy_until = 0u;
  flags = t64_save_channel4_execute(machine, tick, tx, 2u, out, 8u);
  T64_CHECK_INT(flags, 0u);
}

static void case_eeprom_blocks(void) {
  unsigned index;

  /* 4 Kbit: 64 blocks, indexes 64 to 255 mirror onto index and 0x3F. The
     alias rows run first. */
  {
    t64_machine *machine = make_idle_machine(E4K_ID);
    uint8_t pattern[8];
    uint8_t out[8];
    uint8_t before[T64_SAVE_EEPROM_4K_BYTES];
    static const unsigned aliases[] = {0x01u, 0x41u, 0x81u, 0xC1u};
    static const unsigned aliases_3f[] = {0x3Fu, 0x7Fu, 0xBFu, 0xFFu};
    size_t k;

    if (machine == NULL) return;
    T64_CHECK_INT(t64_save_storage_bytes(machine->profile.save_kind), 512u);
    block_pattern(0x41u, pattern);
    direct_write(machine, 1000u, 0x41u, pattern);
    for (k = 0u; k < 4u; k++) {
      direct_read(machine, 2000u, aliases[k], out);
      T64_CHECK(memcmp(out, pattern, 8u) == 0);
    }
    block_pattern(0xFFu, pattern);
    (void)memcpy(before, machine->save.bytes, sizeof before);
    direct_write(machine, 3000u, 0xFFu, pattern);
    for (k = 0u; k < 4u; k++) {
      direct_read(machine, 4000u, aliases_3f[k], out);
      T64_CHECK(memcmp(out, pattern, 8u) == 0);
    }
    /* A write to block 0xFF changes only storage offset 0x1F8 to 0x1FF. */
    for (index = 0u; index < sizeof before; index++) {
      const bool inside = index >= 0x1F8u && index <= 0x1FFu;

      if (inside) {
        T64_CHECK_INT(machine->save.bytes[index], pattern[index - 0x1F8u]);
      } else {
        T64_CHECK_INT(machine->save.bytes[index], before[index]);
      }
    }
    /* Indexes 0 to 63 read back their own pattern; 64 to 255 read the
       pattern of the index and 0x3F. */
    (void)memset(machine->save.bytes, 0xFF, T64_SAVE_EEPROM_4K_BYTES);
    for (index = 0u; index < 64u; index++) {
      block_pattern(index, pattern);
      direct_write(machine, 5000u + index, index, pattern);
    }
    for (index = 0u; index < 256u; index++) {
      block_pattern(index & 0x3Fu, pattern);
      direct_read(machine, 6000u + index, index, out);
      if (memcmp(out, pattern, 8u) != 0) {
        fprintf(stderr, "4 Kbit block %u reads the wrong pattern\n", index);
      }
      T64_CHECK(memcmp(out, pattern, 8u) == 0);
    }
    t64_machine_destroy(machine);
  }

  /* 16 Kbit: all 256 blocks are distinct. */
  {
    t64_machine *machine = make_idle_machine(E16K_ID);
    uint8_t pattern[8];
    uint8_t out[8];

    if (machine == NULL) return;
    T64_CHECK_INT(t64_save_storage_bytes(machine->profile.save_kind), 2048u);
    for (index = 0u; index < 256u; index++) {
      block_pattern(index, pattern);
      direct_write(machine, 1000u + index, index, pattern);
    }
    for (index = 0u; index < 256u; index++) {
      block_pattern(index, pattern);
      direct_read(machine, 5000u + index, index, out);
      if (memcmp(out, pattern, 8u) != 0) {
        fprintf(stderr, "16 Kbit block %u reads the wrong pattern\n", index);
      }
      T64_CHECK(memcmp(out, pattern, 8u) == 0);
      T64_CHECK(memcmp(machine->save.bytes + 8u * index, pattern, 8u) == 0);
    }
    t64_machine_destroy(machine);
  }
}

/* ------------------------------------------------------------------------ */
/* eeprom-busy                                                               */
/* ------------------------------------------------------------------------ */

typedef enum { PROBE_INFO, PROBE_READ, PROBE_WRITE } probe_kind;

static const uint8_t busy_write_tx[10] = {0x05, 0x01, 0x11, 0x12, 0x13,
                                          0x14, 0x15, 0x16, 0x17, 0x18};
static const uint8_t busy_other_tx[10] = {0x05, 0x02, 0x21, 0x22, 0x23,
                                          0x24, 0x25, 0x26, 0x27, 0x28};

/* A fresh e4k machine: block 1 written with an execute tick E1 = 10,000 +
 * 22,876 = 32,876 (the deadline D = 407,876), then one probe whose execute
 * tick is D + delta; returns the image and leaves the machine for checks. */
static t64_machine *busy_probe(probe_kind kind, int64_t delta,
                               uint8_t image[64]) {
  static const uint8_t info_tx[1] = {0x00};
  static const uint8_t read_tx[2] = {0x04, 0x01};
  t64_machine *machine = make_idle_machine(E4K_ID);
  uint8_t frame[64];
  uint8_t ignored[64];
  const uint64_t deadline = UINT64_C(10000) + H_WRITE + BUSY_WINDOW;
  uint64_t handshake;

  if (machine == NULL) return NULL;
  T64_CHECK(advance_guest_to(machine, 3000u));
  build_frame(frame, 10u, 1u, busy_write_tx);
  write_frame(machine, frame);
  read_image(machine, 10000u, H_WRITE, ignored);
  T64_CHECK_INT(machine->save.busy_until, deadline);
  if (kind == PROBE_INFO) {
    build_frame(frame, 1u, 3u, info_tx);
    handshake = H_INFO;
  } else if (kind == PROBE_READ) {
    build_frame(frame, 2u, 8u, read_tx);
    handshake = H_READ;
  } else {
    build_frame(frame, 10u, 1u, busy_other_tx);
    handshake = H_WRITE;
  }
  write_frame(machine, frame);
  read_image(machine, (uint64_t)((int64_t)(deadline - handshake) + delta),
             handshake, image);
  return machine;
}

static void case_eeprom_busy(void) {
  const uint64_t deadline = UINT64_C(10000) + H_WRITE + BUSY_WINDOW;
  uint8_t image[64];
  t64_machine *machine;

  T64_CHECK_INT(deadline, 407876u);
  /* Info: busy status 0x80 one tick before the deadline, idle at the
     deadline and after; a stale deadline is kept as it is. */
  machine = busy_probe(PROBE_INFO, -1, image);
  if (machine != NULL) {
    T64_CHECK_INT(image[5], 0x03u);
    T64_CHECK_INT(rx_at(image, 1u)[2], 0x80u);
    T64_CHECK_INT(machine->save.busy_until, deadline);
    t64_machine_destroy(machine);
  }
  machine = busy_probe(PROBE_INFO, 0, image);
  if (machine != NULL) {
    T64_CHECK_INT(rx_at(image, 1u)[2], 0x00u);
    T64_CHECK_INT(machine->save.busy_until, deadline);
    t64_machine_destroy(machine);
  }
  machine = busy_probe(PROBE_INFO, 1000, image);
  if (machine != NULL) {
    T64_CHECK_INT(rx_at(image, 1u)[2], 0x00u);
    T64_CHECK_INT(machine->save.busy_until, deadline); /* stale, kept */
    t64_machine_destroy(machine);
  }

  /* Read: the timeout flag with no data one tick before; the block at the
     deadline. */
  machine = busy_probe(PROBE_READ, -1, image);
  if (machine != NULL) {
    T64_CHECK_INT(image[5], 0x48u);
    T64_CHECK(rx_all(image, 2u, 8u, 0xFFu));
    t64_machine_destroy(machine);
  }
  machine = busy_probe(PROBE_READ, 0, image);
  if (machine != NULL) {
    T64_CHECK_INT(image[5], 0x08u);
    T64_CHECK_INT(rx_at(image, 2u)[0], 0x11u);
    T64_CHECK_INT(rx_at(image, 2u)[7], 0x18u);
    t64_machine_destroy(machine);
  }

  /* Write: already busy (0x80) one tick before, nothing written, the window
     not extended; accepted at the deadline with a new deadline from its own
     execute tick. */
  machine = busy_probe(PROBE_WRITE, -1, image);
  if (machine != NULL) {
    T64_CHECK_INT(image[5], 0x01u);
    T64_CHECK_INT(rx_at(image, 10u)[0], 0x80u);
    T64_CHECK_INT(machine->save.busy_until, deadline);
    T64_CHECK(machine->save.bytes != NULL &&
              memcmp(machine->save.bytes + 16u, "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF",
                     8u) == 0);
    t64_machine_destroy(machine);
  }
  machine = busy_probe(PROBE_WRITE, 0, image);
  if (machine != NULL) {
    T64_CHECK_INT(image[5], 0x01u);
    T64_CHECK_INT(rx_at(image, 10u)[0], 0x00u);
    T64_CHECK_INT(machine->save.busy_until, deadline + BUSY_WINDOW);
    T64_CHECK(machine->save.bytes != NULL &&
              memcmp(machine->save.bytes + 16u, busy_other_tx + 2u, 8u) == 0);
    t64_machine_destroy(machine);
  }

  /* A deadline that would overflow 64 bits is refused with the timeout flag:
     nothing written, the deadline untouched. */
  machine = make_idle_machine(E4K_ID);
  if (machine != NULL) {
    uint8_t rx[1] = {0xFF};
    uint8_t flags = t64_save_channel4_execute(
        machine, UINT64_MAX - 100u, busy_write_tx, 10u, rx, 1u);

    T64_CHECK_INT(flags, 0x40u);
    T64_CHECK_INT(rx[0], 0xFFu);
    T64_CHECK_INT(machine->save.busy_until, 0u);
    T64_CHECK(machine->save.bytes != NULL &&
              memcmp(machine->save.bytes + 8u, "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF",
                     8u) == 0);
    t64_machine_destroy(machine);
  }

  /* A second machine of the same profile is not affected (no shared state). */
  {
    t64_machine *first = busy_probe(PROBE_INFO, 0, image);
    t64_machine *second = make_idle_machine(E4K_ID);

    if (first != NULL && second != NULL) {
      T64_CHECK_INT(first->save.busy_until, deadline);
      T64_CHECK_INT(second->save.busy_until, 0u);
      T64_CHECK(second->save.bytes != NULL && first->save.bytes != NULL &&
                second->save.bytes != first->save.bytes);
      T64_CHECK_INT(second->save.bytes[8], 0xFFu);
      T64_CHECK_INT(first->save.bytes[8], 0x11u);
    }
    if (first != NULL) t64_machine_destroy(first);
    if (second != NULL) t64_machine_destroy(second);
  }
}

/* ------------------------------------------------------------------------ */
/* eeprom-timing                                                             */
/* ------------------------------------------------------------------------ */

/* The comparator of the execute tick: exact equality. */
static bool tick_matches(uint64_t expected, uint64_t actual) {
  return expected == actual;
}

typedef struct {
  const char *label;
  const char *profile_id;
  uint32_t tx_count;
  uint32_t rx_count;
  uint8_t tx[10];
  uint64_t handshake; /* independent literal */
} timing_row;

static const timing_row timing_rows[] = {
    {"info", E4K_ID, 1u, 3u, {0x00}, 8876u},
    {"read", E4K_ID, 2u, 8u, {0x04, 0x01}, 20876u},
    {"write", E4K_ID, 10u, 1u, {0x05, 0x01, 1, 2, 3, 4, 5, 6, 7, 8}, 22876u},
    {"read with tx 3 rx 8", E4K_ID, 3u, 8u, {0x04, 0x01, 0x00}, 10095u},
    {"command 0xff tx 1", E4K_ID, 1u, 3u, {0xFF}, 6095u},
    {"rtc command 0x06 tx 1 rx 3", E4K_ID, 1u, 3u, {0x06}, 6095u},
    {"write with rx 2", E4K_ID, 10u, 2u,
     {0x05, 0x01, 1, 2, 3, 4, 5, 6, 7, 8}, 24095u},
    {"info with rx 4", E4K_ID, 1u, 4u, {0x00}, 6095u},
    {"info on the stem id (absent)", STEM_ID, 1u, 3u, {0x00}, 6095u},
};

static void case_eeprom_timing(void) {
  size_t index;

  for (index = 0u; index < sizeof timing_rows / sizeof timing_rows[0];
       index++) {
    const timing_row *row = &timing_rows[index];
    t64_machine *machine = make_idle_machine(row->profile_id);
    uint8_t frame[64];
    uint64_t start;

    if (machine == NULL) continue;
    build_frame(frame, row->tx_count, row->rx_count, row->tx);
    write_frame(machine, frame);
    start = machine->tick;
    T64_CHECK_INT(bus_w32(machine, SI_DRAM_ADDR, RECEIVE_DRAM), T64_STATUS_OK);
    T64_CHECK_INT(bus_w32(machine, SI_RD64B, PIF_ADDRESS), T64_STATUS_OK);
    /* C = S + H + 4,096 and E = S + H, from the independent literal. */
    T64_CHECK(tick_matches(start + row->handshake, machine->si.execute_tick));
    T64_CHECK(tick_matches(start + row->handshake + READ_TAIL,
                           machine->si.complete_tick));
    if (!tick_matches(start + row->handshake, machine->si.execute_tick)) {
      fprintf(stderr, "timing row '%s': execute tick %llu, start %llu\n",
              row->label, (unsigned long long)machine->si.execute_tick,
              (unsigned long long)start);
    }
    /* The comparator rejects a deliberately wrong expectation. */
    T64_CHECK(!tick_matches(start + row->handshake + 1u,
                            machine->si.execute_tick));
    T64_CHECK(advance_guest_to(machine, machine->si.complete_tick));
    /* A write row's deadline witnesses E: D = E + 375,000. */
    if (row->tx_count == 10u && row->rx_count == 1u && row->tx[0] == 0x05) {
      T64_CHECK_INT(machine->save.busy_until,
                    start + row->handshake + BUSY_WINDOW);
    }
    t64_machine_destroy(machine);
  }

  /* The classification does not depend on the busy state: the same read
     frame costs 20,876 when the part is idle and when it is busy. */
  {
    t64_machine *machine = make_idle_machine(E4K_ID);
    static const uint8_t read_tx[2] = {0x04, 0x01};
    uint8_t frame[64];
    uint8_t image[64];
    uint8_t ignored[64];

    if (machine != NULL) {
      build_frame(frame, 2u, 8u, read_tx);
      run_txn(machine, frame, UINT64_C(20876), image); /* idle */
      T64_CHECK_INT(image[5], 0x08u);
      build_frame(frame, 10u, 1u, busy_write_tx);
      run_txn(machine, frame, UINT64_C(22876), ignored);
      T64_CHECK(machine->save.busy_until > machine->tick);
      build_frame(frame, 2u, 8u, read_tx);
      run_txn(machine, frame, UINT64_C(20876), image); /* busy */
      T64_CHECK_INT(image[5], 0x48u);
      t64_machine_destroy(machine);
    }
  }
}

/* ------------------------------------------------------------------------ */
/* selection                                                                 */
/* ------------------------------------------------------------------------ */

typedef struct {
  const char *id;
  uint32_t kind;
  uint32_t size;
  bool eeprom;
} id_row;

/* Independent literals: the kind numbering and byte sizes of the four ids. */
static const id_row id_rows[] = {
    {STEM_ID, 0u, 0u, false},
    {E4K_ID, 1u, 512u, true},
    {E16K_ID, 2u, 2048u, true},
    {S32K_ID, 3u, 32768u, false},
};

static void cancel_cpu_events(t64_machine *machine) {
  uint64_t cancelled = 0u;

  while (t64_schedule_cancel(machine, T64_DEVICE_CPU, T64_CPU_EVENT_STEP, 0u,
                             &cancelled) == T64_STATUS_OK) {
  }
}

/* One info transaction on channel 4 at the current tick: returns byte 5 and
 * the three receive bytes. The handshake is 8,876 where an EEPROM answers and
 * 6,095 (the unanswered cost) elsewhere. */
static void info_transaction(t64_machine *machine, bool eeprom,
                             uint8_t *byte5, uint8_t data[3]) {
  static const uint8_t info_tx[1] = {0x00};
  uint8_t frame[64];
  uint8_t image[64];

  build_frame(frame, 1u, 3u, info_tx);
  run_txn(machine, frame, eeprom ? H_INFO : unanswered_handshake(1u), image);
  *byte5 = image[5];
  (void)memcpy(data, rx_at(image, 1u), 3u);
}

static void check_info_result(const id_row *row, const uint8_t byte5,
                              const uint8_t data[3]) {
  if (row->kind == 1u || row->kind == 2u) {
    T64_CHECK_INT(byte5, 0x03u);
    T64_CHECK_INT(data[0], 0x00u);
    T64_CHECK_INT(data[1], row->kind == 1u ? 0x80u : 0xC0u);
    T64_CHECK_INT(data[2], 0x00u);
  } else {
    /* The stem and the SRAM id have no EEPROM: the no-device flag with the
       fill bytes untouched. */
    T64_CHECK_INT(byte5, 0x83u);
    T64_CHECK_INT(data[0], 0xFFu);
    T64_CHECK_INT(data[1], 0xFFu);
    T64_CHECK_INT(data[2], 0xFFu);
  }
}

/* An authored flat ROM that the openIPL3 profiles admit (the pinned payload at
 * 0x40 to 0xFFF, the z64 magic, an entry point and a load length equal to the
 * appended payload): the cleared 4,096-byte asset with its entry point at
 * 0x08 and load length at 0x10 set and a 64-byte zero-word payload appended.
 * The openIPL3 profiles refuse every other edit of the exact asset (a finding
 * recorded in the plan summary), so header edits start from this image. */
#define AUTHORED_ROM_BYTES (IPL3_ASSET_BYTES + 64u)

static bool make_authored_rom(uint8_t image[AUTHORED_ROM_BYTES]) {
  uint8_t asset[IPL3_ASSET_BYTES];

  if (!read_openipl3_asset(asset)) return false;
  (void)memset(image, 0, AUTHORED_ROM_BYTES);
  (void)memcpy(image, asset, IPL3_ASSET_BYTES);
  image[0x08] = 0x80; image[0x09] = 0x00; image[0x0A] = 0x04; image[0x0B] = 0x00;
  image[0x10] = 0x00; image[0x11] = 0x00; image[0x12] = 0x00; image[0x13] = 64u;
  return true;
}

static void case_selection(void) {
  static const uint8_t version_values[] = {0x00, 0x01, 0x02, 0x03, 0x10, 0xFF};
  size_t index;
  size_t edit;

  for (index = 0u; index < sizeof id_rows / sizeof id_rows[0]; index++) {
    const id_row *row = &id_rows[index];
    t64_machine *machine = make_idle_machine(row->id);
    uint8_t authored[AUTHORED_ROM_BYTES];
    uint8_t byte5;
    uint8_t data[3];
    t64_save_info info;
    uint32_t kind;

    if (machine == NULL) continue;
    T64_CHECK(make_authored_rom(authored));
    /* The four ids report kinds none, 1, 2 and 3 and sizes 0, 512, 2,048 and
       32,768 through the info query. */
    T64_CHECK_INT(machine->profile.save_kind, row->kind);
    for (kind = 1u; kind <= 3u; kind++) {
      (void)memset(&info, 0xA5, sizeof info);
      if (kind == row->kind) {
        T64_CHECK_INT(t64_machine_save_info(machine, kind, &info),
                      T64_STATUS_OK);
        T64_CHECK_INT(info.kind, row->kind);
        T64_CHECK_INT(info.size_bytes, row->size);
        T64_CHECK_INT(info.flags, 0u);
      } else {
        T64_CHECK_INT(t64_machine_save_info(machine, kind, &info),
                      T64_STATUS_SAVE_DEVICE_MISMATCH);
      }
    }
    /* Accepted baseline in the same setup: the unmodified authored image is
       admitted over the machine and answers like the first admission. */
    T64_CHECK_INT(t64_machine_admit_rom(machine, authored, sizeof authored),
                  T64_STATUS_OK);
    cancel_cpu_events(machine);
    info_transaction(machine, row->eeprom, &byte5, data);
    check_info_result(row, byte5, data);

    /* The image never selects the device: header byte 0x3F carrying each
       value, then title, game code and checksum bytes edited, each admitted
       over the baseline machine; the kind, the info answer and the wire
       behavior do not change. */
    for (edit = 0u; edit < sizeof version_values + 3u; edit++) {
      uint8_t edited[AUTHORED_ROM_BYTES];
      t64_status status;
      size_t at;

      (void)memcpy(edited, authored, sizeof edited);
      if (edit < sizeof version_values) {
        edited[0x3F] = version_values[edit];
      } else if (edit == sizeof version_values) {
        for (at = 0x20u; at < 0x34u; at++) edited[at] = 0x5A; /* title */
      } else if (edit == sizeof version_values + 1u) {
        for (at = 0x3Bu; at < 0x3Fu; at++) edited[at] = 0x7A; /* game code */
      } else {
        for (at = 0x14u; at < 0x1Cu; at++) edited[at] ^= 0xFF; /* checksum */
      }
      status = t64_machine_admit_rom(machine, edited, sizeof edited);
      if (status != T64_STATUS_OK) {
        fprintf(stderr, "selection edit %zu: admission status %u\n", edit,
                (unsigned)status);
      }
      T64_CHECK_INT(status, T64_STATUS_OK);
      if (status != T64_STATUS_OK) continue;
      cancel_cpu_events(machine);
      T64_CHECK_INT(machine->profile.save_kind, row->kind);
      (void)memset(&info, 0xA5, sizeof info);
      if (row->kind != 0u) {
        T64_CHECK_INT(t64_machine_save_info(machine, row->kind, &info),
                      T64_STATUS_OK);
        T64_CHECK_INT(info.kind, row->kind);
        T64_CHECK_INT(info.size_bytes, row->size);
      } else {
        T64_CHECK_INT(t64_machine_save_info(machine, 1u, &info),
                      T64_STATUS_SAVE_DEVICE_MISMATCH);
      }
      info_transaction(machine, row->eeprom, &byte5, data);
      check_info_result(row, byte5, data);
    }
    t64_machine_destroy(machine);
  }
}

/* ------------------------------------------------------------------------ */
/* refusals                                                                  */
/* ------------------------------------------------------------------------ */

typedef struct {
  uint8_t digest[T64_DIGEST_SIZE];
  uint64_t deadline;
  size_t queue_head;
  size_t queue_count;
  uint64_t tick;
  uint64_t next_sequence;
  uint8_t storage[T64_SAVE_SRAM_BYTES];
  size_t storage_size;
} machine_snapshot;

static void take_snapshot(const t64_machine *machine, machine_snapshot *out) {
  digest_of(machine, out->digest);
  out->deadline = machine->save.busy_until;
  out->queue_head = machine->queue_head;
  out->queue_count = machine->queue_count;
  out->tick = machine->tick;
  out->next_sequence = machine->next_sequence;
  out->storage_size = machine->save.bytes == NULL
                          ? 0u
                          : t64_save_storage_bytes(machine->profile.save_kind);
  if (out->storage_size != 0u) {
    (void)memcpy(out->storage, machine->save.bytes, out->storage_size);
  }
}

static void check_snapshot(const char *label, const t64_machine *machine,
                           const machine_snapshot *before) {
  machine_snapshot after;

  take_snapshot(machine, &after);
  if (memcmp(before->digest, after.digest, T64_DIGEST_SIZE) != 0) {
    fprintf(stderr, "refusal '%s' changed the digest\n", label);
  }
  T64_CHECK(memcmp(before->digest, after.digest, T64_DIGEST_SIZE) == 0);
  T64_CHECK_INT(after.deadline, before->deadline);
  T64_CHECK_INT(after.queue_head, before->queue_head);
  T64_CHECK_INT(after.queue_count, before->queue_count);
  T64_CHECK_INT(after.tick, before->tick);
  T64_CHECK_INT(after.next_sequence, before->next_sequence);
  T64_CHECK_INT(after.storage_size, before->storage_size);
  if (before->storage_size != 0u) {
    T64_CHECK(memcmp(before->storage, after.storage, before->storage_size) ==
              0);
  }
}

static void expect_refusal(const char *label, t64_machine *machine,
                           uint32_t kind, t64_status want) {
  machine_snapshot before;
  t64_save_info info;
  t64_save_info sentinel;
  t64_status status;

  take_snapshot(machine, &before);
  (void)memset(&info, 0xA5, sizeof info);
  sentinel = info;
  status = t64_machine_save_info(machine, kind, &info);
  if (status != want) {
    fprintf(stderr, "refusal '%s' kind %u: status %u want %u\n", label,
            (unsigned)kind, (unsigned)status, (unsigned)want);
  }
  T64_CHECK_INT(status, want);
  T64_CHECK(memcmp(&info, &sentinel, sizeof info) == 0);
  check_snapshot(label, machine, &before);
}

static void case_refusals(void) {
  static const uint32_t unknown[] = {0u, 8u, UINT32_C(0xFFFFFFFF)};
  static const uint32_t unsupported[] = {4u, 5u, 6u, 7u};
  static const uint8_t write_tx[10] = {0x05, 0x07, 0xA1, 0xA2, 0xA3,
                                       0xA4, 0xA5, 0xA6, 0xA7, 0xA8};
  t64_machine *machine = make_idle_machine(E4K_ID);
  t64_save_info info;
  size_t index;

  if (machine != NULL) {
    uint8_t frame[64];
    uint8_t image[64];

    /* Accepted success first, in the same setup. */
    (void)memset(&info, 0xA5, sizeof info);
    T64_CHECK_INT(t64_machine_save_info(machine, 1u, &info), T64_STATUS_OK);
    T64_CHECK_INT(info.kind, 1u);
    T64_CHECK_INT(info.size_bytes, 512u);
    T64_CHECK_INT(info.flags, 0u);
    expect_refusal("mismatch 2", machine, 2u, T64_STATUS_SAVE_DEVICE_MISMATCH);
    expect_refusal("mismatch 3", machine, 3u, T64_STATUS_SAVE_DEVICE_MISMATCH);
    for (index = 0u; index < sizeof unsupported / sizeof unsupported[0];
         index++) {
      expect_refusal("unsupported", machine, unsupported[index],
                     T64_STATUS_SAVE_UNSUPPORTED_DEVICE);
    }
    for (index = 0u; index < sizeof unknown / sizeof unknown[0]; index++) {
      expect_refusal("unknown", machine, unknown[index],
                     T64_STATUS_SAVE_UNKNOWN_DEVICE);
    }
    /* The busy flag follows the deadline: set after an accepted write (the
       deadline is in the future at the machine tick), clear at and after the
       deadline. */
    build_frame(frame, 10u, 1u, write_tx);
    run_txn(machine, frame, H_WRITE, image);
    T64_CHECK_INT(image[5], 0x01u);
    T64_CHECK(machine->save.busy_until > machine->tick);
    T64_CHECK(t64_save_in_flight(machine));
    T64_CHECK_INT(t64_machine_save_info(machine, 1u, &info), T64_STATUS_OK);
    T64_CHECK_INT(info.flags, T64_SAVE_INFO_BUSY);
    T64_CHECK(advance_guest_to(machine, machine->save.busy_until - 1u));
    T64_CHECK_INT(t64_machine_save_info(machine, 1u, &info), T64_STATUS_OK);
    T64_CHECK_INT(info.flags, T64_SAVE_INFO_BUSY);
    T64_CHECK(advance_guest_to(machine, machine->save.busy_until));
    T64_CHECK(!t64_save_in_flight(machine));
    T64_CHECK_INT(t64_machine_save_info(machine, 1u, &info), T64_STATUS_OK);
    T64_CHECK_INT(info.flags, 0u);
    /* Wire-level unsupported requests answer their specific flag or status,
       never a silent success: an RTC command on channel 4 of the EEPROM id,
       and an SRAM window access on an EEPROM id. */
    {
      static const uint8_t rtc_tx[1] = {0x06};
      uint32_t value = 0xA5A5A5A5u;

      build_frame(frame, 1u, 3u, rtc_tx);
      run_txn(machine, frame, unanswered_handshake(1u), image);
      T64_CHECK_INT(image[5], 0x43u);
      T64_CHECK(rx_all(image, 1u, 3u, 0xFFu));
      T64_CHECK_INT(bus_r32(machine, SRAM_FIRST, &value),
                    T64_STATUS_BUS_UNSUPPORTED);
      T64_CHECK_INT(value, 0xA5A5A5A5u);
    }
    t64_machine_destroy(machine);
  }

  /* The stem answers every kind with the mismatch status (the profile-device
     check comes before the kind checks); so does a legacy profile. */
  {
    static const char *const no_device_ids[] = {STEM_ID, LEGACY_ID};

    for (index = 0u; index < 2u; index++) {
      t64_machine *other = create_admitted(no_device_ids[index]);
      uint32_t kind;

      if (other == NULL) continue;
      for (kind = 0u; kind <= 9u; kind++) {
        expect_refusal("no device", other, kind,
                       T64_STATUS_SAVE_DEVICE_MISMATCH);
      }
      expect_refusal("no device", other, UINT32_C(0xFFFFFFFF),
                     T64_STATUS_SAVE_DEVICE_MISMATCH);
      t64_machine_destroy(other);
    }
  }

  /* Argument and admission checks come first: a NULL machine or info pointer
     is INVALID_ARGUMENT with the info untouched, an unadmitted machine is
     NOT_READY (even for an unknown kind, because admission is checked
     before the kind). */
  {
    const t64_profile_descriptor *profile = NULL;
    t64_machine *raw = NULL;
    t64_save_info sentinel;
    t64_machine *admitted = create_admitted(E4K_ID);

    (void)memset(&info, 0xA5, sizeof info);
    sentinel = info;
    T64_CHECK_INT(t64_machine_save_info(NULL, 1u, &info),
                  T64_STATUS_INVALID_ARGUMENT);
    T64_CHECK(memcmp(&info, &sentinel, sizeof info) == 0);
    if (admitted != NULL) {
      T64_CHECK_INT(t64_machine_save_info(admitted, 1u, NULL),
                    T64_STATUS_INVALID_ARGUMENT);
      t64_machine_destroy(admitted);
    }
    T64_CHECK_INT(t64_profile_lookup(E4K_ID, &profile), T64_STATUS_OK);
    T64_CHECK_INT(t64_machine_create(&allocator, profile, &raw), T64_STATUS_OK);
    if (raw != NULL) {
      T64_CHECK_INT(t64_machine_save_info(raw, 1u, &info), T64_STATUS_NOT_READY);
      T64_CHECK_INT(t64_machine_save_info(raw, 0u, &info), T64_STATUS_NOT_READY);
      T64_CHECK(memcmp(&info, &sentinel, sizeof info) == 0);
      T64_CHECK_INT(t64_machine_save_info(raw, 1u, NULL),
                    T64_STATUS_INVALID_ARGUMENT);
      t64_machine_destroy(raw);
    }
  }

  /* The SRAM id: the busy flag is set while a DMA is in flight and clear after
     the completion. */
  {
    t64_machine *sram = make_idle_machine(S32K_ID);

    if (sram != NULL) {
      const uint64_t start = sram->tick;

      T64_CHECK_INT(t64_machine_save_info(sram, 3u, &info), T64_STATUS_OK);
      T64_CHECK_INT(info.size_bytes, 32768u);
      T64_CHECK_INT(info.flags, 0u);
      expect_refusal("sram mismatch", sram, 1u,
                     T64_STATUS_SAVE_DEVICE_MISMATCH);
      expect_refusal("sram unsupported", sram, 4u,
                     T64_STATUS_SAVE_UNSUPPORTED_DEVICE);
      expect_refusal("sram unknown", sram, 0u, T64_STATUS_SAVE_UNKNOWN_DEVICE);
      T64_CHECK_INT(pi_start(sram, DMA_DRAM, DMA_CART, PI_RD_LEN, 15u),
                    T64_STATUS_OK);
      T64_CHECK(t64_save_in_flight(sram));
      T64_CHECK_INT(t64_machine_save_info(sram, 3u, &info), T64_STATUS_OK);
      T64_CHECK_INT(info.flags, T64_SAVE_INFO_BUSY);
      T64_CHECK(advance_guest_to(sram, start + 9u));
      T64_CHECK(!t64_save_in_flight(sram));
      T64_CHECK_INT(t64_machine_save_info(sram, 3u, &info), T64_STATUS_OK);
      T64_CHECK_INT(info.flags, 0u);
      t64_machine_destroy(sram);
    }
  }
}

/* ------------------------------------------------------------------------ */
/* digest-section                                                            */
/* ------------------------------------------------------------------------ */

#define SECTION_CAPACITY (T64_SAVE_SRAM_BYTES + 64u)

typedef struct {
  uint8_t bytes[SECTION_CAPACITY];
  size_t length;
} section_blob;

static void section_u32(section_blob *out, uint32_t value) {
  if (out->length + 4u > SECTION_CAPACITY) {
    T64_CHECK(false);
    return;
  }
  out->bytes[out->length++] = (uint8_t)(value >> 24);
  out->bytes[out->length++] = (uint8_t)(value >> 16);
  out->bytes[out->length++] = (uint8_t)(value >> 8);
  out->bytes[out->length++] = (uint8_t)value;
}

/* Written from the section 11 field table of docs/spec/save-media.md (big
 * endian, new-span class): u32 kind, u32 size_bytes, u64 busy_until, u32
 * pi_save_direction, then size_bytes of storage. It reads no machine field:
 * every argument is a literal of the state under test. */
static void encode_section11(section_blob *out, uint32_t kind, uint32_t size,
                             uint64_t deadline, uint32_t direction,
                             const uint8_t *storage) {
  out->length = 0u;
  section_u32(out, kind);
  section_u32(out, size);
  section_u32(out, (uint32_t)(deadline >> 32));
  section_u32(out, (uint32_t)deadline);
  section_u32(out, direction);
  if (out->length + size > SECTION_CAPACITY) {
    T64_CHECK(false);
    return;
  }
  (void)memcpy(out->bytes + out->length, storage, size);
  out->length += size;
}

static section_blob expected_section;

static void check_section(const char *label, const t64_machine *machine,
                          uint32_t kind, uint32_t size, uint64_t deadline,
                          uint32_t direction, const uint8_t *storage) {
  stream captured;

  encode_section11(&expected_section, kind, size, deadline, direction,
                   storage);
  if (!capture_stream(machine, &captured)) return;
  {
    const size_t span = marker_span(&captured, 11u);

    T64_CHECK(span != SIZE_MAX);
    if (span != SIZE_MAX && span + 1u < captured.span_count) {
      const t64_digest_span *body = &captured.spans[span + 1u];
      const bool same =
          body->class == T64_DIGEST_CLASS_NEW &&
          body->length == expected_section.length &&
          body->offset + body->length == captured.length &&
          memcmp(captured.bytes + body->offset, expected_section.bytes,
                 expected_section.length) == 0;

      if (!same) {
        fprintf(stderr, "section 11 differs from the reference: %s\n", label);
      }
      T64_CHECK(same);
    }
  }
  free_stream(&captured);
}

typedef struct {
  const char *label;
  void (*flip)(t64_machine *machine, size_t size);
} flip_row;

static void flip_deadline(t64_machine *machine, size_t size) {
  (void)size;
  machine->save.busy_until ^= UINT64_C(1);
}

static void flip_direction(t64_machine *machine, size_t size) {
  (void)size;
  machine->pi.active_save_direction ^= 1u;
}

static void flip_first(t64_machine *machine, size_t size) {
  (void)size;
  machine->save.bytes[0] ^= UINT8_C(1);
}

static void flip_middle(t64_machine *machine, size_t size) {
  machine->save.bytes[size / 2u] ^= UINT8_C(1);
}

static void flip_last(t64_machine *machine, size_t size) {
  machine->save.bytes[size - 1u] ^= UINT8_C(1);
}

static const flip_row flip_rows[] = {
    {"deadline", flip_deadline}, {"direction", flip_direction},
    {"first byte", flip_first},  {"middle byte", flip_middle},
    {"last byte", flip_last},
};

/* Every field family alters the digest on its own and restores it when
 * flipped back. */
static void check_field_flips(t64_machine *machine, size_t size) {
  uint8_t base[T64_DIGEST_SIZE];
  uint8_t changed[T64_DIGEST_SIZE];
  uint8_t restored[T64_DIGEST_SIZE];
  size_t index;

  digest_of(machine, base);
  for (index = 0u; index < sizeof flip_rows / sizeof flip_rows[0]; index++) {
    flip_rows[index].flip(machine, size);
    digest_of(machine, changed);
    if (memcmp(base, changed, T64_DIGEST_SIZE) == 0) {
      fprintf(stderr, "field flip '%s' did not alter the digest\n",
              flip_rows[index].label);
    }
    T64_CHECK(memcmp(base, changed, T64_DIGEST_SIZE) != 0);
    flip_rows[index].flip(machine, size);
    digest_of(machine, restored);
    T64_CHECK(memcmp(base, restored, T64_DIGEST_SIZE) == 0);
  }
}

static uint8_t section_model[T64_SAVE_SRAM_BYTES];

static void case_digest_section(void) {
  static const uint8_t write_tx[10] = {0x05, 0x01, 0x11, 0x12, 0x13,
                                       0x14, 0x15, 0x16, 0x17, 0x18};
  uint8_t frame[64];
  uint8_t image[64];

  /* e4k: blank; after a write (deadline in the future); stale (the stored
     deadline is kept after it passes). Expected storage is the 0xFF fill with
     block 1 replaced by 11 to 18; E1 = 10,000 + 22,876 so D = 407,876. */
  {
    t64_machine *machine = make_idle_machine(E4K_ID);
    const uint64_t deadline = UINT64_C(10000) + H_WRITE + BUSY_WINDOW;

    if (machine != NULL) {
      (void)memset(section_model, 0xFF, 512u);
      check_section("e4k blank", machine, 1u, 512u, 0u, 0u, section_model);
      T64_CHECK(advance_guest_to(machine, 3000u));
      build_frame(frame, 10u, 1u, write_tx);
      write_frame(machine, frame);
      read_image(machine, 10000u, H_WRITE, image);
      (void)memcpy(section_model + 8u, write_tx + 2u, 8u);
      check_section("e4k after write", machine, 1u, 512u, deadline, 0u,
                    section_model);
      T64_CHECK(machine->tick < deadline);
      check_field_flips(machine, 512u);
      T64_CHECK(advance_guest_to(machine, deadline + 5000u));
      check_section("e4k stale deadline", machine, 1u, 512u, deadline, 0u,
                    section_model);
      t64_machine_destroy(machine);
    }
  }
  /* e16k blank. */
  {
    t64_machine *machine = make_idle_machine(E16K_ID);

    if (machine != NULL) {
      (void)memset(section_model, 0xFF, 2048u);
      check_section("e16k blank", machine, 2u, 2048u, 0u, 0u, section_model);
      check_field_flips(machine, 2048u);
      t64_machine_destroy(machine);
    }
  }
  /* s32k: blank; after a CPU word write; mid-DMA in both directions. */
  {
    t64_machine *machine = make_idle_machine(S32K_ID);

    if (machine != NULL) {
      const uint64_t start = machine->tick;

      (void)memset(section_model, 0x00, T64_SAVE_SRAM_BYTES);
      check_section("s32k blank", machine, 3u, 32768u, 0u, 0u, section_model);
      T64_CHECK_INT(bus_w32(machine, UINT32_C(0x08000104),
                            UINT32_C(0x12345678)),
                    T64_STATUS_OK);
      section_model[0x104] = 0x12;
      section_model[0x105] = 0x34;
      section_model[0x106] = 0x56;
      section_model[0x107] = 0x78;
      check_section("s32k after word write", machine, 3u, 32768u, 0u, 0u,
                    section_model);
      /* RDRAM to SRAM in flight: direction 2, storage unchanged until C. */
      (void)memcpy(machine->ram + DMA_DRAM, dma_pattern, sizeof dma_pattern);
      T64_CHECK_INT(pi_start(machine, DMA_DRAM, DMA_CART, PI_RD_LEN, 15u),
                    T64_STATUS_OK);
      check_section("s32k mid-DMA RDRAM to SRAM", machine, 3u, 32768u, 0u, 2u,
                    section_model);
      check_field_flips(machine, 32768u);
      T64_CHECK(advance_guest_to(machine, start + 9u));
      (void)memcpy(section_model + 0x100u, dma_pattern, 16u);
      /* The CPU word 0x12345678 was stored at 0x104 before the DMA; the DMA
         then replaced bytes 0x100 to 0x10F with the pattern. */
      check_section("s32k after DMA", machine, 3u, 32768u, 0u, 0u,
                    section_model);
      T64_CHECK_INT(bus_w32(machine, PI_STATUS, 2u), T64_STATUS_OK);
      /* SRAM to RDRAM in flight: direction 1. */
      (void)memset(machine->ram + DMA_BACK_DRAM, 0, 16u);
      T64_CHECK_INT(pi_start(machine, DMA_BACK_DRAM, DMA_CART, PI_WR_LEN, 15u),
                    T64_STATUS_OK);
      check_section("s32k mid-DMA SRAM to RDRAM", machine, 3u, 32768u, 0u, 1u,
                    section_model);
      T64_CHECK(advance_guest_to(machine, start + 18u));
      t64_machine_destroy(machine);
    }
  }
  /* The stem has no section 11: markers 1 to 10 only and the idle digest
     literal of storage-blank. */
  {
    t64_machine *machine = create_admitted(STEM_ID);
    stream captured;
    uint32_t ids[16];

    if (machine != NULL) {
      if (capture_stream(machine, &captured)) {
        T64_CHECK_INT(list_markers(&captured, ids, 16u), 10u);
        T64_CHECK(marker_span(&captured, 11u) == SIZE_MAX);
        free_stream(&captured);
      }
      t64_machine_destroy(machine);
    }
  }
}

/* ------------------------------------------------------------------------ */
/* reset-blank                                                               */
/* ------------------------------------------------------------------------ */

static void case_reset_blank(void) {
  static const uint8_t write_tx[10] = {0x05, 0x07, 0xA1, 0xA2, 0xA3,
                                       0xA4, 0xA5, 0xA6, 0xA7, 0xA8};
  static const struct {
    const char *id;
    uint8_t fill;
    size_t size;
  } rows[] = {{E4K_ID, 0xFFu, 512u},
              {E16K_ID, 0xFFu, 2048u},
              {S32K_ID, 0x00u, 32768u}};
  size_t index;

  for (index = 0u; index < sizeof rows / sizeof rows[0]; index++) {
    t64_machine *machine = create_admitted(rows[index].id);
    t64_machine *fresh = create_admitted(rows[index].id);
    uint8_t asset[IPL3_ASSET_BYTES];
    uint8_t garbage[100];
    uint8_t before[T64_DIGEST_SIZE];
    uint8_t after[T64_DIGEST_SIZE];
    uint8_t digest_machine[T64_DIGEST_SIZE];
    uint8_t digest_fresh[T64_DIGEST_SIZE];
    size_t at;

    if (machine == NULL || fresh == NULL) {
      if (machine != NULL) t64_machine_destroy(machine);
      if (fresh != NULL) t64_machine_destroy(fresh);
      continue;
    }
    T64_CHECK(read_openipl3_asset(asset));
    /* Write something: a block write on the EEPROM ids, a CPU word store on
       the SRAM id. */
    if (rows[index].fill == 0xFFu) {
      uint8_t rx[1] = {0xFF};

      T64_CHECK_INT(t64_save_channel4_execute(machine, 1000u, write_tx, 10u, rx,
                                              1u),
                    0u);
      T64_CHECK(machine->save.busy_until != 0u);
    } else {
      T64_CHECK_INT(bus_w32(machine, UINT32_C(0x08000100), UINT32_C(0xCAFEBABE)),
                    T64_STATUS_OK);
    }
    T64_CHECK(machine->save.bytes[rows[index].fill == 0xFFu ? 8u * 7u : 0x100u] !=
              rows[index].fill);

    /* A refused replacement (an invalid image) leaves bytes and deadline. */
    (void)memset(garbage, 0x5A, sizeof garbage);
    digest_of(machine, before);
    T64_CHECK(t64_machine_admit_rom(machine, garbage, sizeof garbage) !=
              T64_STATUS_OK);
    digest_of(machine, after);
    T64_CHECK(memcmp(before, after, T64_DIGEST_SIZE) == 0);
    T64_CHECK(machine->save.bytes[rows[index].fill == 0xFFu ? 8u * 7u : 0x100u] !=
              rows[index].fill);

    /* A successful replacement restores a blank device and the digest of a
       freshly created and admitted machine of the same id. */
    T64_CHECK_INT(t64_machine_admit_rom(machine, asset, sizeof asset),
                  T64_STATUS_OK);
    T64_CHECK_INT(machine->save.busy_until, 0u);
    for (at = 0u; at < rows[index].size; at++) {
      if (machine->save.bytes[at] != rows[index].fill) {
        T64_CHECK_INT(machine->save.bytes[at], rows[index].fill);
        break;
      }
    }
    digest_of(machine, digest_machine);
    digest_of(fresh, digest_fresh);
    T64_CHECK(memcmp(digest_machine, digest_fresh, T64_DIGEST_SIZE) == 0);

    /* A direct reset restores the blank state too. */
    machine->save.bytes[0] = (uint8_t)(rows[index].fill ^ 0x55u);
    machine->save.busy_until = 99u;
    t64_save_reset_machine_state(machine);
    T64_CHECK_INT(machine->save.busy_until, 0u);
    T64_CHECK_INT(machine->save.bytes[0], rows[index].fill);
    t64_machine_destroy(machine);
    t64_machine_destroy(fresh);
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
    {"sram-dma", case_sram_dma},
    {"eeprom-commands", case_eeprom_commands},
    {"eeprom-blocks", case_eeprom_blocks},
    {"eeprom-busy", case_eeprom_busy},
    {"eeprom-timing", case_eeprom_timing},
    {"selection", case_selection},
    {"refusals", case_refusals},
    {"digest-section", case_digest_section},
    {"reset-blank", case_reset_blank},
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
