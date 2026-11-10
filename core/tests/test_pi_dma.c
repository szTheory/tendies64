#include "t64_test.h"

#include "tendies64/t64_machine.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define TEST_ROM_BYTES 4096u
#define TEST_CART_BASE UINT32_C(0x10000000)
#define TEST_PI_BASE UINT32_C(0x04600000)
#define TEST_MI_BASE UINT32_C(0x04300000)

static unsigned check_count;
static unsigned failure_count;
static const char *first_failure_name;
static uint64_t first_failure_actual;
static uint64_t first_failure_expected;

static void check_u64(const char *name, uint64_t actual, uint64_t expected) {
  bool passed = actual == expected;

  check_count++;
  if (!passed) {
    failure_count++;
    if (first_failure_name == NULL) {
      first_failure_name = name;
      first_failure_actual = actual;
      first_failure_expected = expected;
    }
  }
}

static void check_status(const char *name, t64_status actual,
                         t64_status expected) {
  check_u64(name, (uint64_t)actual, (uint64_t)expected);
}

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  void *data = NULL;

  (void)context;
  if (storage_out == NULL || size == 0u || alignment == 0u ||
      alignment > _Alignof(max_align_t)) {
    return false;
  }
  data = malloc(size);
  if (data == NULL || (uintptr_t)data % alignment != 0u) {
    free(data);
    return false;
  }
  storage_out->data = data;
  storage_out->capacity = size;
  storage_out->alignment = _Alignof(max_align_t);
  return true;
}

static void test_release(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

static void fill_authored_image(uint8_t *image, size_t length) {
  size_t i;

  for (i = 0u; i < length; i++) {
    image[i] = (uint8_t)((i * 37u + 19u) & 0xffu);
  }
  image[0] = 0x80u;
  image[1] = 0x37u;
  image[2] = 0x12u;
  image[3] = 0x40u;
  image[0x3eu] = (uint8_t)'E';
}

static void test_delayed_rom_copy(void) {
  uint8_t image[TEST_ROM_BYTES];
  const t64_host_allocator allocator = {test_allocate, test_release, NULL};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(8)};
  t64_stop_result result;
  uint32_t value = 0u;
  size_t i;

  fill_authored_image(image, sizeof image);
  check_status("PI profile lookup succeeds",
               t64_profile_lookup("ntsc-cart-4m-skeleton", &profile),
               T64_STATUS_OK);
  check_status("PI machine creation succeeds",
               t64_machine_create(&allocator, profile, &machine),
               T64_STATUS_OK);
  check_u64("PI machine is present", machine != NULL, UINT64_C(1));
  if (machine == NULL) {
    return;
  }
  check_status("PI authored ROM admission succeeds",
               t64_machine_admit_rom(machine, image, sizeof image),
               T64_STATUS_OK);

  /*
   * Source: Nintendo SDK rcp.h PI map, lines 667-767, SHA-256
   * 982f5ae01956783653d1085ef31c3d936f471f09bd1e65e27a578fae498f972f;
   * osPiRawStartDma contract, SHA-256
   * 560ef2b35b5cb96b2c175608e137d70203ab1f1e3cb4a14572935bf266f4a83b.
   * These establish register fields, 8-byte RDRAM/2-byte PI alignment and
   * maximum count. N=length+1 and 1+N/2 ticks are named skeleton policies,
   * not hardware timing. Expected bytes use this test-authored pattern.
   */
  check_status("PI DRAM address register write is accepted",
               t64_machine_bus_write(machine, TEST_PI_BASE, 4u,
                                     UINT32_C(0x100)),
               T64_STATUS_OK);
  check_status("PI cartridge address register write is accepted",
               t64_machine_bus_write(machine, TEST_PI_BASE + 4u, 4u,
                                     TEST_CART_BASE + UINT32_C(0x100)),
               T64_STATUS_OK);
  check_status("PI write length starts a ROM transfer",
               t64_machine_bus_write(machine, TEST_PI_BASE + 12u, 4u,
                                     UINT32_C(15)),
               T64_STATUS_OK);
  check_status("PI status is readable while active",
               t64_machine_bus_read(machine, TEST_PI_BASE + 16u, 4u, &value),
               T64_STATUS_OK);
  check_u64("PI is busy after accepting a 16-byte request",
            value & UINT32_C(1), UINT64_C(1));

  for (i = 0u; i < 16u; i++) {
    check_status("RDRAM byte is readable before PI completion",
                 t64_machine_bus_read(machine, UINT32_C(0x100) +
                                                  (uint32_t)i,
                                      1u, &value),
                 T64_STATUS_OK);
    check_u64("PI destination stays unchanged before completion", value,
              UINT64_C(0));
  }

  check_status("Advancement through tick 8 succeeds",
               t64_machine_advance(machine, &request, &result),
               T64_STATUS_OK);
  check_u64("PI remains busy through tick 8", result.tick, UINT64_C(8));
  check_status("PI status remains readable through tick 8",
               t64_machine_bus_read(machine, TEST_PI_BASE + 16u, 4u, &value),
               T64_STATUS_OK);
  check_u64("PI remains busy before its tick 9 deadline",
            value & UINT32_C(1), UINT64_C(1));
  for (i = 0u; i < 16u; i++) {
    check_status("RDRAM byte is readable at tick 8",
                 t64_machine_bus_read(machine, UINT32_C(0x100) +
                                                  (uint32_t)i,
                                      1u, &value),
                 T64_STATUS_OK);
    check_u64("PI destination is unchanged at tick 8", value, UINT64_C(0));
  }

  request.value = UINT64_C(9);
  check_status("Advancement through tick 9 succeeds",
               t64_machine_advance(machine, &request, &result),
               T64_STATUS_OK);
  check_u64("PI completion is at inclusive tick 9", result.tick,
            UINT64_C(9));
  check_status("PI status is readable after completion",
               t64_machine_bus_read(machine, TEST_PI_BASE + 16u, 4u, &value),
               T64_STATUS_OK);
  check_u64("PI is idle after completion", value & UINT32_C(1), UINT64_C(0));
  check_status("MI pending register is readable after PI completion",
               t64_machine_bus_read(machine, TEST_MI_BASE + 8u, 4u, &value),
               T64_STATUS_OK);
  check_u64("PI completion sets pending PI independent of its mask",
            value & UINT32_C(0x10), UINT64_C(0x10));
  for (i = 0u; i < 16u; i++) {
    check_status("RDRAM byte is readable after PI completion",
                 t64_machine_bus_read(machine, UINT32_C(0x100) +
                                                  (uint32_t)i,
                                      1u, &value),
                 T64_STATUS_OK);
    check_u64("PI completion copies independently authored ROM bytes", value,
              image[0x100u + i]);
  }

  t64_machine_destroy(machine);
}

int main(void) {
  bool passed;

  (void)printf("TAP version 13\n");
  test_delayed_rom_copy();
  passed = failure_count == 0u && check_count > 0u;
  (void)printf("%s 1 - PI transfers 16 authored bytes at tick 9 with pending IRQ\n",
               passed ? "ok" : "not ok");
  if (!passed) {
    (void)printf("# failed assertion: %s\n# expected: %llu\n# actual: %llu\n",
                 first_failure_name,
                 (unsigned long long)first_failure_expected,
                 (unsigned long long)first_failure_actual);
  }
  (void)printf("1..1\n");
  return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
