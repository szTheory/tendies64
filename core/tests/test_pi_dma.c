#include "t64_test.h"

#include "t64_internal.h"
#include "tendies64/t64_machine.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

  if (context != NULL) {
    (*(size_t *)context)++;
  }
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
  if (context != NULL) {
    (*(size_t *)context)++;
  }
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

static bool create_test_machine(t64_machine **machine_out, uint8_t *image,
                                size_t image_length) {
  const t64_host_allocator allocator = {test_allocate, test_release, NULL};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  t64_status status;

  fill_authored_image(image, image_length);
  status = t64_profile_lookup("ntsc-cart-4m-skeleton", &profile);
  check_status("PI fixture profile lookup succeeds", status, T64_STATUS_OK);
  if (status != T64_STATUS_OK || profile == NULL) {
    return false;
  }
  status = t64_machine_create(&allocator, profile, &machine);
  check_status("PI fixture machine creation succeeds", status,
               T64_STATUS_OK);
  if (status != T64_STATUS_OK || machine == NULL) {
    return false;
  }
  status = t64_machine_admit_rom(machine, image, image_length);
  check_status("PI fixture ROM admission succeeds", status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) {
    t64_machine_destroy(machine);
    return false;
  }
  *machine_out = machine;
  return true;
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
   * maximum count. N=length+1, odd-length rejection, busy-start ERROR and
   * 1+N/2 ticks are named skeleton policies/inferences, not hardware timing.
   * Expected bytes use this test-authored pattern.
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

static void test_profile_pi_duration_controls_completion_tick(void) {
  uint8_t image[TEST_ROM_BYTES];
  const t64_host_allocator allocator = {test_allocate, test_release, NULL};
  const t64_profile_descriptor *base_profile = NULL;
  t64_profile_descriptor profile;
  t64_machine *machine = NULL;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(6)};
  t64_stop_result result = {0u, 0u};
  uint32_t value = 0u;

  fill_authored_image(image, sizeof image);
  check_status("PI timing profile lookup succeeds",
               t64_profile_lookup("ntsc-cart-4m-skeleton", &base_profile),
               T64_STATUS_OK);
  if (base_profile == NULL) {
    return;
  }
  profile = *base_profile;
  profile.pi_duration_base_ticks = 5u;
  profile.pi_duration_bytes_divisor = 1u;
  check_status("PI timing profile machine creation succeeds",
               t64_machine_create(&allocator, &profile, &machine),
               T64_STATUS_OK);
  if (machine == NULL) {
    return;
  }
  check_status("PI timing profile ROM admission succeeds",
               t64_machine_admit_rom(machine, image, sizeof image),
               T64_STATUS_OK);
  check_status("PI timing profile DRAM address setup succeeds",
               t64_machine_bus_write(machine, TEST_PI_BASE, 4u, 0x100u),
               T64_STATUS_OK);
  check_status("PI timing profile cartridge address setup succeeds",
               t64_machine_bus_write(machine, TEST_PI_BASE + 4u, 4u,
                                     TEST_CART_BASE + 0x100u),
               T64_STATUS_OK);
  check_status("PI timing profile accepts a two-byte request",
               t64_machine_bus_write(machine, TEST_PI_BASE + 12u, 4u, 1u),
               T64_STATUS_OK);
  check_status("PI timing profile advances through tick 6",
               t64_machine_advance(machine, &request, &result), T64_STATUS_OK);
  check_status("PI status is readable before custom completion tick",
               t64_machine_bus_read(machine, TEST_PI_BASE + 16u, 4u, &value),
               T64_STATUS_OK);
  check_u64("PI remains busy through custom tick 6", value & 1u, 1u);

  request.value = UINT64_C(7);
  check_status("PI timing profile advances through tick 7",
               t64_machine_advance(machine, &request, &result), T64_STATUS_OK);
  check_u64("PI custom completion occurs at tick 7", result.tick, 7u);
  check_status("PI status is readable after custom completion tick",
               t64_machine_bus_read(machine, TEST_PI_BASE + 16u, 4u, &value),
               T64_STATUS_OK);
  check_u64("PI is idle at custom tick 7", value & 1u, 0u);
  t64_machine_destroy(machine);
}

