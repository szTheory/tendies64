#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_internal.h"
#include "t64_test.h"
#include "tendies64/t64_machine.h"

#define DEPTH_PROFILE_ID \
  "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-depth-v7"
#define TEST_ROM_CAPACITY 65536u
#define COMMAND_ADDRESS UINT32_C(0x100)
#define FRAMEBUFFER_ADDRESS UINT32_C(0x5000)
#define DEPTH_IMAGE_ADDRESS UINT32_C(0x6000)
#define DP_START UINT32_C(0x04100000)
#define DP_END (DP_START + UINT32_C(4))

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

static t64_machine *create_depth_machine(void) {
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
  T64_CHECK_INT(t64_profile_lookup(DEPTH_PROFILE_ID, &profile),
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

static void write_be16(uint8_t *bytes, uint16_t value) {
  bytes[0] = (uint8_t)(value >> 8);
  bytes[1] = (uint8_t)value;
}

static void write_be64(uint8_t *bytes, uint64_t value) {
  uint32_t i;
  for (i = 0u; i < 8u; i++)
    bytes[i] = (uint8_t)(value >> (56u - i * 8u));
}

static bool dispatch_all(t64_machine *machine) {
  size_t step;

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

static bool run_commands(t64_machine *machine, const uint64_t *words,
                         size_t count) {
  size_t i;
  uint32_t end;

  if (machine == NULL || words == NULL || count == 0u ||
      count > (UINT32_MAX - COMMAND_ADDRESS) / 8u)
    return false;
  machine->queue_head = 0u;
  machine->queue_count = 0u;
  for (i = 0u; i < count; i++)
    write_be64(machine->ram + COMMAND_ADDRESS + i * 8u, words[i]);
  end = COMMAND_ADDRESS + (uint32_t)(count * 8u);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_START, 4u,
                                      COMMAND_ADDRESS), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_END, 4u, end),
                T64_STATUS_OK);
  return dispatch_all(machine);
}

static void case_primitive_depth_raw_packet_accepted_effect(void) {
  static const uint64_t commands[] = {
      UINT64_C(0x3f10000700005000), /* RGBA16 color image, width 8 */
      UINT64_C(0x2d00000000020020), /* scissor [0,8) x [0,8) */
      UINT64_C(0x2f00000000000034), /* primitive Z, compare, update */
      UINT64_C(0x3e00000000006000), /* aligned byte-addressed Z image */
      UINT64_C(0x2e00000013570009), /* primary field literal: Z=0x1357,dZ=9 */
      UINT64_C(0x3a00000000ff00ff), /* green primitive color, opaque alpha */
      UINT64_C(0xfc00000000018600), /* literal combiner selects primitive */
      UINT64_C(0x09000014000c0004), /* Edge W0: YL=5, YM=3, YH=1 */
      UINT64_C(0x00010000fffe8000), /* Edge W1: XL=1, DxLDy=-1.5 */
      UINT64_C(0x0001000000000000), /* Edge W2: XH=1, DxHDy=0 */
      UINT64_C(0x0004000000018000), /* Edge W3: XM=4, DxMDy=+1.5 */
      UINT64_C(0x0000000000000000), /* Z W4 and W5: literal zero plane */
      UINT64_C(0x0000000000000000),
  };
  static const uint8_t expected_color[] = {0x07u, 0xc1u};
  static const uint8_t expected_depth[] = {0x09u, 0xa8u};
  t64_machine *machine = create_depth_machine();
  uint32_t pixel_address = FRAMEBUFFER_ADDRESS + (2u * 8u + 1u) * 2u;
  uint32_t depth_address = DEPTH_IMAGE_ADDRESS + (2u * 8u + 1u) * 2u;

  if (machine == NULL) return;
  T64_CHECK_INT((commands[2] & UINT64_C(0x3f)), UINT64_C(0x34));
  T64_CHECK_INT((commands[4] >> 16) & UINT64_C(0xffff), UINT64_C(0x1357));
  T64_CHECK_INT(commands[4] & UINT64_C(0xffff), UINT64_C(9));
  for (uint32_t i = 0u; i < 8u * 8u; i++)
    write_be16(machine->ram + DEPTH_IMAGE_ADDRESS + i * 2u,
               UINT16_C(0xfffc));
  memset(machine->ram + FRAMEBUFFER_ADDRESS, 0xa5, 8u * 8u * 2u);
  T64_CHECK(run_commands(machine, commands,
                         sizeof commands / sizeof commands[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT(machine->rdp.last_opcode_field, UINT32_C(0x09));
  T64_CHECK(memcmp(machine->ram + pixel_address, expected_color,
                   sizeof expected_color) == 0);
  /* Independent software oracle: 0x1357*8=0x9ab8, exponent=0,
   * floor(0x9ab8/64)=0x26a, visible=0x09a8, delta code=3. */
  T64_CHECK(memcmp(machine->ram + depth_address, expected_depth,
                   sizeof expected_depth) == 0);
  T64_CHECK_INT(machine->rdp_depth_hidden[depth_address / 2u], UINT8_C(0x83));
  t64_machine_destroy(machine);
}

typedef struct {
  const char *name;
  void (*run)(void);
} test_case;

static const test_case cases[] = {
    {"z-depth-primitive-accepted-raw-vi",
     case_primitive_depth_raw_packet_accepted_effect},
};

int main(int argc, char **argv) {
  size_t i;

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) puts(cases[i].name);
    return 0;
  }
  if (argc != 1) return 2;
  for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) cases[i].run();
  return T64_TEST_FINISH("unit.core.rdp.depth");
}
