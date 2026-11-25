/*
 * Phase 9 joybus parser, responder and bounded-campaign tests (plan 09-03).
 *
 * Every 64-byte image and every expected byte below is authored from the rules
 * of the parser (escapes 0x00 skip, 0xFD reset, 0xFE end, 0xFF nop; counts in
 * the low six bits and flags in the top two; a handshake must fit offsets 0 to
 * 62; five channels) before any decoder breadth. Every figure is a named
 * software-profile policy value; no hardware accuracy is claimed. Nothing here
 * calls a production helper to compute an expectation: ticks are written out
 * from the formulas and button bytes are written out as literals.
 *
 * frame-vectors, controller-responses, button-vectors, channels-and-ports and
 * total-bounded make up the parser suite. total-bounded reads its iteration
 * count from T64_JOYBUS_FUZZ_ITERATIONS (default 20,000; the sanitizer preset
 * raises it on the same CTest name).
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_internal.h"
#include "t64_test.h"

#define SI_STEM_ID "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1"
#define IPL3_ASSET_BYTES 4096u

/* ---- frame construction and table expectations -------------------------- */

static void make_frame(uint8_t frame[64], const uint8_t *bytes, size_t count) {
  (void)memset(frame, 0, 64u);
  if (count > 64u) {
    T64_CHECK(false);
    return;
  }
  (void)memcpy(frame, bytes, count);
}

typedef struct {
  uint32_t kind;
  uint32_t channel;
  uint32_t offset;
  uint32_t tx;
  uint32_t rx;
  uint32_t flags;
} want_entry;

#define T_ENTRY(channel, offset, tx, rx, flags) \
  {T64_JOYBUS_ENTRY_TRANSACT, (channel), (offset), (tx), (rx), (flags)}
#define R_ENTRY(channel, offset) \
  {T64_JOYBUS_ENTRY_RESET, (channel), (offset), 0u, 0u, 0u}

static void check_table(const char *label, const uint8_t frame[64],
                        const want_entry *want, size_t count) {
  t64_joybus_table table;
  size_t index;

  t64_joybus_parse(frame, &table);
  if (table.entry_count != count) {
    fprintf(stderr, "vector '%s': %u entries, want %zu\n", label,
            (unsigned)table.entry_count, count);
  }
  T64_CHECK(table.parsed);
  T64_CHECK_INT(table.entry_count, count);
  for (index = 0u; index < count && index < T64_JOYBUS_CHANNELS; index++) {
    const t64_joybus_entry *got = &table.entries[index];

    if (got->kind != want[index].kind || got->channel != want[index].channel ||
        got->offset != want[index].offset || got->tx_count != want[index].tx ||
        got->rx_count != want[index].rx || got->flags != want[index].flags) {
      fprintf(stderr, "vector '%s' entry %zu differs\n", label, index);
      T64_CHECK(false);
    }
  }
}