static void test_register_masks_and_invalid_requests(void) {
  uint8_t image[TEST_ROM_BYTES];
  t64_machine *machine = NULL;
  uint32_t value = 0u;
  const struct {
    uint32_t offset;
    uint32_t written;
    uint32_t expected;
  } timing[] = {
      {UINT32_C(0x14), UINT32_C(0xffffffab), UINT32_C(0xab)},
      {UINT32_C(0x18), UINT32_C(0x123456cd), UINT32_C(0xcd)},
      {UINT32_C(0x1c), UINT32_C(0x7654321e), UINT32_C(0x0e)},
      {UINT32_C(0x20), UINT32_C(0xabcdef03), UINT32_C(0x03)},
      {UINT32_C(0x24), UINT32_C(0x98765432), UINT32_C(0x32)},
      {UINT32_C(0x28), UINT32_C(0x87654321), UINT32_C(0x21)},
      {UINT32_C(0x2c), UINT32_C(0x8765432f), UINT32_C(0x0f)},
      {UINT32_C(0x30), UINT32_C(0x8765432d), UINT32_C(0x01)},
  };
  size_t i;

  if (!create_test_machine(&machine, image, sizeof image)) {
    return;
  }

  check_status("PI DRAM address accepts masked latch write",
               t64_machine_bus_write(machine, TEST_PI_BASE,
                                     UINT32_C(4), UINT32_C(0xab123456)),
               T64_STATUS_OK);
  check_status("PI DRAM address latch reads back",
               t64_machine_bus_read(machine, TEST_PI_BASE, UINT32_C(4),
                                    &value),
               T64_STATUS_OK);
  check_u64("PI DRAM address is masked to 24 bits", value,
            UINT64_C(0x00123456));

  check_status("PI CART address accepts full-width write",
               t64_machine_bus_write(machine, TEST_PI_BASE + UINT32_C(4),
                                     UINT32_C(4), UINT32_MAX),
               T64_STATUS_OK);
  check_status("PI CART address latch reads back",
               t64_machine_bus_read(machine, TEST_PI_BASE + UINT32_C(4),
                                    UINT32_C(4), &value),
               T64_STATUS_OK);
  check_u64("PI CART address retains all 32 bits", value, UINT32_MAX);

  check_status("PI read length write is refused in ROM profile",
               t64_machine_bus_write(machine, TEST_PI_BASE + UINT32_C(8),
                                     UINT32_C(4), UINT32_C(0xab123456)),
               T64_STATUS_BUS_DEVICE_REFUSED);
  check_status("PI read length latch remains readable",
               t64_machine_bus_read(machine, TEST_PI_BASE + UINT32_C(8),
                                    UINT32_C(4), &value),
               T64_STATUS_OK);
  check_u64("PI read length is masked to 24 bits", value,
            UINT64_C(0x00123456));

  for (i = 0u; i < sizeof timing / sizeof timing[0]; i++) {
    check_status("PI domain timing register write succeeds",
                 t64_machine_bus_write(machine,
                                       TEST_PI_BASE + timing[i].offset,
                                       UINT32_C(4), timing[i].written),
                 T64_STATUS_OK);
    check_status("PI domain timing register reads back",
                 t64_machine_bus_read(machine,
                                      TEST_PI_BASE + timing[i].offset,
                                      UINT32_C(4), &value),
                 T64_STATUS_OK);
    check_u64("PI timing register retains only its documented field width",
              value, timing[i].expected);
  }

  check_status("PI status read exposes the initial status",
               t64_machine_bus_read(machine, TEST_PI_BASE + UINT32_C(0x10),
                                    UINT32_C(4), &value),
               T64_STATUS_OK);
  check_u64("PI IO_BUSY and ERROR are clear after semantic refusals", value,
            UINT64_C(0));

  check_status("PI write length starts with an odd one-byte request",
               t64_machine_bus_write(machine, TEST_PI_BASE + UINT32_C(0x0c),
                                     UINT32_C(4), UINT32_C(0)),
               T64_STATUS_BUS_DEVICE_REFUSED);
  check_status("PI odd length latch reads back",
               t64_machine_bus_read(machine, TEST_PI_BASE + UINT32_C(0x0c),
                                    UINT32_C(4), &value),
               T64_STATUS_OK);
  check_u64("PI odd semantic refusal retains the masked length", value,
            UINT64_C(0));
  check_status("PI status remains idle after odd length refusal",
               t64_machine_bus_read(machine, TEST_PI_BASE + UINT32_C(0x10),
                                    UINT32_C(4), &value),
               T64_STATUS_OK);
  check_u64("PI odd length refusal does not set busy or error", value,
            UINT64_C(0));

  check_status("PI offset outside the documented register set is refused",
               t64_machine_bus_read(machine, TEST_PI_BASE + UINT32_C(0x34),
                                    UINT32_C(4), &value),
               T64_STATUS_BUS_UNSUPPORTED);
  check_status("PI byte register access is refused",
               t64_machine_bus_read(machine, TEST_PI_BASE, UINT32_C(1),
                                    &value),
               T64_STATUS_BUS_UNSUPPORTED);
  t64_machine_destroy(machine);
}

