#include "t64_internal.h"
#include "tendies64/t64_machine.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define TEST_IPL3_BYTES 4096u

typedef struct {
  size_t outstanding;
  t64_host_allocator allocator;
  t64_machine *machine;
} fixture;

static unsigned failure_count;

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  fixture *value = (fixture *)context;
  void *memory;

  if (value == NULL || storage_out == NULL || size == 0u || alignment == 0u) {
    return false;
  }
  memory = malloc(size);
  if (memory == NULL || (uintptr_t)memory % alignment != 0u) {
    free(memory);
    return false;
  }
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = alignment;
  value->outstanding++;
  return true;
}

static void test_release(void *context, t64_host_storage storage) {
  fixture *value = (fixture *)context;

  if (value != NULL && storage.data != NULL) {
    free(storage.data);
    value->outstanding--;
  }
}

static bool read_qualified_asset(uint8_t bytes[TEST_IPL3_BYTES]) {
  const char *path = getenv("T64_TEST_IPL3_PATH");
  FILE *file;
  size_t length;
  int extra;
  int read_error;
  int close_error;

  if (path == NULL || path[0] == '\0' || bytes == NULL) {
    return false;
  }
  file = fopen(path, "rb");
  if (file == NULL) {
    return false;
  }
  length = fread(bytes, 1u, TEST_IPL3_BYTES, file);
  extra = fgetc(file);
  read_error = ferror(file);
  close_error = fclose(file);
  return read_error == 0 && close_error == 0 &&
         length == TEST_IPL3_BYTES && extra == EOF;
}

static bool fixture_init(fixture *value,
                         const uint8_t asset[TEST_IPL3_BYTES]) {
  const t64_profile_descriptor *profile = NULL;

  if (value == NULL || asset == NULL) {
    return false;
  }
  *value = (fixture){0};
  value->allocator.allocate = test_allocate;
  value->allocator.release = test_release;
  value->allocator.context = value;
  if (t64_profile_lookup("ntsc-cart-4m-openipl3-vr4300-v1", &profile) !=
          T64_STATUS_OK ||
      profile == NULL ||
      t64_machine_create(&value->allocator, profile, &value->machine) !=
          T64_STATUS_OK ||
      value->machine == NULL ||
      t64_machine_admit_rom(value->machine, asset, TEST_IPL3_BYTES) !=
          T64_STATUS_OK) {
    if (value->machine != NULL) {
      t64_machine_destroy(value->machine);
      value->machine = NULL;
    }
    return false;
  }
  return true;
}

static void fixture_destroy(fixture *value) {
  if (value != NULL && value->machine != NULL) {
    t64_machine_destroy(value->machine);
    value->machine = NULL;
  }
}

static void write_instruction(t64_machine *machine, size_t index,
                              uint32_t opcode) {
  size_t offset = UINT32_C(0x1000) + index * 4u;

  machine->ram[offset] = (uint8_t)(opcode >> 24);
  machine->ram[offset + 1u] = (uint8_t)(opcode >> 16);
  machine->ram[offset + 2u] = (uint8_t)(opcode >> 8);
  machine->ram[offset + 3u] = (uint8_t)opcode;
}

static void start_kernel_program(t64_machine *machine) {
  machine->cpu.pc = UINT32_C(0x80001000);
  machine->cpu.pc_full = UINT64_C(0xffffffff80001000);
}

static void check_bool(const char *name, bool actual, bool expected) {
  if (actual != expected) {
    (void)printf("# %s expected=%u actual=%u\n", name,
                 expected ? 1u : 0u, actual ? 1u : 0u);
    failure_count++;
  }
}