static void case_frame_vectors(void) {
  uint8_t frame[64];

  /* End escape stops parsing; earlier entries stay valid. */
  {
    const uint8_t bytes[] = {0x01, 0x04, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
                             0xFE, 0x01, 0x04, 0x01, 0xFF, 0xFF, 0xFF, 0xFF};
    const want_entry want[] = {T_ENTRY(0u, 0u, 1u, 4u, 0u)};

    make_frame(frame, bytes, sizeof bytes);
    check_table("end escape", frame, want, 1u);
  }
  /* Skip escape 0x00 consumes a channel index and makes no entry. */
  {
    const uint8_t bytes[] = {0x00, 0x01, 0x04, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
                             0xFE};
    const want_entry want[] = {T_ENTRY(1u, 1u, 1u, 4u, 0u)};

    make_frame(frame, bytes, sizeof bytes);
    check_table("skip escape", frame, want, 1u);
  }
  /* Reset escape 0xFD makes a reset entry and consumes a channel index. */
  {
    const uint8_t bytes[] = {0xFD, 0x01, 0x04, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
                             0xFE};
    const want_entry want[] = {R_ENTRY(0u, 0u), T_ENTRY(1u, 1u, 1u, 4u, 0u)};

    make_frame(frame, bytes, sizeof bytes);
    check_table("reset escape", frame, want, 2u);
    {
      t64_joybus_table table;
      t64_joybus_controller controller = {true, 0u, 0u, 0u};

      t64_joybus_parse(frame, &table);
      /* Reset 62,500 ticks plus the absent channel 1 (2000 + 188 + 3907). */
      T64_CHECK_INT(t64_joybus_plan(frame, &table, true), 62500u + 6095u);
      t64_joybus_execute(frame, &table, &controller);
      /* The reset entry leaves PIF RAM alone; only channel 1 wrote its RX
         byte (no device: 0x84). */
      T64_CHECK_INT(frame[0], 0xFDu);
      T64_CHECK_INT(frame[2], 0x84u);
    }
  }
  /* Nop 0xFF consumes one byte and no channel. */
  {
    const uint8_t bytes[] = {0xFF, 0xFF, 0x01, 0x04, 0x01, 0xFF, 0xFF, 0xFF,
                             0xFF, 0xFE};
    const want_entry want[] = {T_ENTRY(0u, 2u, 1u, 4u, 0u)};

    make_frame(frame, bytes, sizeof bytes);
    check_table("nop escape", frame, want, 1u);
  }
  /* Counts take the low six bits and flags the top two. */
  {
    const uint8_t bytes[] = {0x41, 0xC4, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
                             0x81, 0x04, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE};
    const want_entry want[] = {T_ENTRY(0u, 0u, 1u, 4u, 0x40u),
                               T_ENTRY(1u, 7u, 1u, 4u, 0x80u)};

    make_frame(frame, bytes, sizeof bytes);
    check_table("counts and flags", frame, want, 2u);
  }
  /* A TX byte of 0x40, 0x80 or 0xC0 alone (count zero) is an incomplete
     handshake that stops parsing. */
  {
    static const uint8_t first[] = {0x40u, 0x80u, 0xC0u};
    size_t index;

    for (index = 0u; index < sizeof first; index++) {
      const uint8_t bytes[] = {0x01, 0x04, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
                               first[index], 0x04, 0x01, 0xFF, 0xFF, 0xFF,
                               0xFF};
      const want_entry want[] = {T_ENTRY(0u, 0u, 1u, 4u, 0u)};

      make_frame(frame, bytes, sizeof bytes);
      check_table("count zero", frame, want, 1u);
    }
  }
  /* A handshake whose body ends past offset 62 stops parsing; one that ends
     exactly at offset 62 is accepted. */
  {
    want_entry want[] = {T_ENTRY(0u, 0u, 1u, 4u, 0u),
                         T_ENTRY(1u, 56u, 1u, 4u, 0u)};

    (void)memset(frame, 0xFF, 64u);
    frame[0] = 0x01;
    frame[1] = 0x04;
    frame[2] = 0x01;
    frame[7] = 0x00; /* skip channel 1... then pad with nops to offset 56 */
    frame[56] = 0x01;
    frame[57] = 0x04;
    frame[58] = 0x01;
    frame[63] = 0u;
    /* Channel 1 is the skip at offset 7, so the entry at 56 is channel 2. */
    want[1].channel = 2u;
    check_table("handshake ends at offset 62", frame, want, 2u);
    (void)memset(frame, 0xFF, 64u);
    frame[0] = 0x01;
    frame[1] = 0x04;
    frame[2] = 0x01;
    frame[7] = 0x00;
    frame[57] = 0x01;
    frame[58] = 0x04;
    frame[59] = 0x01;
    frame[63] = 0u;
    check_table("handshake ends at offset 63", frame, want, 1u);
    /* A TX byte at offset 62 needs its RX byte at 63: incomplete. */
    (void)memset(frame, 0xFF, 64u);
    frame[62] = 0x01;
    frame[63] = 0x04;
    check_table("tx byte at offset 62", frame, want, 0u);
    /* An overlong RX count: 0x10 needs 16 response bytes past offset 55. */
    (void)memset(frame, 0xFF, 64u);
    frame[0] = 0x01;
    frame[1] = 0x04;
    frame[2] = 0x01;
    frame[7] = 0x00;
    frame[55] = 0x01;
    frame[56] = 0x10;
    frame[57] = 0x01;
    check_table("overlong rx", frame, want, 1u);
  }
  /* The sixth channel index is never parsed. */
  {
    uint8_t bytes[24];
    want_entry want[5];
    size_t index;

    for (index = 0u; index < 6u; index++) {
      bytes[index * 4u] = 0x01;
      bytes[index * 4u + 1u] = 0x01;
      bytes[index * 4u + 2u] = 0x01;
      bytes[index * 4u + 3u] = 0xFF;
    }
    for (index = 0u; index < 5u; index++) {
      const want_entry entry = T_ENTRY((uint32_t)index,
                                       (uint32_t)(index * 4u), 1u, 1u, 0u);

      want[index] = entry;
    }
    make_frame(frame, bytes, sizeof bytes);
    check_table("sixth channel index", frame, want, 5u);
    {
      const uint8_t skips[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01,
                               0xFF};

      make_frame(frame, skips, sizeof skips);
      check_table("five skips then a handshake", frame, want, 0u);
    }
  }
  /* A frame of 63 nop bytes terminates, and byte 63 is never read as a
     handshake byte whatever its value. */
  {
    uint8_t byte63;

    for (byte63 = 0u; byte63 < 4u; byte63++) {
      (void)memset(frame, 0xFF, 64u);
      frame[63] = byte63 == 0u ? 0x00u : (uint8_t)(0x40u + byte63);
      check_table("63 nops", frame, NULL, 0u);
    }
  }
}