static void test_busy_start_and_model_minimum(void) {
  uint8_t image[TEST_ROM_BYTES];
  const t64_host_allocator allocator = {test_allocate, test_release, NULL};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(1)};
  t64_stop_result result;
  uint32_t value = 0u;

  fill_authored_image(image, sizeof image);
  check_status("PI minimum fixture profile lookup succeeds",
               t64_profile_lookup("ntsc-cart-4m-skeleton", &profile),
               T64_STATUS_OK);
  check_status("PI minimum fixture machine creation succeeds",
               t64_machine_create(&allocator, profile, &machine),
               T64_STATUS_OK);
  if (machine == NULL) {
    check_u64("PI minimum fixture machine exists", 0u, 1u);
    return;
  }
  check_status("PI minimum fixture ROM admission succeeds",
               t64_machine_admit_rom(machine, image, sizeof image),
               T64_STATUS_OK);
  check_status("PI minimum RDRAM address write succeeds",
               t64_machine_bus_write(machine, TEST_PI_BASE, 4u,
                                     UINT32_C(0x100)),
               T64_STATUS_OK);
  check_status("PI minimum CART address write succeeds",
               t64_machine_bus_write(machine, TEST_PI_BASE + 4u, 4u,
                                     TEST_CART_BASE + UINT32_C(0x100)),
               T64_STATUS_OK);
  check_status("PI accepts the minimum legal two-byte transfer",
               t64_machine_bus_write(machine, TEST_PI_BASE + 12u, 4u,
                                     UINT32_C(1)),
               T64_STATUS_OK);
  check_status("PI advances to one tick before minimum deadline",
               t64_machine_advance(machine, &request, &result),
               T64_STATUS_OK);
  check_u64("Minimum PI request has not completed at tick 1", result.tick,
            UINT64_C(1));
  check_status("Minimum transfer destination is readable before completion",
               t64_machine_bus_read(machine, UINT32_C(0x100), 1u, &value),
               T64_STATUS_OK);
  check_u64("Minimum transfer destination is unchanged before completion",
            value, UINT64_C(0));
  request.value = UINT64_C(2);
  check_status("PI advances inclusively to minimum deadline",
               t64_machine_advance(machine, &request, &result),
               T64_STATUS_OK);
  check_u64("Two-byte minimum completes at model tick 2", result.tick,
            UINT64_C(2));
  check_status("Minimum transfer first byte is observable",
               t64_machine_bus_read(machine, UINT32_C(0x100), 1u, &value),
               T64_STATUS_OK);
  check_u64("Minimum transfer copies the independently authored first byte",
            value, image[0x100u]);
  check_status("Minimum transfer last byte is observable",
               t64_machine_bus_read(machine, UINT32_C(0x101), 1u, &value),
               T64_STATUS_OK);
  check_u64("Minimum transfer copies the independently authored last byte",
            value, image[0x101u]);

  request.value = UINT64_C(11);
  check_status("PI accepts an initial 16-byte request",
               t64_machine_bus_write(machine, TEST_PI_BASE, 4u,
                                     UINT32_C(0x108)),
               T64_STATUS_OK);
  check_status("PI accepts original CART request address",
               t64_machine_bus_write(machine, TEST_PI_BASE + 4u, 4u,
                                     TEST_CART_BASE + UINT32_C(0x120)),
               T64_STATUS_OK);
  check_status("PI accepts original request length",
               t64_machine_bus_write(machine, TEST_PI_BASE + 12u, 4u,
                                     UINT32_C(15)),
               T64_STATUS_OK);
  check_status("PI raw address latches may change while transfer is busy",
               t64_machine_bus_write(machine, TEST_PI_BASE, 4u,
                                     UINT32_C(0x200)),
               T64_STATUS_OK);
  check_status("PI raw CART latch may change while transfer is busy",
               t64_machine_bus_write(machine, TEST_PI_BASE + 4u, 4u,
                                     TEST_CART_BASE + UINT32_C(0x180)),
               T64_STATUS_OK);
  check_status("PI busy second start is refused",
               t64_machine_bus_write(machine, TEST_PI_BASE + 12u, 4u,
                                     UINT32_C(15)),
               T64_STATUS_BUS_DEVICE_REFUSED);
  check_status("PI busy start exposes busy plus sticky error only",
               t64_machine_bus_read(machine, TEST_PI_BASE + 16u, 4u, &value),
               T64_STATUS_OK);
  check_u64("PI busy start retains the original active operation", value,
            UINT64_C(5));
  request.value = UINT64_C(11);
  check_status("Original PI request completes after busy refusal",
               t64_machine_advance(machine, &request, &result),
               T64_STATUS_OK);
  check_u64("Original PI request completes at its original deadline",
            result.tick, UINT64_C(11));
  check_status("Original PI destination byte is readable",
               t64_machine_bus_read(machine, UINT32_C(0x108), 1u, &value),
               T64_STATUS_OK);
  check_u64("Busy retry retained original DRAM descriptor", value,
            image[0x120u]);
  check_status("Changed PI destination remains readable",
               t64_machine_bus_read(machine, UINT32_C(0x200), 1u, &value),
               T64_STATUS_OK);
  check_u64("Busy retry did not copy to changed DRAM latch", value,
            UINT64_C(0));
  check_status("PI error remains sticky after completion",
               t64_machine_bus_read(machine, TEST_PI_BASE + 16u, 4u, &value),
               T64_STATUS_OK);
  check_u64("PI completion clears busy but retains sticky error", value,
            UINT64_C(4));
  check_status("PI reset command clears idle sticky error",
               t64_machine_bus_write(machine, TEST_PI_BASE + 16u, 4u,
                                     UINT32_C(1)),
               T64_STATUS_OK);
  check_status("PI status after idle reset is clear",
               t64_machine_bus_read(machine, TEST_PI_BASE + 16u, 4u, &value),
               T64_STATUS_OK);
  check_u64("Idle PI reset clears error without setting busy", value,
            UINT64_C(0));
  t64_machine_destroy(machine);
}

