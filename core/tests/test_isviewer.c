#include "t64_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define T64_ISVIEWER_MAGIC_ADDRESS UINT32_C(0x13ff0000)
#define T64_IPL3_ASSET_BYTES 4096u

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  void *memory;

  (void)context;
  if (storage_out == NULL || size == 0u || alignment == 0u) {
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
  return true;
}

static void test_release(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

static bool read_ipl3_asset(uint8_t bytes[T64_IPL3_ASSET_BYTES]) {
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
  length = fread(bytes, 1u, T64_IPL3_ASSET_BYTES, file);
  extra = fgetc(file);
  read_error = ferror(file);
  close_error = fclose(file);
  return read_error == 0 && close_error == 0 &&
         length == T64_IPL3_ASSET_BYTES && extra == EOF;
}

int main(void) {
  static const char test_name[] =
      "libdragon magic register stores and reads back guest values";
  uint8_t asset[T64_IPL3_ASSET_BYTES];
  const t64_host_allocator allocator = {test_allocate, test_release, NULL};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint32_t observed = UINT32_C(0xdecafbad);
  t64_status write_status = T64_STATUS_INVALID_ARGUMENT;
  t64_status read_status = T64_STATUS_INVALID_ARGUMENT;
  bool setup_ok;
  bool passed;

  puts("TAP version 13");
  setup_ok = read_ipl3_asset(asset) &&
             t64_profile_lookup("ntsc-cart-4m-openipl3-warm-v1", &profile) ==
                 T64_STATUS_OK &&
             profile != NULL &&
             t64_machine_create(&allocator, profile, &machine) ==
                 T64_STATUS_OK &&
             machine != NULL &&
             t64_machine_admit_rom(machine, asset, sizeof asset) ==
                 T64_STATUS_OK;
  if (setup_ok) {
    write_status = t64_machine_bus_write(
        machine, T64_ISVIEWER_MAGIC_ADDRESS, 4u, UINT32_C(0xedcba987));
    read_status = t64_machine_bus_read(machine, T64_ISVIEWER_MAGIC_ADDRESS,
                                       4u, &observed);
  }
  passed = setup_ok && write_status == T64_STATUS_OK &&
           read_status == T64_STATUS_OK && observed == UINT32_C(0xedcba987);
  (void)printf("%s 1 - %s\n", passed ? "ok" : "not ok", test_name);
  (void)printf("# setup=%s write-status=%u read-status=%u value=0x%08x\n",
               setup_ok ? "ready" : "failed", (unsigned)write_status,
               (unsigned)read_status, observed);
  puts("1..1");
  t64_machine_destroy(machine);
  return passed ? 0 : 1;
}