/* ---- responders and ticks ------------------------------------------------ */

/* The receive fill of the frames whose answer is a timeout or an absent device:
 * not 0xFF, so that "the bytes stay as the guest wrote them" differs from "the
 * area is filled with 0xFF" (plan 09-10 review L5 WR-02: every frame used 0xFF
 * and a memset in the timeout branch passed every test). */
#define RX_FILL 0xA5u

/* A controller on port 0 with a fixed state for the responder cases. */
static const t64_joybus_controller local_controller = {true, 0x1234u, 0x56u,
                                                       0x9Au};

/* Execute a frame once and return the image; ticks come from plan. */
static uint64_t run_frame(uint8_t frame[64], const uint8_t *bytes,
                          size_t count) {
  t64_joybus_table table;
  uint64_t ticks;

  make_frame(frame, bytes, count);
  t64_joybus_parse(frame, &table);
  ticks = t64_joybus_plan(frame, &table, true);
  t64_joybus_execute(frame, &table, &local_controller);
  return ticks;
}

static bool response_matches(const uint8_t *got, const uint8_t *want,
                             size_t count) {
  return memcmp(got, want, count) == 0;
}

static void case_timelines(void);

static void case_controller_responses(void) {
  uint8_t frame[64];
  uint64_t ticks;

  /* Info (0x00) and reset+info (0xFF) with TX 1 answer 05 00 02; the RX byte
     keeps its count with no flag. */
  {
    const uint8_t info[] = {0x01, 0x03, 0x00, 0xFF, 0xFF, 0xFF, 0xFE};
    const uint8_t reset_info[] = {0x01, 0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE};
    const uint8_t want[3] = {0x05u, 0x00u, 0x02u};
    const uint8_t wrong[3] = {0x05u, 0x00u, 0x01u};

    ticks = run_frame(frame, info, sizeof info);
    T64_CHECK(response_matches(frame + 3, want, 3u));
    T64_CHECK_INT(frame[1], 0x03u);
    /* 2000 + 188 + 438 + 3 * 2000 + 250 */
    T64_CHECK_INT(ticks, 8876u);
    /* One deliberately wrong expectation (status 0x01) is rejected. */
    T64_CHECK(!response_matches(frame + 3, wrong, 3u));
    ticks = run_frame(frame, reset_info, sizeof reset_info);
    T64_CHECK(response_matches(frame + 3, want, 3u));
    T64_CHECK_INT(ticks, 8876u);
  }
  /* State (0x01): the four bytes of the fixed controller (buttons 0x1234
     with the 0x00C0 bits masked, x 0x56, y 0x9A). */
  {
    const uint8_t state[] = {0x01, 0x04, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE};
    const uint8_t want[4] = {0x12u, 0x34u & 0x3Fu, 0x56u, 0x9Au};

    ticks = run_frame(frame, state, sizeof state);
    T64_CHECK(response_matches(frame + 3, want, 4u));
    T64_CHECK_INT(frame[1], 0x04u);
    /* 2000 + 188 + 438 + 4 * 2000 + 250 */
    T64_CHECK_INT(ticks, 10876u);
  }
  /* Pak read, pak write and an unlisted command time out: flag 0x40, no
     data, and the timeout tick cost 2000n + 188 + 3,907. */
  {
    static const uint8_t commands[] = {0x02u, 0x03u, 0x04u};
    size_t index;

    for (index = 0u; index < sizeof commands; index++) {
      const uint8_t bytes[] = {0x01, 0x04, commands[index], RX_FILL, RX_FILL,
                               RX_FILL, RX_FILL, 0xFE};

      ticks = run_frame(frame, bytes, sizeof bytes);
      T64_CHECK_INT(frame[1], 0x44u);
      /* no data: the receive bytes keep the fill the guest wrote, which is
         not 0xFF, so a memset of the area would be seen */
      T64_CHECK_INT(frame[3], RX_FILL);
      T64_CHECK_INT(frame[6], RX_FILL);
      T64_CHECK_INT(ticks, 6095u);
    }
    {
      /* Pak read with TX 3 and 33 response bytes: 6,000 + 188 + 3,907. */
      uint8_t bytes[44];

      (void)memset(bytes, RX_FILL, sizeof bytes);
      bytes[0] = 0x03;
      bytes[1] = 0x21;
      bytes[2] = 0x02;
      bytes[3] = 0x00;
      bytes[4] = 0x00;
      bytes[38] = 0xFE;
      ticks = run_frame(frame, bytes, sizeof bytes);
      T64_CHECK_INT(frame[1], 0x61u);
      T64_CHECK_INT(frame[5], RX_FILL);
      T64_CHECK_INT(ticks, 10095u);
    }
  }
  /* A TX count other than the expected one is unsupported. */
  {
    const uint8_t bytes[] = {0x02, 0x04, 0x01, 0x00, RX_FILL, RX_FILL, RX_FILL,
                             RX_FILL, 0xFE};

    ticks = run_frame(frame, bytes, sizeof bytes);
    T64_CHECK_INT(frame[1], 0x44u);
    T64_CHECK_INT(frame[4], RX_FILL);
    /* 2 * 2000 + 188 + 3,907 */
    T64_CHECK_INT(ticks, 8095u);
  }
  /* RX count above the response length writes the available bytes and sets
     0x40; below writes only that many; zero writes nothing. */
  {
    const uint8_t above[] = {0x01, 0x04, 0x00, RX_FILL, RX_FILL, RX_FILL,
                             RX_FILL, 0xFE};
    const uint8_t below[] = {0x01, 0x02, 0x01, RX_FILL, RX_FILL, RX_FILL, 0xFE};
    const uint8_t zero[] = {0x01, 0x00, 0x01, RX_FILL, 0xFE};

    ticks = run_frame(frame, above, sizeof above);
    T64_CHECK_INT(frame[1], 0x44u);
    T64_CHECK_INT(frame[3], 0x05u);
    T64_CHECK_INT(frame[4], 0x00u);
    T64_CHECK_INT(frame[5], 0x02u);
    T64_CHECK_INT(frame[6], RX_FILL);
    /* three transferred bytes: 2000 + 188 + 438 + 6000 + 250 */
    T64_CHECK_INT(ticks, 8876u);
    ticks = run_frame(frame, below, sizeof below);
    T64_CHECK_INT(frame[1], 0x02u);
    T64_CHECK_INT(frame[3], 0x12u);
    T64_CHECK_INT(frame[4], 0x34u & 0x3Fu);
    T64_CHECK_INT(frame[5], RX_FILL);
    /* 2000 + 188 + 438 + 2 * 2000 + 250 */
    T64_CHECK_INT(ticks, 6876u);
    ticks = run_frame(frame, zero, sizeof zero);
    T64_CHECK_INT(frame[1], 0x00u);
    T64_CHECK_INT(frame[3], RX_FILL);
    /* 2000 + 188 + 438 + 0 + 250 */
    T64_CHECK_INT(ticks, 2876u);
  }
  /* The RX byte is rebuilt from the parsed count on every execution: a frame
     executed again never accumulates flag bits. */
  {
    const uint8_t absent[] = {0x00, 0x01, 0x04, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
                              0xFE};
    t64_joybus_table table;
    uint8_t first[64];

    make_frame(frame, absent, sizeof absent);
    t64_joybus_parse(frame, &table);
    T64_CHECK_INT(t64_joybus_plan(frame, &table, true), 6095u);
    t64_joybus_execute(frame, &table, &local_controller);
    T64_CHECK_INT(frame[2], 0x84u);
    (void)memcpy(first, frame, 64u);
    t64_joybus_execute(frame, &table, &local_controller);
    T64_CHECK(memcmp(first, frame, 64u) == 0);
    T64_CHECK_INT(frame[2], 0x84u);
  }
  /* A TX skip flag runs nothing and writes nothing; a TX reset flag acts as
     command 0xFF (info) whatever the command byte says. */
  {
    const uint8_t skip[] = {0x81, 0x04, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE};
    const uint8_t reset_flag[] = {0x41, 0x03, 0x01, 0xFF, 0xFF, 0xFF, 0xFE};

    ticks = run_frame(frame, skip, sizeof skip);
    T64_CHECK_INT(frame[1], 0x04u);
    T64_CHECK_INT(frame[3], 0xFFu);
    T64_CHECK_INT(ticks, 0u);
    ticks = run_frame(frame, reset_flag, sizeof reset_flag);
    T64_CHECK_INT(frame[3], 0x05u);
    T64_CHECK_INT(frame[4], 0x00u);
    T64_CHECK_INT(frame[5], 0x02u);
    T64_CHECK_INT(ticks, 8876u);
  }
  /* The tick totals of the reset escape and of an absent channel. */
  {
    const uint8_t reset[] = {0xFD, 0xFE};
    t64_joybus_table table;

    make_frame(frame, reset, sizeof reset);
    t64_joybus_parse(frame, &table);
    T64_CHECK_INT(t64_joybus_plan(frame, &table, true), 62500u);
  }
  /* The same totals through the SI timeline. */
  case_timelines();
}