static void test_exact_range_boundaries(void) {
  uint8_t image[TEST_ROM_BYTES];
  t64_machine *machine = NULL;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(9)};
  t64_stop_result result;
  uint32_t value = 0u;

  if (!create_test_machine(&machine, image, sizeof image)) {
    return;
  }
  check_status("PI end-boundary DRAM latch is writable",
               t64_machine_bus_write(machine, TEST_PI_BASE, 4u,
                                     UINT32_C(0x003ffff0)),
               T64_STATUS_OK);
  check_status("PI end-boundary CART latch is writable",
               t64_machine_bus_write(machine, TEST_PI_BASE + 4u, 4u,
                                     TEST_CART_BASE + UINT32_C(0xff0)),
               T64_STATUS_OK);
  check_status("PI accepts exact ROM and RDRAM end boundaries",
               t64_machine_bus_write(machine, TEST_PI_BASE + 12u, 4u,
                                     UINT32_C(15)),
               T64_STATUS_OK);
  check_status("PI completes exact end-boundary request",
               t64_machine_advance(machine, &request, &result),
               T64_STATUS_OK);
  check_u64("Exact RDRAM endpoint completes at tick 9", result.tick,
            UINT64_C(9));
  check_status("Exact RDRAM endpoint last byte is readable",
               t64_machine_bus_read(machine, UINT32_C(0x003fffff), 1u,
                                    &value),
               T64_STATUS_OK);
  check_u64("Exact RDRAM endpoint copied the expected ROM byte", value,
            image[0xfffu]);

  check_status("PI rejects span past the installed ROM end",
               t64_machine_bus_write(machine, TEST_PI_BASE + 4u, 4u,
                                     TEST_CART_BASE + UINT32_C(0xff8)),
               T64_STATUS_OK);
  check_status("PI ROM overrun request refuses before busy",
               t64_machine_bus_write(machine, TEST_PI_BASE + 12u, 4u,
                                     UINT32_C(15)),
               T64_STATUS_BUS_DEVICE_REFUSED);
  check_status("PI rejects span past installed RDRAM end",
               t64_machine_bus_write(machine, TEST_PI_BASE, 4u,
                                     UINT32_C(0x003ffff8)),
               T64_STATUS_OK);
  check_status("PI RDRAM overrun request refuses before busy",
               t64_machine_bus_write(machine, TEST_PI_BASE + 4u, 4u,
                                     TEST_CART_BASE + UINT32_C(0x100)),
               T64_STATUS_OK);
  check_status("PI RDRAM overrun length is refused",
               t64_machine_bus_write(machine, TEST_PI_BASE + 12u, 4u,
                                     UINT32_C(15)),
               T64_STATUS_BUS_DEVICE_REFUSED);
  check_status("PI 24-bit maximum length is retained when profile range refuses",
               t64_machine_bus_write(machine, TEST_PI_BASE + 12u, 4u,
                                     UINT32_C(0xffffffff)),
               T64_STATUS_BUS_DEVICE_REFUSED);
  check_status("PI maximum raw length reads back masked",
               t64_machine_bus_read(machine, TEST_PI_BASE + 12u, 4u, &value),
               T64_STATUS_OK);
  check_u64("PI raw length field is 24 bits and represents N minus one", value,
            UINT64_C(0x00ffffff));
  check_status("PI remains idle after out-of-range requests",
               t64_machine_bus_read(machine, TEST_PI_BASE + 16u, 4u, &value),
               T64_STATUS_OK);
  check_u64("PI out-of-range refusals do not create busy or error", value,
            UINT64_C(0));
  t64_machine_destroy(machine);
}

static void test_largest_profile_transfer(void) {
  const size_t image_length = (size_t)UINT32_C(0x00400000);
  uint8_t *image = (uint8_t *)malloc(image_length);
  t64_machine *machine = NULL;
  t64_stop_request request = {T64_STOP_GUEST_TICK,
                              UINT64_C(2097152)};
  t64_stop_result result;
  uint32_t value = 0u;

  check_u64("Largest PI fixture allocation succeeds", image != NULL,
            UINT64_C(1));
  if (image == NULL) {
    return;
  }
  if (!create_test_machine(&machine, image, image_length)) {
    free(image);
    return;
  }
  check_status("PI refuses a start until its cartridge address is set",
               t64_machine_bus_write(machine, TEST_PI_BASE + 12u, 4u,
                                     UINT32_C(0x003fffff)),
               T64_STATUS_BUS_DEVICE_REFUSED);
  /* The default DRAM/CART latches are zero; use the actual cartridge base. */
  check_status("PI largest transfer DRAM base is writable",
               t64_machine_bus_write(machine, TEST_PI_BASE, 4u, 0u),
               T64_STATUS_OK);
  check_status("PI largest transfer cartridge base is writable",
               t64_machine_bus_write(machine, TEST_PI_BASE + 4u, 4u,
                                     TEST_CART_BASE),
               T64_STATUS_OK);
  check_status("PI accepts largest whole profile span",
               t64_machine_bus_write(machine, TEST_PI_BASE + 12u, 4u,
                                     UINT32_C(0x003fffff)),
               T64_STATUS_OK);
  request.value = UINT64_C(2097152);
  check_status("PI largest span stops one tick before deadline",
               t64_machine_advance(machine, &request, &result),
               T64_STATUS_OK);
  check_u64("Largest PI request remains before its deadline", result.tick,
            UINT64_C(2097152));
  check_status("Largest PI destination first byte is readable",
               t64_machine_bus_read(machine, 0u, 1u, &value), T64_STATUS_OK);
  check_u64("Largest PI destination remains unchanged before completion",
            value, UINT64_C(0));
  request.value = UINT64_C(2097153);
  check_status("PI largest span completes at its inclusive deadline",
               t64_machine_advance(machine, &request, &result),
               T64_STATUS_OK);
  check_u64("Four MiB PI request deadline follows the named model equation",
            result.tick, UINT64_C(2097153));
  check_u64("Largest PI copy exactly matches the test-authored image",
            memcmp(machine->ram, image, image_length) == 0, UINT64_C(1));
  check_status("Largest PI destination end byte is readable",
               t64_machine_bus_read(machine, UINT32_C(0x003fffff), 1u,
                                    &value),
               T64_STATUS_OK);
  check_u64("Largest PI span reaches the final installed RDRAM byte", value,
            image[image_length - 1u]);
  check_status("PI refuses a 16 MiB count beyond installed profile ranges",
               t64_machine_bus_write(machine, TEST_PI_BASE + 12u, 4u,
                                     UINT32_C(0x00ffffff)),
               T64_STATUS_BUS_DEVICE_REFUSED);
  check_status("PI remains idle after maximum count exceeds profile span",
               t64_machine_bus_read(machine, TEST_PI_BASE + 16u, 4u, &value),
               T64_STATUS_OK);
  check_u64("16 MiB request was not accepted without 16 MiB of installed RAM",
            value & UINT32_C(1), UINT64_C(0));
  t64_machine_destroy(machine);
  free(image);
}

