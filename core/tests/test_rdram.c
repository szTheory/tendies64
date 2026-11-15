#include "t64_test.h"

#include "t64_internal.h"
#include "tendies64/t64_machine.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define IPL3_ASSET_BYTES 4096u

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  void *memory;

  (void)context;
  if (storage_out == NULL || size == 0u || alignment == 0u ||
      alignment > _Alignof(max_align_t)) {
    return false;
  }
  memory = malloc(size);
  if (memory == NULL || ((uintptr_t)memory % alignment) != 0u) {
    free(memory);
    return false;
  }
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = _Alignof(max_align_t);
  return true;
}

static void test_release(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

static void write_instruction(t64_machine *machine, uint32_t offset,
                              uint32_t instruction) {
  uint8_t *bytes = machine->sp_dmem + offset;

  bytes[0] = (uint8_t)(instruction >> 24);
  bytes[1] = (uint8_t)(instruction >> 16);
  bytes[2] = (uint8_t)(instruction >> 8);
  bytes[3] = (uint8_t)instruction;
}

static bool read_ipl3_asset(uint8_t bytes[IPL3_ASSET_BYTES]) {
  const char *path = getenv("T64_TEST_IPL3_PATH");
  FILE *file;
  size_t count;
  int extra;
  int read_error;
  int close_error;

  if (path == NULL || path[0] == '\0') {
    return false;
  }
  file = fopen(path, "rb");
  if (file == NULL) {
    return false;
  }
  count = fread(bytes, 1u, IPL3_ASSET_BYTES, file);
  extra = fgetc(file);
  read_error = ferror(file);
  close_error = fclose(file);
  return count == IPL3_ASSET_BYTES && extra == EOF && read_error == 0 &&
         close_error == 0;
}

static void test_first_cold_register_instruction(void) {
  static const uint32_t instructions[] = {
      UINT32_C(0x8c42000c), /* lw v0,12(v0), the retained RI_SELECT word. */
      UINT32_C(0x3c08a470), /* lui t0,0xa470 */
      UINT32_C(0x34090040), /* ori t1,zero,0x0040 */
      UINT32_C(0xad090004)  /* sw t1,4(t0): RI_CONFIG auto-calibration. */
  };
  const uint32_t start_pc = UINT32_C(0xa4000080);
  const uint32_t start_offset = UINT32_C(0x80);
  const t64_host_allocator allocator = {test_allocate, test_release, NULL};
  const t64_profile_descriptor *profile = NULL;
  uint8_t ipl3_asset[IPL3_ASSET_BYTES];
  t64_machine *machine = NULL;
  bool asset_ok;
  t64_status status;
  size_t index;

  T64_CHECK_INT(t64_profile_lookup("ntsc-cart-4m-openipl3-rdram-b-t1-v1",
                                   &profile),
                T64_STATUS_OK);
  T64_CHECK(profile != NULL);
  if (profile == NULL) {
    return;
  }
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  T64_CHECK(machine != NULL);
  if (machine == NULL) {
    return;
  }
  asset_ok = read_ipl3_asset(ipl3_asset);
  T64_CHECK(asset_ok);
  if (!asset_ok) {
    t64_machine_destroy(machine);
    return;
  }
  status = t64_machine_admit_rom(machine, ipl3_asset, sizeof ipl3_asset);
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) {
    t64_machine_destroy(machine);
    return;
  }

  /* Native Tier-B seed for this authored sequence; not a reset measurement. */
  machine->guest_devices.policy_version = T64_WARM_DEVICE_POLICY_VERSION;
  machine->guest_devices.mi_version = 0u;
  machine->guest_devices.ri_select = 0u;
  machine->guest_devices.mi_version_known = true;
  machine->guest_devices.ri_select_known = true;
  T64_CHECK_INT(t64_machine_trace_set_enabled(machine, true), T64_STATUS_OK);
  for (index = 0u; index < sizeof instructions / sizeof instructions[0];
       index++) {
    write_instruction(machine,
                      start_offset + (uint32_t)(index * sizeof(uint32_t)),
                      instructions[index]);
  }
  machine->cpu.pc = start_pc;
  machine->cpu.pc_full = UINT64_C(0xffffffffa4000080);
  machine->cpu.gpr[0] = 0u;
  machine->cpu.gpr[2] = UINT64_C(0xffffffffa4700000);
  machine->cpu.gpr[8] = 0u;
  machine->cpu.gpr[9] = 0u;

  /* The observed selector read still executes in this independent sequence. */
  T64_CHECK_INT(t64_cpu_step(machine, NULL), T64_STATUS_OK);
  T64_CHECK_INT(machine->cpu.last_pc_full,
                UINT64_C(0xffffffffa4000080));
  T64_CHECK_INT(machine->cpu.last_opcode, UINT32_C(0x8c42000c));
  T64_CHECK_INT(machine->cpu.attempt_outcome, T64_CPU_ATTEMPT_RETIRED);
  T64_CHECK_INT(machine->cpu.gpr[2], 0u);
  T64_CHECK_INT(machine->cpu.retired_instructions, 1u);

  T64_CHECK_INT(t64_cpu_step(machine, NULL), T64_STATUS_OK);
  T64_CHECK_INT(t64_cpu_step(machine, NULL), T64_STATUS_OK);
  T64_CHECK_INT(machine->cpu.gpr[8], UINT64_C(0xffffffffa4700000));
  T64_CHECK_INT(machine->cpu.gpr[9], UINT64_C(0x40));

  /* RED target: RI_CONFIG=0x40 must be accepted through CPU -> bus -> RI. */
  T64_CHECK_INT(t64_cpu_step(machine, NULL), T64_STATUS_OK);
  /* Confirm the pre-feature failure is at this exact authored store. */
  T64_CHECK_INT(machine->cpu.stop_reason, T64_STOP_REASON_CPU_UNSUPPORTED);
  T64_CHECK_INT(machine->cpu.stop_pc_full,
                UINT64_C(0xffffffffa400008c));
  T64_CHECK_INT(machine->cpu.stop_opcode, UINT32_C(0xad090004));
  T64_CHECK_INT(machine->cpu.last_pc_full,
                UINT64_C(0xffffffffa400008c));
  T64_CHECK_INT(machine->cpu.last_opcode, UINT32_C(0xad090004));
  T64_CHECK_INT(machine->cpu.attempt_outcome, T64_CPU_ATTEMPT_RETIRED);
  T64_CHECK_INT(machine->cpu.retired_instructions, 4u);

  t64_machine_destroy(machine);
}

int main(void) {
  test_first_cold_register_instruction();
  return T64_TEST_FINISH("unit.core.rdram-protocol");
}