static bool joybus_test_allocate(void *context, size_t size, size_t alignment,
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

static void joybus_test_release(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

static const t64_host_allocator allocator =
    T64_HOST_ALLOCATOR_INIT(joybus_test_allocate, joybus_test_release, NULL);

/* An RD64B timeline over the info frame and the reset frame: C = S + H + X. */
static void check_timeline(const uint8_t *bytes, size_t count,
                           uint64_t handshake) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t asset[IPL3_ASSET_BYTES];
  const char *path = getenv("T64_TEST_IPL3_PATH");
  FILE *file;
  uint64_t cancelled = 0u;
  const uint64_t start = 3000u;

  T64_CHECK(path != NULL && path[0] != '\0');
  if (path == NULL || path[0] == '\0') return;
  file = fopen(path, "rb");
  T64_CHECK(file != NULL);
  if (file == NULL) return;
  T64_CHECK_INT(fread(asset, 1u, sizeof asset, file), sizeof asset);
  (void)fclose(file);
  T64_CHECK_INT(t64_profile_lookup(SI_STEM_ID, &profile), T64_STATUS_OK);
  if (profile == NULL) return;
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  if (machine == NULL) return;
  T64_CHECK_INT(t64_machine_admit_rom(machine, asset, sizeof asset),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_schedule_cancel(machine, T64_DEVICE_CPU,
                                    T64_CPU_EVENT_STEP, 0u, &cancelled),
                T64_STATUS_OK);
  (void)memcpy(machine->ram + 0x3800u, bytes, count);
  machine->ram[0x3800u + 63u] = 0x01u;
  T64_CHECK_INT(t64_machine_bus_write(machine, T64_SI_BASE, 4u, 0x3800u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, T64_SI_BASE + T64_SI_REG_WR64B,
                                      4u, T64_SI_PIF_ADDRESS),
                T64_STATUS_OK);
  {
    t64_stop_request request =
        T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK, start);
    t64_stop_result result = T64_STOP_RESULT_INIT;

    T64_CHECK_INT(t64_machine_advance(machine, &request, &result),
                  T64_STATUS_OK);
  }
  T64_CHECK_INT(t64_machine_bus_write(machine, T64_SI_BASE + T64_SI_REG_STATUS,
                                      4u, 0u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, T64_SI_BASE, 4u, 0x3900u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, T64_SI_BASE + T64_SI_REG_RD64B,
                                      4u, T64_SI_PIF_ADDRESS),
                T64_STATUS_OK);
  T64_CHECK_INT(machine->si.execute_tick, start + handshake);
  T64_CHECK_INT(machine->si.complete_tick, start + handshake + 4096u);
  t64_machine_destroy(machine);
}

