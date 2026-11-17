#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_internal.h"
#include "tendies64/t64_machine.h"

static unsigned test_checks;
static unsigned test_failures;

#define T64_CHECK(expression)                                                  \
  do {                                                                         \
    bool t64_ok_ = (expression);                                               \
    test_checks++;                                                             \
    if (!t64_ok_) {                                                            \
      test_failures++;                                                         \
      printf("# assertion failed: %s\n", #expression);                        \
    }                                                                          \
  } while (0)

#define T64_CHECK_INT(got, want)                                               \
  do {                                                                         \
    long long t64_got_ = (long long)(got);                                     \
    long long t64_want_ = (long long)(want);                                   \
    test_checks++;                                                             \
    if (t64_got_ != t64_want_) {                                               \
      test_failures++;                                                         \
      printf("# assertion failed: %s got %lld want %lld\n", #got,             \
             t64_got_, t64_want_);                                             \
    }                                                                          \
  } while (0)

#define COMBINER_PROFILE_ID \
  "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-combiner-v5"
#define TEST_ROM_CAPACITY 65536u
#define COMMAND_ADDRESS UINT32_C(0x100)
#define FRAMEBUFFER_ADDRESS UINT32_C(0x5000)
#define DP_START UINT32_C(0x04100000)
#define DP_END (DP_START + UINT32_C(4))
#define VI_BASE UINT32_C(0x04400000)

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  void *memory;
  (void)context;
  if (storage_out == NULL || alignment == 0u) return false;
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

static t64_machine *create_authored_machine(void) {
  const t64_host_allocator allocator = {test_allocate, test_release, NULL};
  const char *path = getenv("T64_TEST_RDP_FILL_PATH");
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t rom[TEST_ROM_CAPACITY];
  FILE *file;
  long length;

  T64_CHECK(path != NULL && path[0] != '\0');
  if (path == NULL || path[0] == '\0') return NULL;
  file = fopen(path, "rb");
  T64_CHECK(file != NULL);
  if (file == NULL) return NULL;
  if (fseek(file, 0L, SEEK_END) != 0 || (length = ftell(file)) <= 0 ||
      (unsigned long)length > sizeof rom || fseek(file, 0L, SEEK_SET) != 0) {
    (void)fclose(file);
    T64_CHECK(false);
    return NULL;
  }
  T64_CHECK(fread(rom, 1u, (size_t)length, file) == (size_t)length);
  T64_CHECK(fclose(file) == 0);
  T64_CHECK_INT(t64_profile_lookup(COMBINER_PROFILE_ID, &profile),
                T64_STATUS_OK);
  T64_CHECK(profile != NULL);
  if (profile == NULL) return NULL;
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  if (machine == NULL) return NULL;
  T64_CHECK_INT(t64_machine_admit_rom(machine, rom, (size_t)length),
                T64_STATUS_OK);
  return machine;
}

static void write_be64(uint8_t *bytes, uint64_t value) {
  uint32_t i;
  for (i = 0u; i < 8u; i++)
    bytes[i] = (uint8_t)(value >> (56u - i * 8u));
}

static bool run_commands(t64_machine *machine, const uint64_t *words,
                         size_t count) {
  size_t step;
  uint32_t end;
  if (machine == NULL || words == NULL || count == 0u ||
      count > (UINT32_MAX - COMMAND_ADDRESS) / 8u) return false;
  machine->queue_head = 0u;
  machine->queue_count = 0u;
  for (step = 0u; step < count; step++)
    write_be64(machine->ram + COMMAND_ADDRESS + step * 8u, words[step]);
  end = COMMAND_ADDRESS + (uint32_t)(count * 8u);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_START, 4u, COMMAND_ADDRESS),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_END, 4u, end),
                T64_STATUS_OK);
  for (step = 0u; step < 4096u && machine->queue_count != 0u; step++) {
    t64_event event = machine->events[machine->queue_head];
    machine->queue_head = (machine->queue_head + 1u) %
                          T64_EVENT_QUEUE_CAPACITY;
    machine->queue_count--;
    machine->tick = event.tick;
    T64_CHECK(event.handler != NULL);
    if (event.handler != NULL)
      T64_CHECK_INT(event.handler(machine, &event), T64_STATUS_OK);
  }
  T64_CHECK_INT(machine->queue_count, 0u);
  return machine->queue_count == 0u;
}

