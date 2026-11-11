#include "t64_internal.h"
#include "tendies64/t64_machine.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define IPL3_ASSET_BYTES 4096u
#define IPL3_ENTRY_PC UINT32_C(0xa4000040)

typedef struct {
  size_t outstanding;
} allocator_state;

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  allocator_state *state = (allocator_state *)context;
  void *memory;

  if (state == NULL || storage_out == NULL || alignment == 0u) {
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
  state->outstanding++;
  return true;
}

static void test_release(void *context, t64_host_storage storage) {
  allocator_state *state = (allocator_state *)context;

  if (state != NULL && storage.data != NULL) {
    free(storage.data);
    state->outstanding--;
  }
}

static bool read_qualified_asset(uint8_t bytes[IPL3_ASSET_BYTES]) {
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
  length = fread(bytes, 1u, IPL3_ASSET_BYTES, file);
  extra = fgetc(file);
  read_error = ferror(file);
  close_error = fclose(file);
  return read_error == 0 && close_error == 0 &&
         length == IPL3_ASSET_BYTES && extra == EOF;
}

int main(void) {
  allocator_state allocator_state_value = {0u};
  const t64_host_allocator allocator = {
      test_allocate, test_release, &allocator_state_value};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t asset[IPL3_ASSET_BYTES];
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(2)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_status status;
  bool okay = false;

  (void)printf("1..1\n");
  if (!read_qualified_asset(asset) ||
      t64_profile_lookup("ntsc-cart-4m-openipl3-warm-v1", &profile) !=
          T64_STATUS_OK ||
      profile == NULL ||
      t64_machine_create(&allocator, profile, &machine) != T64_STATUS_OK ||
      t64_machine_admit_rom(machine, asset, sizeof asset) != T64_STATUS_OK) {
    (void)printf("not ok 1 - qualified IPL3 fixture setup\n");
    goto done;
  }

  status = t64_machine_advance(machine, &request, &result);
  okay = status == T64_STATUS_OK && machine->cpu.pc == IPL3_ENTRY_PC + 8u &&
         machine->cpu.retired_instructions == UINT64_C(2) &&
         machine->instruction_count == UINT64_C(2) &&
         machine->cpu.gpr[29] == UINT64_C(0xffffffffa4000000);
  if (okay) {
    (void)printf(
        "ok 1 - IPL3 LUI retires with MIPS III 32-bit sign extension\n");
  } else {
    (void)printf(
        "not ok 1 - IPL3 LUI retires with MIPS III 32-bit sign extension\n"
        "# expected pc=0x%08x retired=2 sp=0xffffffffa4000000\n"
        "# actual pc=0x%08llx retired=%llu sp=0x%016llx\n",
        IPL3_ENTRY_PC + 8u, (unsigned long long)machine->cpu.pc,
        (unsigned long long)machine->cpu.retired_instructions,
        (unsigned long long)machine->cpu.gpr[29]);
  }

done:
  if (machine != NULL) {
    t64_machine_destroy(machine);
  }
  if (allocator_state_value.outstanding != 0u) {
    okay = false;
  }
  return okay ? 0 : 1;
}