static void case_timelines(void) {
  const uint8_t info[] = {0x01, 0x03, 0x00, 0xFF, 0xFF, 0xFF, 0xFE};
  const uint8_t reset[] = {0xFD, 0xFE};

  /* C = S + 8,876 + 4,096 and C = S + 62,500 + 4,096. */
  check_timeline(info, sizeof info, 8876u);
  check_timeline(reset, sizeof reset, 62500u);
}

/* ---- sixteen single-button vectors --------------------------------------- */

typedef struct {
  const char *name;
  uint16_t buttons;
  uint8_t byte0;
  uint8_t byte1;
} button_vector;

/* Literal bytes: bit 15 is A and bit 0 is C-right; bits 0x0080 (RST) and
 * 0x0040 (reserved) are masked to zero, never refused. */
static const button_vector button_vectors[16] = {
    {"A", 0x8000u, 0x80u, 0x00u},      {"B", 0x4000u, 0x40u, 0x00u},
    {"Z", 0x2000u, 0x20u, 0x00u},      {"Start", 0x1000u, 0x10u, 0x00u},
    {"Up", 0x0800u, 0x08u, 0x00u},     {"Down", 0x0400u, 0x04u, 0x00u},
    {"Left", 0x0200u, 0x02u, 0x00u},   {"Right", 0x0100u, 0x01u, 0x00u},
    {"RST", 0x0080u, 0x00u, 0x00u},    {"reserved", 0x0040u, 0x00u, 0x00u},
    {"L", 0x0020u, 0x00u, 0x20u},      {"R", 0x0010u, 0x00u, 0x10u},
    {"C-up", 0x0008u, 0x00u, 0x08u},   {"C-down", 0x0004u, 0x00u, 0x04u},
    {"C-left", 0x0002u, 0x00u, 0x02u}, {"C-right", 0x0001u, 0x00u, 0x01u},
};

/* A wrong decoder: the two bytes swapped. */
static void swapped_decode(uint16_t buttons, uint8_t *byte0, uint8_t *byte1) {
  *byte0 = (uint8_t)(buttons & 0x3Fu);
  *byte1 = (uint8_t)(buttons >> 8);
}