static bool capture_raw_vi(t64_machine *machine, uint8_t *bytes,
                           size_t capacity, size_t *length_out) {
  static const struct {
    uint32_t offset;
    uint32_t value;
  } writes[] = {
      {UINT32_C(0x04), FRAMEBUFFER_ADDRESS},
      {UINT32_C(0x08), 8u},
      {UINT32_C(0x1c), 3u},
      {UINT32_C(0x18), 0u},
      {UINT32_C(0x24), 8u},
      {UINT32_C(0x28), 8u},
      {UINT32_C(0x30), UINT32_C(1024)},
      {UINT32_C(0x34), UINT32_C(1024)},
      {UINT32_C(0x00), UINT32_C(2)},
  };
  t64_video_packet_info info = {0};
  size_t i, step;

  for (i = 0u; i < sizeof writes / sizeof writes[0]; i++)
    T64_CHECK_INT(t64_machine_bus_write(machine, VI_BASE + writes[i].offset,
                                        4u, writes[i].value),
                  T64_STATUS_OK);
  for (step = 0u; step < 32u && machine->vi.packet_count == 0u; step++) {
    t64_event event;
    if (machine->queue_count == 0u) break;
    event = machine->events[machine->queue_head];
    machine->queue_head = (machine->queue_head + 1u) %
                          T64_EVENT_QUEUE_CAPACITY;
    machine->queue_count--;
    machine->tick = event.tick;
    T64_CHECK(event.handler != NULL);
    if (event.handler != NULL)
      T64_CHECK_INT(event.handler(machine, &event), T64_STATUS_OK);
  }
  T64_CHECK(machine->vi.packet_count > 0u);
  if (machine->vi.packet_count == 0u) return false;
  T64_CHECK_INT(t64_video_info(machine, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.width, 8u);
  T64_CHECK_INT(info.height, 8u);
  T64_CHECK(info.packet_bytes <= capacity);
  if (info.packet_bytes > capacity) return false;
  T64_CHECK_INT(t64_video_copy(machine, bytes, capacity, length_out),
                T64_STATUS_OK);
  return true;
}

static void case_one_cycle_raw_dpc_vi(void) {
  static const uint64_t commands[] = {
      UINT64_C(0x3f10000700005000), /* RGBA16 color image, width 8 */
      UINT64_C(0x2d00000000020020), /* scissor [0,8) x [0,8) */
      UINT64_C(0x2f00000000000000), /* explicitly select one-cycle mode */
      UINT64_C(0x3a000000c8c8c8ff), /* primitive RGBA=(200,200,200,255) */
      UINT64_C(0x3b000000505050ff), /* environment RGBA=(80,80,80,255) */
      UINT64_C(0xfc55566c8877ffff), /* independent SetCombine literal */
      UINT64_C(0x08000014000c0004), /* Edge: (1,1), (4,3), (1,5) */
      UINT64_C(0x00010000fffe8000),
      UINT64_C(0x0001000000000000),
      UINT64_C(0x0004000000018000),
  };
  uint8_t raw_vi[8u * 8u * 4u];
  size_t raw_vi_length = 0u;
  const uint64_t combine = UINT64_C(0xfc55566c8877ffff);
  t64_machine *machine = create_authored_machine();
  const size_t pixel_address = FRAMEBUFFER_ADDRESS + (2u * 8u + 1u) * 2u;
  const size_t vi_pixel_offset = (2u * 8u + 1u) * 4u;

  if (machine == NULL) return;
  memset(machine->ram + FRAMEBUFFER_ADDRESS, 0xa5, 8u * 8u * 2u);
  T64_CHECK_INT((combine >> 56) & UINT64_C(0x3f), UINT64_C(0x3c));
  T64_CHECK(combine == commands[5]);
  T64_CHECK(run_commands(machine, commands,
                         sizeof commands / sizeof commands[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT(machine->rdp.last_opcode_field, UINT32_C(0x08));
  T64_CHECK(machine->rdp.last_original_word == commands[6]);
  T64_CHECK_INT(machine->rdp.triangle_count, 1u);
  T64_CHECK_INT(((uint32_t)machine->ram[pixel_address] << 8) |
                    machine->ram[pixel_address + 1u],
                UINT32_C(0x5295));
  T64_CHECK(capture_raw_vi(machine, raw_vi, sizeof raw_vi, &raw_vi_length));
  T64_CHECK_INT(raw_vi_length, sizeof raw_vi);
  T64_CHECK_INT(raw_vi[vi_pixel_offset], 82u);
  T64_CHECK_INT(raw_vi[vi_pixel_offset + 1u], 82u);
  T64_CHECK_INT(raw_vi[vi_pixel_offset + 2u], 82u);
  T64_CHECK_INT(raw_vi[vi_pixel_offset + 3u], 255u);
  t64_machine_destroy(machine);
}

typedef struct {
  const char *name;
  void (*run)(void);
} test_case;

static const test_case cases[] = {
    {"combiner-one-cycle-raw-dpc-vi", case_one_cycle_raw_dpc_vi},
};

int main(int argc, char **argv) {
  size_t i;
  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) puts(cases[i].name);
    return 0;
  }
  if (argc != 1) return 2;
  printf("TAP version 13\n1..%lu\n", (unsigned long)(sizeof cases /
                                                      sizeof cases[0]));
  for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) {
    unsigned failures_before = test_failures;
    cases[i].run();
    printf("%s %lu - %s\n", test_failures == failures_before ? "ok" : "not ok",
           (unsigned long)(i + 1u), cases[i].name);
  }
  return test_failures == 0u && test_checks > 0u ? 0 : 1;
}
