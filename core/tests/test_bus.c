#include "t64_test.h"

#include "tendies64/t64_machine.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

static void test_memory_boundaries(void) {
#define TEST_ROM_BYTES 4100u
  uint8_t image[TEST_ROM_BYTES];
  const t64_host_allocator allocator = {test_allocate, test_release, NULL};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint32_t value = 0u;
  size_t i;

  for (i = 0u; i < sizeof image; i++) {
    image[i] = (uint8_t)((i * 29u + 7u) & 0xffu);
  }
  image[0] = 0x80u;
  image[1] = 0x37u;
  image[2] = 0x12u;
  image[3] = 0x40u;

  T64_CHECK_INT(t64_profile_lookup("ntsc-cart-4m-skeleton", &profile),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_admit_rom(machine, image, sizeof image),
                T64_STATUS_OK);

  /*
   * RDRAM endpoints follow Decision B §Physical map and base memory.
   * Capacity is profile policy: 4 MiB byte plane. Source: Nintendo 64
   * Introductory Manual §1.2.4, retrieved SHA-256
   * 53547b3266fe21c319601385874f0728d088e7ac96081ddb1a468a654a200cbb.
   */
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x00000000), 1u,
                                      UINT32_C(0x5a)), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x00000000), 1u,
                                     &value), T64_STATUS_OK);
  T64_CHECK_INT(value, UINT32_C(0x5a));

  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x003fffff), 1u,
                                      UINT32_C(0xa5)), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x003fffff), 1u,
                                     &value), T64_STATUS_OK);
  T64_CHECK_INT(value, UINT32_C(0xa5));

  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x00000100), 4u,
                                      UINT32_C(0x01020304)), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x00000100), 4u,
                                     &value), T64_STATUS_OK);
  T64_CHECK_INT(value, UINT32_C(0x01020304));
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x00000100), 1u,
                                     &value), T64_STATUS_OK);
  T64_CHECK_INT(value, UINT32_C(0x01));
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x00000103), 1u,
                                     &value), T64_STATUS_OK);
  T64_CHECK_INT(value, UINT32_C(0x04));

  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x00400000), 1u,
                                     &value), T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0xdead0000), 1u,
                                     &value), T64_STATUS_BUS_UNMAPPED);

  /*
   * SP DMEM/IMEM are distinct adjacent banks per Decision B §Physical map
   * and base memory; SDK rcp.h register map, lines 162-201, SHA-256
   * 982f5ae01956783653d1085ef31c3d936f471f09bd1e65e27a578fae498f972f.
   */
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04000000), 1u,
                                     &value), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x04001000), 1u,
                                      UINT32_C(0x39)), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04001000), 1u,
                                     &value), T64_STATUS_OK);
  T64_CHECK_INT(value, UINT32_C(0x39));
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x04001fff), 1u,
                                      UINT32_C(0xc3)), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04001fff), 1u,
                                     &value), T64_STATUS_OK);
  T64_CHECK_INT(value, UINT32_C(0xc3));
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04002000), 1u,
                                     &value), T64_STATUS_BUS_UNMAPPED);

  /* Every reserved aperture stays explicit and cannot alias low RDRAM. */
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x03f00000), 4u,
                                     &value), T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04040000), 4u,
                                     &value), T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04080000), 4u,
                                     &value), T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04080004), 4u,
                                     &value), T64_STATUS_BUS_UNMAPPED);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04100000), 4u,
                                     &value), T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04200000), 4u,
                                     &value), T64_STATUS_BUS_UNMAPPED);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04300000), 4u,
                                     &value), T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04300010), 4u,
                                     &value), T64_STATUS_BUS_UNMAPPED);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04400000), 4u,
                                     &value), T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04500000), 4u,
                                     &value), T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04600000), 4u,
                                     &value), T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04700000), 4u,
                                     &value), T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04800000), 4u,
                                     &value), T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x1fc00000), 1u,
                                     &value), T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x1fc00800), 1u,
                                     &value), T64_STATUS_BUS_UNMAPPED);

  /*
   * The SDK's rcp.h map at the Decision B source URL, lines 29-55 (SHA-256
   * 982f5ae01956783653d1085ef31c3d936f471f09bd1e65e27a578fae498f972f),
   * identifies alternate cartridge/domain-2 and extended cartridge windows.
   * Only the profile's admitted domain-1 span at 0x10000000 is installed.
   */
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x05000000), 4u,
                                     &value), T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x06000000), 4u,
                                     &value), T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x08000000), 4u,
                                     &value), T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x1fd00000), 4u,
                                     &value), T64_STATUS_BUS_UNSUPPORTED);

  /* Cartridge span is four aligned ROM words beyond its minimum size. */
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x10000000), 4u,
                                     &value), T64_STATUS_OK);
  T64_CHECK_INT(value, UINT32_C(0x80371240));
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x10001000), 1u,
                                     &value), T64_STATUS_OK);
  T64_CHECK_INT(value, image[4096u]);
  value = UINT32_C(0xfeedface);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x10001004), 1u,
                                     &value), T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(value, UINT32_C(0xfeedface));
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x10000000), 1u,
                                      UINT32_C(0x00)),
                T64_STATUS_BUS_DEVICE_REFUSED);

  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x00000001), 4u,
                                     &value), T64_STATUS_BUS_ALIGNMENT);
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0xfffffffd), 4u,
                                      UINT32_C(0x11223344)),
                T64_STATUS_BUS_RANGE);

  t64_machine_destroy(machine);