static void case_button_vectors(void) {
  static const uint8_t state[] = {0x01, 0x04, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
                                  0xFE};
  uint8_t frame[64];
  size_t index;
  bool wrong_decoder_rejected = false;
  t64_joybus_table table;

  for (index = 0u; index < 16u; index++) {
    t64_joybus_controller controller = {true, button_vectors[index].buttons,
                                        0u, 0u};
    uint8_t wrong0;
    uint8_t wrong1;

    make_frame(frame, state, sizeof state);
    t64_joybus_parse(frame, &table);
    t64_joybus_execute(frame, &table, &controller);
    if (frame[3] != button_vectors[index].byte0 ||
        frame[4] != button_vectors[index].byte1) {
      fprintf(stderr, "button %s: %02x %02x\n", button_vectors[index].name,
              (unsigned)frame[3], (unsigned)frame[4]);
    }
    T64_CHECK_INT(frame[3], button_vectors[index].byte0);
    T64_CHECK_INT(frame[4], button_vectors[index].byte1);
    T64_CHECK_INT(frame[5], 0x00u);
    T64_CHECK_INT(frame[6], 0x00u);
    swapped_decode(button_vectors[index].buttons, &wrong0, &wrong1);
    if (wrong0 != button_vectors[index].byte0 ||
        wrong1 != button_vectors[index].byte1) {
      wrong_decoder_rejected = true;
    }
  }
  /* The swapped decoder disagrees with the literals, so a vector set that
     compared against it would fail. */
  T64_CHECK(wrong_decoder_rejected);
  /* Axes are the unsigned bytes of the signed values. */
  {
    static const struct {
      uint8_t x;
      uint8_t y;
      uint8_t want_x;
      uint8_t want_y;
    } axes[] = {{0x80u, 0x7Fu, 0x80u, 0x7Fu},
                {0x7Fu, 0x80u, 0x7Fu, 0x80u},
                {0xFFu, 0x01u, 0xFFu, 0x01u},
                {0x00u, 0x00u, 0x00u, 0x00u}};

    for (index = 0u; index < sizeof axes / sizeof axes[0]; index++) {
      t64_joybus_controller controller = {true, 0u, axes[index].x,
                                          axes[index].y};

      make_frame(frame, state, sizeof state);
      t64_joybus_parse(frame, &table);
      t64_joybus_execute(frame, &table, &controller);
      T64_CHECK_INT(frame[5], axes[index].want_x);
      T64_CHECK_INT(frame[6], axes[index].want_y);
    }
  }
}

/* ---- channels and ports ---------------------------------------------------- */

static void case_channels_and_ports(void) {
  /* Five handshakes, one per channel, each asking for the state response. */
  uint8_t bytes[36];
  uint8_t frame[64];
  t64_joybus_table table;
  size_t channel;
  t64_joybus_controller busy = {true, 0xFFFFu, 0x7Fu, 0x80u};
  t64_joybus_controller neutral = {true, 0u, 0u, 0u};

  for (channel = 0u; channel < 5u; channel++) {
    bytes[channel * 7u] = 0x01;
    bytes[channel * 7u + 1u] = 0x04;
    bytes[channel * 7u + 2u] = 0x01;
    bytes[channel * 7u + 3u] = RX_FILL;
    bytes[channel * 7u + 4u] = RX_FILL;
    bytes[channel * 7u + 5u] = RX_FILL;
    bytes[channel * 7u + 6u] = RX_FILL;
  }
  bytes[35] = 0xFE;
  /* The neutral initial latch yields 00 00 00 00 on channel 0. */
  make_frame(frame, bytes, sizeof bytes);
  t64_joybus_parse(frame, &table);
  T64_CHECK_INT(table.entry_count, 5u);
  t64_joybus_execute(frame, &table, &neutral);
  T64_CHECK_INT(frame[3], 0u);
  T64_CHECK_INT(frame[4], 0u);
  T64_CHECK_INT(frame[5], 0u);
  T64_CHECK_INT(frame[6], 0u);
  /* Channels 1 to 3 and the cartridge channel 4 answer with the no-device
     flag, count 4 | 0x80, and leave their data bytes untouched, however busy
     the controller on channel 0 is. */
  make_frame(frame, bytes, sizeof bytes);
  t64_joybus_parse(frame, &table);
  t64_joybus_execute(frame, &table, &busy);
  T64_CHECK_INT(frame[3], 0xFFu);
  T64_CHECK_INT(frame[4], 0x3Fu);
  T64_CHECK_INT(frame[5], 0x7Fu);
  T64_CHECK_INT(frame[6], 0x80u);
  for (channel = 1u; channel < 5u; channel++) {
    T64_CHECK_INT(frame[channel * 7u + 1u], 0x84u);
    T64_CHECK_INT(frame[channel * 7u + 3u], RX_FILL);
    T64_CHECK_INT(frame[channel * 7u + 4u], RX_FILL);
    T64_CHECK_INT(frame[channel * 7u + 5u], RX_FILL);
    T64_CHECK_INT(frame[channel * 7u + 6u], RX_FILL);
  }
  /* Channel 0 with no controller is also absent. */
  make_frame(frame, bytes, sizeof bytes);
  t64_joybus_parse(frame, &table);
  {
    t64_joybus_controller none = {false, 0x1234u, 1u, 2u};

    t64_joybus_execute(frame, &table, &none);
    T64_CHECK_INT(frame[1], 0x84u);
    T64_CHECK_INT(frame[3], RX_FILL);
  }
}

