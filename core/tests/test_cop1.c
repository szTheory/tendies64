#include "t64_internal.h"
#include "tendies64/t64_machine.h"
#include "t64_test.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define IPL3_ASSET_BYTES 4096u
#define TEST_PC UINT64_C(0xffffffff80001000)
#define STATUS_CU1 UINT64_C(0x20000000)
#define FCR31_CONDITION UINT32_C(0x00800000)

typedef struct { size_t outstanding; } allocator_state;

static bool allocate(void *context, size_t size, size_t alignment,
                     t64_host_storage *storage_out) {
  allocator_state *state = (allocator_state *)context;
  void *data = malloc(size);
  if (state == NULL || storage_out == NULL || data == NULL || alignment == 0u ||
      ((uintptr_t)data % alignment) != 0u) {
    free(data);
    return false;
  }
  storage_out->data = data;
  storage_out->capacity = size;
  storage_out->alignment = alignment;
  state->outstanding++;
  return true;
}

static void release(void *context, t64_host_storage storage) {
  allocator_state *state = (allocator_state *)context;
  if (storage.data != NULL) {
    free(storage.data);
    if (state != NULL) state->outstanding--;
  }
}

static bool read_asset(uint8_t bytes[IPL3_ASSET_BYTES]) {
  const char *path = getenv("T64_TEST_IPL3_PATH");
  FILE *file;
  size_t count;
  int extra;
  if (path == NULL || path[0] == '\0') return false;
  file = fopen(path, "rb");
  if (file == NULL) return false;
  count = fread(bytes, 1u, IPL3_ASSET_BYTES, file);
  extra = fgetc(file);
  return fclose(file) == 0 && count == IPL3_ASSET_BYTES && extra == EOF;
}

static void write_instruction(t64_machine *machine, uint32_t address,
                              uint32_t opcode) {
  machine->ram[address] = (uint8_t)(opcode >> 24);
  machine->ram[address + 1u] = (uint8_t)(opcode >> 16);
  machine->ram[address + 2u] = (uint8_t)(opcode >> 8);
  machine->ram[address + 3u] = (uint8_t)opcode;
}

static t64_machine *make_machine(const t64_host_allocator *allocator,
                                 const t64_profile_descriptor *profile,
                                 const uint8_t *asset) {
  static const uint32_t program[] = {
      UINT32_C(0x45010002), /* BC1T +2 */
      UINT32_C(0x24080001), /* delay: addiu t0,zero,1 */
      UINT32_C(0x240a0003), /* fall-through: addiu t2,zero,3 */
      UINT32_C(0x24090002), /* target: addiu t1,zero,2 */
  };
  t64_machine *machine = NULL;
  size_t index;
  if (t64_machine_create(allocator, profile, &machine) != T64_STATUS_OK ||
      machine == NULL ||
      t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) != T64_STATUS_OK) {
    if (machine != NULL) t64_machine_destroy(machine);
    return NULL;
  }
  machine->cpu.pc = (uint32_t)TEST_PC;
  machine->cpu.pc_full = TEST_PC;
  machine->cpu.cp0[12] |= STATUS_CU1;
  machine->cpu.fcr31 = FCR31_CONDITION;
  for (index = 0u; index < sizeof program / sizeof program[0]; index++) {
    write_instruction(machine, UINT32_C(0x1000) + (uint32_t)(index * 4u),
                      program[index]);
  }
  return machine;
}

int main(void) {
  allocator_state allocation = {0u};
  const t64_host_allocator allocator = {allocate, release, &allocation};
  const t64_profile_descriptor *profile = NULL;
  uint8_t asset[IPL3_ASSET_BYTES];
  t64_machine *machine;
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(1)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_status status;

  if (!read_asset(asset) ||
      t64_profile_lookup("ntsc-cart-4m-openipl3-vr4300-v1", &profile) !=
          T64_STATUS_OK || profile == NULL) {
    puts("1..1\nnot ok 1 - qualified IPL3 and VR4300 profile setup");
    return 1;
  }
  machine = make_machine(&allocator, profile, asset);
  T64_CHECK(machine != NULL);
  if (machine != NULL) {
    status = t64_machine_advance(machine, &request, &result);
    (void)fprintf(stderr,
                   "# COP1 BC1T expected opcode=45010002 retires to PC="
                   "ffffffff80001004 with delay target=ffffffff8000100c; "
                   "observed opcode=%08x status=%u cause=%08x epc=%016llx "
                   "pc=%016llx outcome=%u delay=%u target=%016llx\n",
                   machine->cpu.last_opcode, (unsigned)status,
                   (uint32_t)machine->cpu.cp0[13],
                   (unsigned long long)machine->cpu.cp0[14],
                   (unsigned long long)machine->cpu.pc_full,
                   (unsigned)machine->cpu.attempt_outcome,
                   machine->cpu.delay_slot_pending ? 1u : 0u,
                   (unsigned long long)machine->cpu.delay_target_full);
    T64_CHECK(status == T64_STATUS_OK);
    T64_CHECK(machine->cpu.attempt_outcome == T64_CPU_ATTEMPT_RETIRED);
    T64_CHECK(machine->cpu.pc_full == TEST_PC + UINT64_C(4));
    T64_CHECK(machine->cpu.delay_slot_pending);
    T64_CHECK(machine->cpu.delay_target_full == TEST_PC + UINT64_C(12));
    T64_CHECK(machine->cpu.cp0[13] == UINT64_C(0));
    t64_machine_destroy(machine);
  }
  T64_CHECK(allocation.outstanding == 0u);
  return T64_TEST_FINISH("unit.core.cop1-state");
}