static void test_resource_refusals_are_atomic(void) {
  uint8_t image[TEST_ROM_BYTES];
  t64_machine *machine = NULL;
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  uint32_t value = 0u;
  uint32_t i;
  t64_status status;

  if (!create_test_machine(&machine, image, sizeof image)) {
    return;
  }
  check_status("PI resource test DRAM latch is valid",
               t64_machine_bus_write(machine, TEST_PI_BASE, 4u,
                                     UINT32_C(0x100)),
               T64_STATUS_OK);
  check_status("PI resource test CART latch is valid",
               t64_machine_bus_write(machine, TEST_PI_BASE + 4u, 4u,
                                     TEST_CART_BASE + UINT32_C(0x100)),
               T64_STATUS_OK);
  for (i = 0u; i < machine->profile.event_queue_capacity; i++) {
    check_status("PI queue capacity fixture fills one scheduler slot",
                 t64_test_schedule_synthetic(machine, UINT64_C(100),
                                             T64_TEST_EVENT_NOOP, 0u),
                 T64_STATUS_OK);
  }
  check_status("PI digest captured before queue-full refusal",
               t64_machine_digest(machine, digest_before), T64_STATUS_OK);
  status = t64_machine_bus_write(machine, TEST_PI_BASE + 12u, 4u,
                                 UINT32_C(15));
  check_status("PI start refuses when completion queue is full", status,
               T64_STATUS_QUEUE_FULL);
  check_status("PI digest captured after queue-full refusal",
               t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  check_u64("Queue-full refusal leaves canonical machine state unchanged",
            memcmp(digest_before, digest_after, sizeof digest_before) == 0,
            UINT64_C(1));
  check_status("Queue-full refusal leaves the triggering length latch alone",
               t64_machine_bus_read(machine, TEST_PI_BASE + 12u, 4u, &value),
               T64_STATUS_OK);
  check_u64("Queue-full refusal did not change the raw write length", value,
            UINT64_C(0));

  machine->queue_head = 0u;
  machine->queue_count = 0u;
  machine->sequence_exhausted = true;
  check_status("PI digest captured before sequence exhaustion refusal",
               t64_machine_digest(machine, digest_before), T64_STATUS_OK);
  check_status("PI start refuses at sequence exhaustion",
               t64_machine_bus_write(machine, TEST_PI_BASE + 12u, 4u,
                                     UINT32_C(15)),
               T64_STATUS_SEQUENCE_EXHAUSTED);
  check_status("PI digest captured after sequence exhaustion refusal",
               t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  check_u64("Sequence refusal leaves canonical machine state unchanged",
            memcmp(digest_before, digest_after, sizeof digest_before) == 0,
            UINT64_C(1));
  machine->sequence_exhausted = false;

  machine->tick = UINT64_MAX - UINT64_C(8);
  check_status("PI digest captured before deadline overflow refusal",
               t64_machine_digest(machine, digest_before), T64_STATUS_OK);
  check_status("PI start refuses model deadline addition overflow",
               t64_machine_bus_write(machine, TEST_PI_BASE + 12u, 4u,
                                     UINT32_C(15)),
               T64_STATUS_TICK_OVERFLOW);
  check_status("PI digest captured after deadline overflow refusal",
               t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  check_u64("Deadline overflow leaves canonical machine state unchanged",
            memcmp(digest_before, digest_after, sizeof digest_before) == 0,
            UINT64_C(1));

  machine->tick = 0u;
  machine->pi.token_exhausted = true;
  check_status("PI digest captured before token exhaustion refusal",
               t64_machine_digest(machine, digest_before), T64_STATUS_OK);
  check_status("PI start refuses token exhaustion without wrapping",
               t64_machine_bus_write(machine, TEST_PI_BASE + 12u, 4u,
                                     UINT32_C(15)),
               T64_STATUS_RESOURCE_EXHAUSTED);
  check_status("PI digest captured after token exhaustion refusal",
               t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  check_u64("Token exhaustion leaves canonical machine state unchanged",
            memcmp(digest_before, digest_after, sizeof digest_before) == 0,
            UINT64_C(1));
  check_status("PI status remains idle after resource refusals",
               t64_machine_bus_read(machine, TEST_PI_BASE + 16u, 4u, &value),
               T64_STATUS_OK);
  check_u64("Resource refusals never leave uncompleted busy state", value,
            UINT64_C(0));
  t64_machine_destroy(machine);
}

static void test_pi_digest_inclusion(void) {
  uint8_t image[TEST_ROM_BYTES];
  t64_machine *machine = NULL;
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  t64_status status;

  if (!create_test_machine(&machine, image, sizeof image)) {
    return;
  }
  status = t64_machine_digest(machine, digest_before);
  check_status("PI digest baseline is available", status, T64_STATUS_OK);
  status = t64_machine_bus_write(machine, TEST_PI_BASE, 4u,
                                 UINT32_C(0x100));
  check_status("PI DRAM latch mutation succeeds", status, T64_STATUS_OK);
  status = t64_machine_digest(machine, digest_after);
  check_status("PI digest after latch mutation is available", status,
               T64_STATUS_OK);
  check_u64("PI raw register latch changes canonical future-state digest",
            memcmp(digest_before, digest_after, sizeof digest_before) != 0,
            UINT64_C(1));
  t64_machine_destroy(machine);
}

static t64_status clear_pi_event(t64_machine *machine,
                                 const t64_event *event) {
  return t64_pi_write_register(machine, TEST_PI_BASE + UINT32_C(0x10),
                               (uint32_t)event->value);
}

static t64_status schedule_pi_clear(t64_machine *machine, uint64_t tick) {
  t64_event_reservation reservation;
  t64_status status = t64_schedule_reserve(
      machine, tick, T64_DEVICE_SYNTHETIC_TEST, UINT32_C(0x5049434c),
      UINT64_C(2), UINT64_C(0), clear_pi_event, &reservation);

  return status == T64_STATUS_OK
             ? t64_schedule_commit(machine, &reservation)
             : status;
}

static void test_cancel_and_stale_completion(void) {
  uint8_t image[TEST_ROM_BYTES];
  size_t callback_calls = 0u;
  const t64_host_allocator allocator = {test_allocate, test_release,
                                        &callback_calls};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(8)};
  t64_stop_result result;
  t64_event stale = {0};
  uint32_t value = 0u;
  uint8_t digest_before_stale[T64_DIGEST_SIZE];
  uint8_t digest_after_stale[T64_DIGEST_SIZE];
  uint64_t canceled_token;
  size_t callbacks_before_advance;
  size_t i;

  fill_authored_image(image, sizeof image);
  check_status("PI cancel profile lookup succeeds",
               t64_profile_lookup("ntsc-cart-4m-skeleton", &profile),
               T64_STATUS_OK);
  check_status("PI cancel machine creation succeeds",
               t64_machine_create(&allocator, profile, &machine),
               T64_STATUS_OK);
  if (machine == NULL) {
    check_u64("PI cancel fixture machine exists", 0u, 1u);
    return;
  }
  check_status("PI cancel image admission succeeds",
               t64_machine_admit_rom(machine, image, sizeof image),
               T64_STATUS_OK);
  check_status("PI cancel DRAM latch setup succeeds",
               t64_machine_bus_write(machine, TEST_PI_BASE, 4u, 0x100u),
               T64_STATUS_OK);
  check_status("PI cancel CART latch setup succeeds",
               t64_machine_bus_write(machine, TEST_PI_BASE + 4u, 4u,
                                     TEST_CART_BASE + 0x100u),
               T64_STATUS_OK);
  check_status("PI cancel request is admitted",
               t64_machine_bus_write(machine, TEST_PI_BASE + 12u, 4u, 15u),
               T64_STATUS_OK);
  canceled_token = machine->pi.active_token;
  check_u64("PI request has a nonzero completion token",
            canceled_token != 0u, 1u);

  callbacks_before_advance = callback_calls;
  check_status("PI advances to one tick before reset cancellation",
               t64_machine_advance(machine, &request, &result),
               T64_STATUS_OK);
  check_u64("Reset test reaches the deadline minus one", result.tick, 8u);
  check_u64("PI is active at the deadline minus one",
            machine->pi.active_valid, 1u);
  check_status("PI destination is readable before reset at tick 8",
               t64_machine_bus_read(machine, 0x100u, 1u, &value),
               T64_STATUS_OK);
  check_u64("No copy occurs before reset at deadline minus one", value, 0u);
  check_u64("Guest advancement makes no host allocator calls",
            callback_calls, callbacks_before_advance);

  /* Full queue proves reset reuses its reserved completion, not a new slot. */
  for (i = 0u; i < T64_EVENT_QUEUE_CAPACITY - 1u; i++) {
    check_status("PI cancel queue fill preserves reserved completion",
                 t64_test_schedule_synthetic(machine, 100u,
                                             T64_TEST_EVENT_NOOP, i),
                 T64_STATUS_OK);
  }
  check_u64("PI completion remains reserved in a full queue",
            machine->queue_count, T64_EVENT_QUEUE_CAPACITY);
  check_status("PI reset cancels without allocating a queue slot",
               t64_machine_bus_write(machine, TEST_PI_BASE + 16u, 4u, 1u),
               T64_STATUS_OK);
  check_status("Repeated PI reset is idempotent",
               t64_machine_bus_write(machine, TEST_PI_BASE + 16u, 4u, 1u),
               T64_STATUS_OK);
  check_u64("Repeated reset preserves the original token",
            machine->pi.active_token, canceled_token);
  check_u64("Repeated reset preserves one active completion",
            machine->queue_count, T64_EVENT_QUEUE_CAPACITY);
  check_u64("Repeated reset marks the request canceled",
            machine->pi.active_canceled, 1u);
  check_status("PI status remains busy until the original deadline",
               t64_machine_bus_read(machine, TEST_PI_BASE + 16u, 4u, &value),
               T64_STATUS_OK);
  check_u64("Canceled transfer retains busy and clears error", value, 1u);

  request.value = 9u;
  check_status("Canceled request dispatches its retained completion",
               t64_machine_advance(machine, &request, &result),
               T64_STATUS_OK);
  check_u64("Canceled completion runs at original tick 9", result.tick, 9u);
  check_u64("Canceled completion returns controller idle",
            machine->pi.active_valid, 0u);
  check_status("Canceled destination remains readable after completion",
               t64_machine_bus_read(machine, 0x100u, 1u, &value),
               T64_STATUS_OK);
  check_u64("Canceled completion suppresses the copy", value, 0u);
  check_u64("Canceled completion raises PI pending exactly as a completion",
            (machine->mi_pending & UINT32_C(0x10)) != 0u, 1u);
  check_status("PI clear command acknowledges canceled completion",
               t64_machine_bus_write(machine, TEST_PI_BASE + 16u, 4u, 2u),
               T64_STATUS_OK);
  request.value = 9u;
  check_status("Repeated advance at completed tick succeeds",
               t64_machine_advance(machine, &request, &result),
               T64_STATUS_OK);
  check_u64("No duplicate PI event restores cleared pending", machine->mi_pending,
            0u);

  check_status("New PI request can start after canceled completion",
               t64_machine_bus_write(machine, TEST_PI_BASE, 4u, 0x108u),
               T64_STATUS_OK);
  check_status("New PI request uses a new cartridge span",
               t64_machine_bus_write(machine, TEST_PI_BASE + 4u, 4u,
                                     TEST_CART_BASE + 0x120u),
               T64_STATUS_OK);
  check_status("New request is admitted with a distinct token",
               t64_machine_bus_write(machine, TEST_PI_BASE + 12u, 4u, 15u),
               T64_STATUS_OK);
  check_u64("A later PI request uses a distinct nonwrapping token",
            machine->pi.active_token != canceled_token, 1u);
  stale.device = T64_DEVICE_PI;
  stale.kind = T64_PI_EVENT_COMPLETE;
  stale.token = canceled_token;
  stale.tick = 10u;
  check_status("Digest before injected stale completion is available",
               t64_machine_digest(machine, digest_before_stale),
               T64_STATUS_OK);
  check_status("Injected stale PI completion is ignored",
               t64_pi_complete(machine, &stale), T64_STATUS_OK);
  check_status("Digest after injected stale completion is available",
               t64_machine_digest(machine, digest_after_stale),
               T64_STATUS_OK);
  check_u64("Stale completion is a canonical state no-op",
            memcmp(digest_before_stale, digest_after_stale,
                   sizeof digest_before_stale) == 0,
            1u);
  check_u64("Stale token cannot deactivate the newer transfer",
            machine->pi.active_valid, 1u);
  check_u64("Stale token cannot change the newer active token",
            machine->pi.active_token != canceled_token, 1u);
  check_status("New request completes at its own deadline",
               t64_machine_advance(machine, &(t64_stop_request){
                                                  T64_STOP_GUEST_TICK, 18u},
                                   &result),
               T64_STATUS_OK);
  check_status("New destination contains the new source bytes",
               t64_machine_bus_read(machine, 0x108u, 1u, &value),
               T64_STATUS_OK);
  check_u64("New request copies only its own descriptor", value,
            image[0x120u]);
  check_status("Old canceled destination remains zero",
               t64_machine_bus_read(machine, 0x100u, 1u, &value),
               T64_STATUS_OK);
  check_u64("Stale event cannot write the old destination", value, 0u);
  check_u64("Advance through PI events has no host allocator calls",
            callback_calls, callbacks_before_advance);
  t64_machine_destroy(machine);
}

static void test_same_tick_clear_fifo(void) {
  uint8_t image_a[TEST_ROM_BYTES];
  uint8_t image_b[TEST_ROM_BYTES];
  t64_machine *machine_a = NULL;
  t64_machine *machine_b = NULL;
  t64_stop_request request = {T64_STOP_GUEST_TICK, 9u};
  t64_stop_result result;

  if (!create_test_machine(&machine_a, image_a, sizeof image_a) ||
      !create_test_machine(&machine_b, image_b, sizeof image_b)) {
    t64_machine_destroy(machine_a);
    t64_machine_destroy(machine_b);
    return;
  }
  check_status("Clear-before-completion event reserves tick 9 first",
               schedule_pi_clear(machine_a, 9u), T64_STATUS_OK);
  check_status("PI clear-before request DRAM latch succeeds",
               t64_machine_bus_write(machine_a, TEST_PI_BASE, 4u, 0x100u),
               T64_STATUS_OK);
  check_status("PI clear-before request CART latch succeeds",
               t64_machine_bus_write(machine_a, TEST_PI_BASE + 4u, 4u,
                                     TEST_CART_BASE + 0x100u),
               T64_STATUS_OK);
  check_status("PI clear-before request queues after clear at same tick",
               t64_machine_bus_write(machine_a, TEST_PI_BASE + 12u, 4u, 15u),
               T64_STATUS_OK);
  check_status("PI clear-after request DRAM latch succeeds",
               t64_machine_bus_write(machine_b, TEST_PI_BASE, 4u, 0x100u),
               T64_STATUS_OK);
  check_status("PI clear-after request CART latch succeeds",
               t64_machine_bus_write(machine_b, TEST_PI_BASE + 4u, 4u,
                                     TEST_CART_BASE + 0x100u),
               T64_STATUS_OK);
  check_status("PI completion reserves tick 9 before later clear",
               t64_machine_bus_write(machine_b, TEST_PI_BASE + 12u, 4u, 15u),
               T64_STATUS_OK);
  check_status("Clear-after-completion event reserves tick 9 second",
               schedule_pi_clear(machine_b, 9u), T64_STATUS_OK);
  check_status("FIFO clear-before request advances through tick 9",
               t64_machine_advance(machine_a, &request, &result),
               T64_STATUS_OK);
  check_u64("Completion after an earlier same-tick clear sets PI pending",
            (machine_a->mi_pending & UINT32_C(0x10)) != 0u, 1u);
  check_status("FIFO clear-after request advances through tick 9",
               t64_machine_advance(machine_b, &request, &result),
               T64_STATUS_OK);
  check_u64("A later same-tick clear removes PI pending",
            (machine_b->mi_pending & UINT32_C(0x10)) != 0u, 0u);
  t64_machine_destroy(machine_a);
  t64_machine_destroy(machine_b);
}

static void test_pi_trace_observer(void) {
  uint8_t image[TEST_ROM_BYTES];
  t64_machine *machine = NULL;
  t64_trace_record records[T64_TRACE_RECORD_CAPACITY];
  t64_stop_request request = {T64_STOP_GUEST_TICK, 9u};
  t64_stop_result result;
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  size_t count = 0u;
  uint64_t dropped = 0u;
  bool saturated = false;
  bool saw_refused = false;
  bool saw_issue = false;
  bool saw_cancel = false;
  bool saw_completion = false;
  size_t cancel_count = 0u;
  size_t i;

  if (!create_test_machine(&machine, image, sizeof image)) {
    return;
  }
  check_status("PI trace observer enables without a host sink",
               t64_machine_trace_set_enabled(machine, true), T64_STATUS_OK);
  check_status("PI refusal is visible through a model trace record",
               t64_machine_bus_write(machine, TEST_PI_BASE + 12u, 4u, 0u),
               T64_STATUS_BUS_DEVICE_REFUSED);
  check_status("PI trace DRAM address setup succeeds",
               t64_machine_bus_write(machine, TEST_PI_BASE, 4u, 0x100u),
               T64_STATUS_OK);
  check_status("PI trace cartridge address setup succeeds",
               t64_machine_bus_write(machine, TEST_PI_BASE + 4u, 4u,
                                     TEST_CART_BASE + 0x100u),
               T64_STATUS_OK);
  check_status("PI trace transfer issues",
               t64_machine_bus_write(machine, TEST_PI_BASE + 12u, 4u, 15u),
               T64_STATUS_OK);
  check_status("PI trace cancellation is accepted",
               t64_machine_bus_write(machine, TEST_PI_BASE + 16u, 4u, 1u),
               T64_STATUS_OK);
  check_status("Repeated PI trace reset is idempotent",
               t64_machine_bus_write(machine, TEST_PI_BASE + 16u, 4u, 1u),
               T64_STATUS_OK);
  check_status("PI trace transfer completes at its scheduled boundary",
               t64_machine_advance(machine, &request, &result),
               T64_STATUS_OK);
  check_status("PI digest is available before draining observer records",
               t64_machine_digest(machine, digest_before), T64_STATUS_OK);
  check_status("PI observer records drain outside machine advancement",
               t64_machine_trace_drain(machine, records,
                                       T64_TRACE_RECORD_CAPACITY, &count,
                                       &dropped, &saturated),
               T64_STATUS_OK);
  for (i = 0u; i < count; i++) {
    if (records[i].device != T64_TRACE_DEVICE_PI) {
      continue;
    }
    saw_refused |= records[i].kind == T64_TRACE_KIND_PI_REFUSED;
    saw_issue |= records[i].kind == T64_TRACE_KIND_PI_ISSUE;
    if (records[i].kind == T64_TRACE_KIND_PI_CANCEL) {
      saw_cancel = true;
      cancel_count++;
    }
    saw_completion |= records[i].kind == T64_TRACE_KIND_PI_COMPLETE &&
                      records[i].tick == UINT64_C(9);
  }
  check_u64("PI observer identifies refused operations", saw_refused, 1u);
  check_u64("PI observer identifies request issue", saw_issue, 1u);
  check_u64("PI observer identifies cancellation", saw_cancel, 1u);
  check_u64("Repeated reset emits only one cancellation trace", cancel_count,
            1u);
  check_u64("PI observer timestamps completion at tick 9", saw_completion,
            1u);
  check_status("PI digest is available after observer drain",
               t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  check_u64("PI trace drain leaves canonical machine state unchanged",
            memcmp(digest_before, digest_after, sizeof digest_before) == 0,
            1u);
  t64_machine_destroy(machine);
}

static void emit_tap_assertion(unsigned test_number, const char *name) {
  bool passed = failure_count == 0u && check_count > 0u;

  (void)printf("%s %u - %s\n", passed ? "ok" : "not ok", test_number,
               name);
  if (!passed) {
    (void)printf("# failed assertion: %s\n# expected: %llu\n# actual: %llu\n",
                 first_failure_name != NULL ? first_failure_name :
                                              "at least one test assertion",
                 (unsigned long long)first_failure_expected,
                 (unsigned long long)first_failure_actual);
  }
}

int main(void) {
  bool all_passed;

  (void)printf("TAP version 13\n");
  test_delayed_rom_copy();
  test_profile_pi_duration_controls_completion_tick();
  test_register_masks_and_invalid_requests();
  test_busy_start_and_model_minimum();
  test_exact_range_boundaries();
  test_largest_profile_transfer();
  test_resource_refusals_are_atomic();
  emit_tap_assertion(1u,
                     "PI transfers 16 authored bytes at tick 9 with pending IRQ");
  all_passed = failure_count == 0u && check_count > 0u;

  check_count = 0u;
  failure_count = 0u;
  first_failure_name = NULL;
  first_failure_actual = 0u;
  first_failure_expected = 0u;
  test_pi_digest_inclusion();
  test_cancel_and_stale_completion();
  test_same_tick_clear_fifo();
  test_pi_trace_observer();
  emit_tap_assertion(2u,
                     "PI raw register state changes canonical future-state digest");
  all_passed = all_passed && failure_count == 0u && check_count > 0u;

  (void)printf("1..2\n");
  return all_passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