/* ---- the bounded campaign with an independent reference parser ------------- */

typedef struct {
  uint32_t kind;
  uint32_t channel;
  uint32_t offset;
  uint32_t tx;
  uint32_t rx;
  uint32_t flags;
} ref_entry;

typedef struct {
  uint32_t count;
  ref_entry entry[5];
} ref_table;

/* An offset-driven state machine written separately from the production
 * parser: it walks one byte at a time and keeps explicit states. */
static void ref_parse(const uint8_t *image, ref_table *out) {
  enum { WANT_FIRST, WANT_RX } state = WANT_FIRST;
  uint32_t offset = 0u;
  uint32_t channel = 0u;
  uint32_t start = 0u;
  uint32_t tx = 0u;
  uint32_t flags = 0u;
  bool stop = false;

  (void)memset(out, 0, sizeof *out);
  while (!stop && offset < 63u && channel < 5u) {
    const uint8_t byte = image[offset];

    if (state == WANT_FIRST) {
      if (byte == 0xFEu) {
        stop = true;
      } else if (byte == 0xFFu) {
        offset += 1u;
      } else if (byte == 0x00u) {
        offset += 1u;
        channel += 1u;
      } else if (byte == 0xFDu) {
        out->entry[out->count].kind = 2u;
        out->entry[out->count].channel = channel;
        out->entry[out->count].offset = offset;
        out->count += 1u;
        offset += 1u;
        channel += 1u;
      } else if ((byte & 0x3Fu) == 0u) {
        stop = true;
      } else {
        start = offset;
        tx = (uint32_t)(byte & 0x3Fu);
        flags = (uint32_t)(byte & 0xC0u);
        offset += 1u;
        state = WANT_RX;
      }
    } else {
      const uint32_t rx = (uint32_t)(byte & 0x3Fu);

      if (start + 2u + tx + rx > 63u) {
        stop = true;
      } else {
        out->entry[out->count].kind = 1u;
        out->entry[out->count].channel = channel;
        out->entry[out->count].offset = start;
        out->entry[out->count].tx = tx;
        out->entry[out->count].rx = rx;
        out->entry[out->count].flags = flags;
        out->count += 1u;
        offset = start + 2u + tx + rx;
        channel += 1u;
        state = WANT_FIRST;
      }
    }
  }
}

static uint32_t next_random(uint32_t *state) {
  uint32_t x = *state;

  x ^= x << 13u;
  x ^= x >> 17u;
  x ^= x << 5u;
  *state = x;
  return x;
}

static bool same_table(const t64_joybus_table *got, const ref_table *want) {
  uint32_t index;

  if (got->entry_count != want->count) return false;
  for (index = 0u; index < want->count; index++) {
    if (got->entries[index].kind != want->entry[index].kind ||
        got->entries[index].channel != want->entry[index].channel ||
        got->entries[index].offset != want->entry[index].offset ||
        got->entries[index].tx_count != want->entry[index].tx ||
        got->entries[index].rx_count != want->entry[index].rx ||
        got->entries[index].flags != want->entry[index].flags) {
      return false;
    }
  }
  return true;
}

static void make_campaign_frame(uint32_t *random, uint8_t frame[64],
                                size_t iteration) {
  static const uint8_t escapes[4] = {0x00u, 0xFDu, 0xFEu, 0xFFu};
  size_t index;
  const uint32_t mode = next_random(random) % 4u;

  if (mode == 0u) {
    /* Random bytes. */
    for (index = 0u; index < 64u; index++) {
      frame[index] = (uint8_t)next_random(random);
    }
  } else if (mode == 1u) {
    /* A structured vector with a few mutations. */
    static const uint8_t base[] = {0x01, 0x04, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
                                   0x01, 0x04, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
                                   0xFE};
    uint32_t mutations = 1u + next_random(random) % 4u;

    (void)memset(frame, 0, 64u);
    (void)memcpy(frame, base, sizeof base);
    while (mutations-- > 0u) {
      frame[next_random(random) % 64u] = (uint8_t)next_random(random);
    }
  } else if (mode == 2u) {
    /* All-escape runs with occasional handshake bytes. */
    for (index = 0u; index < 64u; index++) {
      const uint32_t pick = next_random(random);

      frame[index] = (pick & 0x0Fu) == 0u ? (uint8_t)(pick >> 8)
                                          : escapes[(pick >> 4) & 3u];
    }
  } else {
    /* Small counts so several entries fit. */
    for (index = 0u; index < 64u; index++) {
      const uint32_t pick = next_random(random);

      frame[index] = (uint8_t)((pick & 0xC3u) | ((pick >> 8) & 0x04u));
    }
    (void)iteration;
  }
}

