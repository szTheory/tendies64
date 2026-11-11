#include "t64_test.h"

#include "t64_internal.h"
#include "tendies64/t64_machine.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define TEST_IPL3_BYTES 4096u
#define TEST_HELLO_BYTES 4280u
#define TEST_RDRAM_BYTES UINT32_C(0x00400000)

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  void *memory;

  (void)context;
  if (storage_out == NULL || size == 0u || alignment == 0u) {
    return false;
  }
  memory = malloc(size);
  if (memory == NULL || ((uintptr_t)memory % alignment) != 0u) {
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

static bool read_exact(const char *environment_name, uint8_t *bytes,
                       size_t length) {
  const char *path = getenv(environment_name);
  FILE *file;
  size_t actual;
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
  actual = fread(bytes, 1u, length, file);
  extra = fgetc(file);
  read_error = ferror(file);
  close_error = fclose(file);
  return actual == length && extra == EOF && read_error == 0 &&
         close_error == 0;
}

int main(void) {
  uint8_t source_asset[TEST_IPL3_BYTES];
  uint8_t authored_rom[TEST_HELLO_BYTES];
  const t64_host_allocator allocator = {test_allocate, test_release, NULL};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint32_t value = UINT32_C(0xdecafbad);
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(100)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  bool image_ok;
  bool advanced_past_device;

  puts("TAP version 13");
  image_ok = read_exact("T64_TEST_IPL3_PATH", source_asset,
                        sizeof source_asset) &&
             read_exact("T64_TEST_HELLO_PATH", authored_rom,
                        sizeof authored_rom);
  T64_CHECK(image_ok);
  T64_CHECK_INT(t64_profile_lookup("ntsc-cart-4m-openipl3-warm-v1", &profile),
                T64_STATUS_OK);
  T64_CHECK(profile != NULL);
  if (profile == NULL || !image_ok) {
    puts("not ok 1 - qualified warm-device contract admits authored ROM and executes RI path");
    puts("1..1");
    return 1;
  }

  T64_CHECK_INT(profile->rom_max_bytes, UINT32_C(1048576));
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  T64_CHECK(machine != NULL);
  if (machine == NULL) {
    puts("not ok 1 - qualified warm-device contract admits authored ROM and executes RI path");
    puts("1..1");
    return 1;
  }

  T64_CHECK_INT(t64_machine_admit_rom(machine, authored_rom,
                                      sizeof authored_rom),
                T64_STATUS_OK);
  if (!machine->has_image) {
    T64_CHECK_INT(t64_machine_admit_rom(machine, source_asset,
                                        sizeof source_asset),
                  T64_STATUS_OK);
  }
  T64_CHECK(machine->has_image);

  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04300004), 4u,
                                     &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, 0u);
  value = UINT32_C(0xdecafbad);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x0470000c), 4u,
                                     &value),
                T64_STATUS_OK);
  T64_CHECK(value != 0u);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x0470000d), 1u,
                                     &value),
                T64_STATUS_BUS_UNSUPPORTED);
  value = UINT32_C(0xdecafbad);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04700010), 4u,
                                     &value),
                T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(value, UINT32_C(0xdecafbad));
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x04300004), 4u, 1u),
                T64_STATUS_BUS_UNSUPPORTED);

  T64_CHECK_INT(t64_machine_bus_write(machine, TEST_RDRAM_BYTES - 1u, 1u,
                                      UINT32_C(0xa5)),
                T64_STATUS_OK);
  value = 0u;
  T64_CHECK_INT(t64_machine_bus_read(machine, TEST_RDRAM_BYTES - 1u, 1u,
                                     &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, UINT32_C(0xa5));
  value = UINT32_C(0xdecafbad);
  T64_CHECK_INT(t64_machine_bus_read(machine, TEST_RDRAM_BYTES, 1u, &value),
                T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(value, UINT32_C(0xdecafbad));

  T64_CHECK_INT(t64_machine_advance(machine, &request, &result),
                T64_STATUS_OK);
  advanced_past_device = result.reason == T64_STOP_REASON_CPU_UNSUPPORTED &&
                         result.tick > UINT64_C(38) &&
                         machine->cpu.retired_instructions > UINT64_C(37);
  T64_CHECK(advanced_past_device);

  puts(t64_test_failures == 0
           ? "ok 1 - qualified warm-device contract admits authored ROM and executes RI path"
           : "not ok 1 - qualified warm-device contract admits authored ROM and executes RI path");
  puts("1..1");
  t64_machine_destroy(machine);
  return t64_test_failures == 0 ? 0 : 1;
}