#undef TEST_ROM_BYTES
}

static void test_bus_trace_uses_guest_tick(void) {
  uint8_t image[4096];
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  t64_trace_record records[4];
  size_t record_count = 0u;
  uint64_t dropped = 0u;
  bool saturated = false;
  const t64_host_allocator allocator = {test_allocate, test_release, NULL};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  const t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(7)};
  t64_stop_result result = {0u, 0u};
  uint32_t value = 0u;

  (void)memset(image, 0, sizeof image);
  image[0] = 0x80u;
  image[1] = 0x37u;
  image[2] = 0x12u;
  image[3] = 0x40u;
  T64_CHECK_INT(t64_profile_lookup("ntsc-cart-4m-skeleton", &profile),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_admit_rom(machine, image, sizeof image),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_trace_set_enabled(machine, true), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_advance(machine, &request, &result), T64_STATUS_OK);
  T64_CHECK_INT(result.tick, UINT64_C(7));
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x00000100), 1u,
                                      UINT32_C(0xa5)), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(machine, digest_before), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x00000100), 1u,
                                     &value), T64_STATUS_OK);
  T64_CHECK_INT(value, UINT32_C(0xa5));
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) == 0);
  T64_CHECK_INT(t64_machine_trace_drain(machine, records, 4u, &record_count,
                                        &dropped, &saturated),
                T64_STATUS_OK);
  T64_CHECK_INT(record_count, 2u);
  if (record_count == 2u) {
    T64_CHECK_INT(records[0].tick, UINT64_C(7));
    T64_CHECK_INT(records[0].device, T64_TRACE_DEVICE_BUS);
    T64_CHECK_INT(records[0].kind, T64_TRACE_KIND_BUS_WRITE8);
    T64_CHECK_INT(records[0].value,
                  ((uint64_t)UINT32_C(0x00000100) << 32) | UINT64_C(0xa5));
    T64_CHECK_INT(records[1].tick, UINT64_C(7));
    T64_CHECK_INT(records[1].kind, T64_TRACE_KIND_BUS_READ8);
  }
  T64_CHECK_INT(dropped, UINT64_C(0));
  T64_CHECK(!saturated);
  t64_machine_destroy(machine);
}

int main(void) {
  test_memory_boundaries();
  test_bus_trace_uses_guest_tick();
  return T64_TEST_FINISH("unit.core.bus");
}
