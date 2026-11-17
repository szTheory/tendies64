#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_internal.h"
#include "t64_test.h"
#include "tendies64/t64_machine.h"

#define TRIANGLE_PROFILE_ID \
  "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-triangle-v3"
#define TEST_ROM_CAPACITY 65536u
#define COMMAND_ADDRESS UINT32_C(0x100)
#define FRAMEBUFFER_ADDRESS UINT32_C(0x5000)
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
  T64_CHECK_INT(t64_profile_lookup(TRIANGLE_PROFILE_ID, &profile),
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
  T64_CHECK(machine->queue_count == 0u);
  return machine->queue_count == 0u;
}

static bool run_commands(t64_machine *machine, const uint64_t *words,
                         size_t count) {
  size_t i;
  uint32_t end;
  if (machine == NULL || words == NULL || count == 0u ||
      count > (UINT32_MAX - COMMAND_ADDRESS) / 8u) return false;
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

static uint32_t framebuffer_pixel(uint32_t x, uint32_t y) {
  return FRAMEBUFFER_ADDRESS + (y * 8u + x) * 2u;
}

static void case_texture_perspective_nonzero_signed_q16(void) {
  static const uint64_t commands[] = {
      UINT64_C(0x3f10000700005000), /* RGBA16 color image, width 8 */
      UINT64_C(0x2d00000000020020), /* scissor [0,8) x [0,8) */
      UINT64_C(0x2f080c0000000000), /* perspective, point, one cycle */
      UINT64_C(0x3510040000080200), /* tile 0: RGBA16, 2 words/row, clamp */
      UINT64_C(0x320000000000701c), /* tile bounds: [0,8) x [0,8) */
      UINT64_C(0x0a80002000100000), /* edge + texture, YL=8,YM=4,YH=0 */
      UINT64_C(0x0000000000000000), /* XL=0, dXL/dy=0 */
      UINT64_C(0x0004000000000000), /* XH=4, dXH/dy=0 */
      UINT64_C(0x00000000ffff0000), /* XM=0, dXM/dy=-1 */
      UINT64_C(0x0006fffe00020000), /* S=6, T=-2, W=2 */
      UINT64_C(0x0000000000000000),
      UINT64_C(0x0000000000000000),
      UINT64_C(0x8000400040000000), /* dS/dx=.5,dT/dx=.25,dW/dx=.25 */
      UINT64_C(0x0000000000000000),
      UINT64_C(0x0000000000000000),
      UINT64_C(0x0000000000000000),
      UINT64_C(0x0000000000000000),
  };
  static const uint8_t red[] = {0xf8u, 0x01u};
  static const uint8_t untouched[] = {0xa5u, 0xa5u};
  const uint64_t raw_w0 = UINT64_C(0x0a80002000100000);
  const uint64_t raw_w4 = UINT64_C(0x0006fffe00020000);
  const uint64_t raw_w7 = UINT64_C(0x8000400040000000);
  t64_machine *machine = create_authored_machine();
  uint32_t inside = framebuffer_pixel(1u, 4u);
  uint32_t outside = framebuffer_pixel(6u, 4u);

  if (machine == NULL) return;
  memset(machine->ram + FRAMEBUFFER_ADDRESS, 0xa5, 8u * 8u * 2u);
  machine->rdp.tmem[6] = 0xf8u;  /* independent logical texel (3,0) */
  machine->rdp.tmem[7] = 0x01u;
  machine->rdp.tmem[8] = 0x07u;  /* independent logical texel (4,0) */
  machine->rdp.tmem[9] = 0xc1u;
  machine->rdp.tmem[122] = 0x00u; /* logical texel (3,7), odd bank */
  machine->rdp.tmem[123] = 0x3fu;
  machine->rdp.tmem[124] = 0xffu; /* logical texel (4,7), odd bank */
  machine->rdp.tmem[125] = 0xffu;

  T64_CHECK_INT((raw_w0 >> 56) & UINT64_C(0x3f), UINT64_C(0x0a));
  T64_CHECK_INT((raw_w0 >> 55) & UINT64_C(1), 1u);
  T64_CHECK_INT((raw_w0 >> 32) & UINT64_C(0x3fff), 32u);
  T64_CHECK_INT((raw_w0 >> 16) & UINT64_C(0x3fff), 16u);
  T64_CHECK_INT(raw_w0 & UINT64_C(0x3fff), 0u);
  T64_CHECK_INT((raw_w4 >> 32) & UINT64_C(0xffff), UINT64_C(0xfffe));
  T64_CHECK_INT((raw_w4 >> 16) & UINT64_C(0xffff), 2u);
  T64_CHECK_INT((raw_w7 >> 48) & UINT64_C(0xffff), UINT64_C(0x8000));
  T64_CHECK_INT((raw_w7 >> 32) & UINT64_C(0xffff), UINT64_C(0x4000));
  T64_CHECK_INT((raw_w7 >> 16) & UINT64_C(0xffff), UINT64_C(0x4000));

  T64_CHECK(run_commands(machine, commands,
                         sizeof commands / sizeof commands[0]));
  T64_CHECK_INT(machine->rdp.last_opcode_field, UINT32_C(0x0a));
  T64_CHECK(machine->rdp.last_original_word == raw_w0);
  T64_CHECK_INT(machine->rdp.dpc_current, COMMAND_ADDRESS + UINT32_C(40));
  T64_CHECK_INT(machine->rdp.refusal_reason,
                T64_RDP_REFUSAL_UNSUPPORTED_OPCODE);
  T64_CHECK(memcmp(machine->ram + inside, red, sizeof red) == 0);
  T64_CHECK(memcmp(machine->ram + outside, untouched, sizeof untouched) == 0);
  t64_machine_destroy(machine);
}

typedef struct {
  const char *name;
  void (*run)(void);
} test_case;

static const test_case cases[] = {
    {"texture-perspective-nonzero-signed-q16",
     case_texture_perspective_nonzero_signed_q16},
};

int main(int argc, char **argv) {
  size_t i;
  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) puts(cases[i].name);
    return 0;
  }
  if (argc != 1) return 2;
  for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) cases[i].run();
  return T64_TEST_FINISH("unit.core.rdp.texture");
}