static uint32_t campaign_iterations(void) {
  const char *text = getenv("T64_JOYBUS_FUZZ_ITERATIONS");
  char *end = NULL;
  unsigned long value;

  if (text == NULL || text[0] == '\0') return 20000u;
  value = strtoul(text, &end, 10);
  if (end == NULL || *end != '\0' || value == 0ul || value > 100000000ul) {
    T64_CHECK(false);
    return 20000u;
  }
  return (uint32_t)value;
}

static void case_total_bounded(void) {
  uint32_t random = UINT32_C(0x7a64c0de);
  const uint32_t iterations = campaign_iterations();
  uint32_t iteration;
  uint64_t differences = 0u;
  uint64_t bound_violations = 0u;
  uint64_t max_entries = 0u;

  for (iteration = 0u; iteration < iterations; iteration++) {
    uint8_t frame[64];
    uint8_t flipped[64];
    t64_joybus_table table;
    t64_joybus_table again;
    t64_joybus_table other;
    ref_table want;
    uint32_t index;

    make_campaign_frame(&random, frame, iteration);
    t64_joybus_parse(frame, &table);
    ref_parse(frame, &want);
    if (!same_table(&table, &want)) differences++;
    /* Repeated runs give identical results. */
    t64_joybus_parse(frame, &again);
    if (memcmp(&table, &again, sizeof table) != 0) differences++;
    /* Byte 63 is never read as a handshake byte: changing it changes
       nothing. */
    (void)memcpy(flipped, frame, 64u);
    flipped[63] = (uint8_t)(frame[63] ^ 0xFFu);
    t64_joybus_parse(flipped, &other);
    if (memcmp(&table, &other, sizeof table) != 0) differences++;
    /* Bounds: at most five entries, each handshake fits offsets 0 to 62. */
    if (table.entry_count > 5u) bound_violations++;
    if (table.entry_count > max_entries) max_entries = table.entry_count;
    for (index = 0u; index < table.entry_count && index < 5u; index++) {
      const t64_joybus_entry *entry = &table.entries[index];

      if (entry->kind == T64_JOYBUS_ENTRY_TRANSACT &&
          entry->offset + 2u + entry->tx_count + entry->rx_count > 63u) {
        bound_violations++;
      }
      if (entry->channel > 4u) bound_violations++;
    }
    /* Sampled expensive checks: plan and execute stay inside the table and
       never touch byte 63. */
    if (iteration % 100u == 0u) {
      uint8_t image[64];
      const t64_joybus_controller controller = {true, 0xA5C3u, 0x12u, 0xEDu};
      uint64_t ticks;

      (void)memcpy(image, frame, 64u);
      image[63] = 0x5Au;
      ticks = t64_joybus_plan(image, &table, true);
      t64_joybus_execute(image, &table, &controller);
      if (image[63] != 0x5Au) bound_violations++;
      if (ticks > 5u * UINT64_C(140000)) {
        bound_violations++;
      }
    }
  }
  fprintf(stderr, "joybus campaign: %u iterations, max entries %llu\n",
          (unsigned)iterations, (unsigned long long)max_entries);
  T64_CHECK_INT(differences, 0u);
  T64_CHECK_INT(bound_violations, 0u);
  /* The campaign reaches multi-entry tables, not only empty ones. */
  T64_CHECK(max_entries >= 3u);
}

typedef struct {
  const char *name;
  void (*run)(void);
} test_case;

static const test_case cases[] = {
    {"frame-vectors", case_frame_vectors},
    {"controller-responses", case_controller_responses},
    {"button-vectors", case_button_vectors},
    {"channels-and-ports", case_channels_and_ports},
    {"total-bounded", case_total_bounded},
};

int main(int argc, char **argv) {
  size_t index;
  const size_t count = sizeof cases / sizeof cases[0];

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (index = 0u; index < count; index++) puts(cases[index].name);
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--suite") == 0 &&
      strcmp(argv[2], "parser") == 0) {
    for (index = 0u; index < count; index++) {
      printf("case %s\n", cases[index].name);
      cases[index].run();
    }
    return T64_TEST_FINISH("unit.core.joybus.parser");
  }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    for (index = 0u; index < count; index++) {
      if (strcmp(cases[index].name, argv[2]) == 0) {
        cases[index].run();
        return T64_TEST_FINISH("unit.core.joybus.parser");
      }
    }
    fprintf(stderr, "unknown joybus case: %s\n", argv[2]);
    return 2;
  }
  fprintf(stderr, "usage: %s [--list-cases|--suite parser|--case NAME]\n",
          argv[0]);
  return 2;
}