static void test_targeted_dcache_invalidation(
    const uint8_t asset[TEST_IPL3_BYTES]) {
  static const uint32_t program[] = {
      UINT32_C(0xc0880000), /* ll t0,0(a0) */
      UINT32_C(0xbcb10000), /* cache 0x11,0(a1): hit invalidate D */
      UINT32_C(0xe0890000), /* sc t1,0(a0) */
  };
  fixture value;
  t64_status status;
  size_t index;

  if (!fixture_init(&value, asset)) {
    (void)puts("# D-cache invalidation fixture setup failed");
    failure_count++;
    return;
  }
  for (index = 0u; index < sizeof program / sizeof program[0]; index++) {
    write_instruction(value.machine, index, program[index]);
  }
  value.machine->cpu.gpr[4] = UINT64_C(0xffffffff80002004);
  value.machine->cpu.gpr[5] = UINT64_C(0xffffffff8000200c);
  value.machine->cpu.gpr[9] = UINT64_C(0xdeadbeef);
  value.machine->ram[UINT32_C(0x2004)] = UINT8_C(0x11);
  value.machine->ram[UINT32_C(0x2005)] = UINT8_C(0x22);
  value.machine->ram[UINT32_C(0x2006)] = UINT8_C(0x33);
  value.machine->ram[UINT32_C(0x2007)] = UINT8_C(0x44);
  start_kernel_program(value.machine);

  status = t64_cpu_step(value.machine, NULL);
  if (status != T64_STATUS_OK || !value.machine->cpu.linked_valid ||
      value.machine->cpu.linked_physical_address != UINT32_C(0x2004) ||
      value.machine->cpu.linked_width != UINT32_C(4)) {
    (void)printf("# LL fixture expected status=0 linked=[0x2004,0x2008) "
                 "observed status=%u linked=%u address=0x%08x width=%u\n",
                 (unsigned)status, value.machine->cpu.linked_valid ? 1u : 0u,
                 value.machine->cpu.linked_physical_address,
                 value.machine->cpu.linked_width);
    failure_count++;
    fixture_destroy(&value);
    return;
  }
  status = t64_cpu_step(value.machine, NULL);
  if (status != T64_STATUS_OK) {
    (void)printf("# CACHE hit invalidate D expected status=0 observed=%u\n",
                 (unsigned)status);
    failure_count++;
    fixture_destroy(&value);
    return;
  }
  check_bool("targeted 16-byte D-line invalidation clears overlapping link",
             value.machine->cpu.linked_valid, false);

  status = t64_cpu_step(value.machine, NULL);
  if (status != T64_STATUS_OK || value.machine->cpu.gpr[9] != 0u ||
      value.machine->ram[UINT32_C(0x2004)] != UINT8_C(0x11) ||
      value.machine->ram[UINT32_C(0x2005)] != UINT8_C(0x22) ||
      value.machine->ram[UINT32_C(0x2006)] != UINT8_C(0x33) ||
      value.machine->ram[UINT32_C(0x2007)] != UINT8_C(0x44)) {
    (void)printf("# SC after targeted D-line invalidation status=%u rt=%016llx "
                 "bytes=%02x%02x%02x%02x\n",
                 (unsigned)status,
                 (unsigned long long)value.machine->cpu.gpr[9],
                 value.machine->ram[UINT32_C(0x2004)],
                 value.machine->ram[UINT32_C(0x2005)],
                 value.machine->ram[UINT32_C(0x2006)],
                 value.machine->ram[UINT32_C(0x2007)]);
    failure_count++;
  }
  fixture_destroy(&value);
}

int main(void) {
  uint8_t asset[TEST_IPL3_BYTES];

  (void)puts("1..1");
  if (!read_qualified_asset(asset)) {
    (void)puts("not ok 1 - qualified IPL3 fixture");
    return 1;
  }
  test_targeted_dcache_invalidation(asset);
  if (failure_count == 0u) {
    (void)puts("ok 1 - targeted D-cache invalidation breaks overlapping link");
  } else {
    (void)puts("not ok 1 - targeted D-cache invalidation breaks overlapping link");
  }
  return failure_count == 0u ? 0 : 1;
}
