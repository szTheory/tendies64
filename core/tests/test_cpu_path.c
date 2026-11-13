#include "t64_internal.h"
#include "tendies64/t64_machine.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IPL3_ASSET_BYTES 4096u
#define IPL3_ENTRY_PC UINT32_C(0xa4000040)
#define T64_CP0_CAUSE_SOFTWARE_MASK UINT32_C(0x00000300)
#define T64_CP0_CAUSE_TIMER_MASK UINT32_C(0x00008000)
#define T64_CP0_ENTRYHI UINT32_C(10)
#define T64_CP0_STATUS UINT32_C(12)
#define T64_CP0_TAGLO UINT32_C(28)
#define T64_CP0_TAGHI UINT32_C(29)
#define T64_CPU_CACHE_OPCODE UINT32_C(0x2f)
#define T64_CACHE_INDEX_INVALIDATE_I UINT32_C(0x00)
#define T64_CACHE_INDEX_WRITEBACK_INVALIDATE_D UINT32_C(0x01)
#define T64_CACHE_INDEX_LOAD_TAG_I UINT32_C(0x04)
#define T64_CACHE_INDEX_STORE_TAG_I UINT32_C(0x08)
#define T64_CACHE_HIT_INVALIDATE_I UINT32_C(0x10)
#define T64_CACHE_FILL_I UINT32_C(0x14)

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

static void write_instruction(t64_machine *machine, size_t index,
                              uint32_t opcode) {
  size_t offset = (size_t)T64_SP_BANK_BYTES - IPL3_ASSET_BYTES +
                  (size_t)0x40u + index * 4u;

  machine->sp_dmem[offset] = (uint8_t)(opcode >> 24);
  machine->sp_dmem[offset + 1u] = (uint8_t)(opcode >> 16);
  machine->sp_dmem[offset + 2u] = (uint8_t)(opcode >> 8);
  machine->sp_dmem[offset + 3u] = (uint8_t)opcode;
}

static void write_rdram_instruction(t64_machine *machine,
                                    uint32_t physical_address,
                                    uint32_t opcode) {
  machine->ram[physical_address] = (uint8_t)(opcode >> 24);
  machine->ram[physical_address + 1u] = (uint8_t)(opcode >> 16);
  machine->ram[physical_address + 2u] = (uint8_t)(opcode >> 8);
  machine->ram[physical_address + 3u] = (uint8_t)opcode;
}

static void clear_instruction_index(t64_machine *machine,
                                    uint64_t virtual_address) {
  uint32_t index = (uint32_t)(virtual_address >> 5) &
                   (T64_VR4300_ICACHE_LINE_COUNT - UINT32_C(1));

  machine->cpu.icache[index].valid = false;
}

static uint32_t encode_cvt_d_w(uint32_t source, uint32_t destination) {
  return (UINT32_C(0x11) << 26) | (UINT32_C(0x14) << 21) |
         (source << 11) | (destination << 6) | UINT32_C(0x21);
}

static uint32_t encode_mtc1(uint32_t source, uint32_t destination) {
  return (UINT32_C(0x11) << 26) | (UINT32_C(0x04) << 21) |
         (source << 16) | (destination << 11);
}

static uint32_t encode_ctc1(uint32_t source, uint32_t destination) {
  return (UINT32_C(0x11) << 26) | (UINT32_C(0x06) << 21) |
         (source << 16) | (destination << 11);
}

static uint32_t encode_add_s(uint32_t left, uint32_t right,
                             uint32_t destination) {
  return (UINT32_C(0x11) << 26) | (UINT32_C(0x10) << 21) |
         (right << 16) | (left << 11) | (destination << 6);
}

static uint32_t encode_dmfc1(uint32_t destination, uint32_t source) {
  return (UINT32_C(0x11) << 26) | (UINT32_C(0x01) << 21) |
         (destination << 16) | (source << 11);
}

static uint32_t encode_cop1_memory(uint32_t operation, uint32_t base,
                                   uint32_t reg, uint16_t offset) {
  return (operation << 26) | (base << 21) | (reg << 16) | (uint32_t)offset;
}

static uint32_t read_be32_bytes(const uint8_t *bytes) {
  return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
         ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

static uint32_t encode_cache(uint32_t operation, uint32_t base,
                             uint16_t offset) {
  return (T64_CPU_CACHE_OPCODE << 26) | (base << 21) | (operation << 16) |
         (uint32_t)offset;
}

static uint32_t cacheable_tlb_entry_lo(uint64_t physical_base,
                                       uint32_t cache_algorithm) {
  return (uint32_t)((((physical_base >> 12) << 6) &
                     UINT64_C(0x03ffffc0))) |
         ((cache_algorithm & UINT32_C(7)) << 3) | UINT32_C(0x6);
}

static void install_tlb_pair(t64_machine *machine, uint64_t virtual_base,
                             uint64_t physical_base,
                             uint32_t cache_algorithm) {
  t64_vr4300_tlb_pair *entry;
  size_t index;

  machine->cpu.cp0[T64_CP0_ENTRYHI] = 0u;
  for (index = 0u; index < T64_VR4300_TLB_ENTRY_COUNT; index++) {
    machine->cpu.tlb[index] = (t64_vr4300_tlb_pair){0};
    machine->cpu.tlb[index].asid = UINT32_C(0xff);
  }
  entry = &machine->cpu.tlb[0];
  entry->page_mask = 0u;
  entry->vpn2 = virtual_base & UINT64_C(0x000000ffffffe000);
  entry->region = (virtual_base >> 62) & UINT64_C(3);
  entry->asid = 0u;
  entry->entry_lo0 =
      cacheable_tlb_entry_lo(physical_base, cache_algorithm);
  entry->entry_lo1 =
      cacheable_tlb_entry_lo(physical_base, cache_algorithm);
}

static bool find_compat_zero_data_line(const t64_machine *machine,
                                       uint32_t line_bytes,
                                       uint32_t *address_out) {
  const uint64_t stride = UINT64_C(0x00010000);
  uint64_t candidate;

  if (machine == NULL || address_out == NULL || line_bytes == 0u ||
      line_bytes > T64_VR4300_ICACHE_LINE_BYTES) {
    return false;
  }
  candidate = ((uint64_t)machine->profile.rdram_bytes + stride - UINT64_C(1)) &
              ~(stride - UINT64_C(1));
  for (; candidate + line_bytes <= UINT64_C(0x100000000);
       candidate += stride) {
    if (t64_cpu_compat_zero_data_range(machine, (uint32_t)candidate,
                                       line_bytes)) {
      *address_out = (uint32_t)candidate;
      return true;
    }
  }
  return false;
}

static bool install_program(t64_machine *machine,
                            const uint8_t asset[IPL3_ASSET_BYTES],
                            const uint32_t *program, size_t instruction_count) {
  size_t i;

  if (machine == NULL || asset == NULL || program == NULL ||
      instruction_count > (T64_SP_BANK_BYTES - 0x40u) / 4u ||
      t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK) {
    return false;
  }
  (void)memset(machine->cpu.gpr, 0, sizeof machine->cpu.gpr);
  machine->cpu.pc = IPL3_ENTRY_PC;
  for (i = 0u; i < instruction_count; i++) {
    write_instruction(machine, i, program[i]);
  }
  return true;
}

static bool test_vr4300_break_enters_rdram_handler(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  const uint32_t break_pc = UINT32_C(0x80001000);
  const uint32_t handler_pc = UINT32_C(0x80000180);
  const uint32_t store_value = UINT32_C(0xdeadbeef);
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(2)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_status status;
  uint32_t cause;
  uint32_t status_register;
  uint32_t stored_value;
  const t64_vr4300_dcache_line *stored_line;

  if (machine == NULL || asset == NULL ||
      t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK) {
    (void)puts("# new-profile exception fixture setup failed");
    return false;
  }
  write_rdram_instruction(machine, UINT32_C(0x1000), UINT32_C(0x0000000d));
  write_rdram_instruction(machine, UINT32_C(0x0180), UINT32_C(0xad280000));
  machine->cpu.pc = break_pc;
  machine->cpu.pc_full = t64_cpu_canonical_address(break_pc);
  machine->cpu.gpr[8] = store_value;
  machine->cpu.gpr[9] = UINT64_C(0xffffffff80002000);

  status = t64_machine_advance(machine, &request, &result);
  cause = (uint32_t)machine->cpu.cp0[13];
  status_register = (uint32_t)machine->cpu.cp0[12];
  stored_line = &machine->cpu.dcache[(UINT32_C(0x2000) >> 4) &
                                    (T64_VR4300_DCACHE_LINE_COUNT - 1u)];
  stored_value = read_be32_bytes(stored_line->data);
  if (status != T64_STATUS_OK ||
      result.reason != T64_STOP_REASON_BOUND ||
      result.tick != UINT64_C(2) ||
      machine->cpu.retired_instructions != UINT64_C(1) ||
      machine->cpu.pc != handler_pc + UINT32_C(4) ||
      machine->cpu.cp0[14] != t64_cpu_canonical_address(break_pc) ||
      (cause & UINT32_C(0x8000007c)) != (UINT32_C(9) << 2) ||
      (status_register & UINT32_C(2)) == 0u ||
      stored_value != store_value || !stored_line->valid ||
      !stored_line->dirty ||
      stored_line->physical_tag != UINT32_C(0x00002000) ||
      read_be32_bytes(machine->ram + UINT32_C(0x2000)) != 0u) {
    (void)printf("# BREAK handler status=%u reason=%u tick=%llu retired=%llu "
                 "pc=%016llx cause=%08x epc=%016llx status=%08x "
                 "store=%08x line=%u/%u/%08x/%08x backing=%08x "
                 "base=%016llx\n",
                 (unsigned)status, result.reason,
                 (unsigned long long)result.tick,
                 (unsigned long long)machine->cpu.retired_instructions,
                 (unsigned long long)machine->cpu.pc_full, cause,
                 (unsigned long long)machine->cpu.cp0[14], status_register,
                 stored_value, stored_line->valid ? 1u : 0u,
                 stored_line->dirty ? 1u : 0u, stored_line->physical_tag,
                 read_be32_bytes(stored_line->data),
                 read_be32_bytes(machine->ram + UINT32_C(0x2000)),
                 (unsigned long long)machine->cpu.gpr[9]);
    return false;
  }
  return true;
}

static bool test_vr4300_cvt_d_w_is_machine_local(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  const uint64_t cvt_pc =
      t64_cpu_canonical_address(UINT32_C(0x80001000));
  const uint64_t destination_sentinel = UINT64_C(0x1122334455667788);
  const uint32_t coprocessor_usable = UINT32_C(1) << 29;
  const uint32_t instruction_a = encode_cvt_d_w(1u, 2u);
  const uint32_t instruction_b = encode_cvt_d_w(3u, 4u);
  const uint32_t instruction_fr0 = encode_cvt_d_w(0u, 2u);
  const uint64_t expected_a = UINT64_C(0xc1e0000000000000);
  const uint64_t expected_b = UINT64_C(0x405ec00000000000);
  allocator_state other_allocator_state = {0u};
  const t64_host_allocator other_allocator = {
      test_allocate, test_release, &other_allocator_state};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *other = NULL;
  t64_status status;
  uint64_t retired_before;
  const uint64_t status_fr = UINT64_C(1) << 26;

  if (machine == NULL || asset == NULL ||
      t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK ||
      t64_profile_lookup("ntsc-cart-4m-openipl3-vr4300-v1", &profile) !=
          T64_STATUS_OK ||
      profile == NULL ||
      t64_machine_create(&other_allocator, profile, &other) != T64_STATUS_OK ||
      other == NULL ||
      t64_machine_admit_rom(other, asset, IPL3_ASSET_BYTES) != T64_STATUS_OK) {
    (void)puts("# CVT.D.W fixture setup failed");
    if (other != NULL) {
      t64_machine_destroy(other);
    }
    return false;
  }

  write_rdram_instruction(machine, UINT32_C(0x1000), instruction_a);
  clear_instruction_index(machine, cvt_pc);
  machine->cpu.pc = (uint32_t)cvt_pc;
  machine->cpu.pc_full = cvt_pc;
  machine->cpu.cp0[12] |= coprocessor_usable;
  machine->cpu.fpr[1] = UINT32_C(0x80000000);
  machine->cpu.fpr[2] = destination_sentinel;
  machine->cpu.fcr31 = 0u;

  write_rdram_instruction(other, UINT32_C(0x1000), instruction_b);
  clear_instruction_index(other, cvt_pc);
  other->cpu.pc = (uint32_t)cvt_pc;
  other->cpu.pc_full = cvt_pc;
  other->cpu.cp0[12] |= coprocessor_usable;
  other->cpu.fpr[3] = UINT32_C(123);
  other->cpu.fpr[4] = destination_sentinel;
  other->cpu.fcr31 = UINT32_C(0x41); /* invalid sticky flag plus toward-zero RM */

  status = t64_cpu_step(machine, NULL);
  if (status != T64_STATUS_OK || machine->cpu.fpr[2] != expected_a ||
      machine->cpu.fcr31 != 0u ||
      machine->cpu.last_opcode != instruction_a ||
      machine->cpu.attempt_outcome != T64_CPU_ATTEMPT_RETIRED ||
      machine->cpu.retired_instructions != UINT64_C(1)) {
    (void)printf("# CVT.D.W -2^31 expected=%016llx observed=%016llx "
                 "status=%u fcr31=%08x cause=%08x last_opcode=%08x "
                 "pc=%016llx outcome=%u retired=%llu\n",
                 (unsigned long long)expected_a,
                 (unsigned long long)machine->cpu.fpr[2], (unsigned)status,
                 (unsigned)machine->cpu.fcr31,
                 (unsigned)machine->cpu.cp0[13],
                 (unsigned)machine->cpu.last_opcode,
                 (unsigned long long)machine->cpu.pc_full,
                 (unsigned)machine->cpu.attempt_outcome,
                 (unsigned long long)machine->cpu.retired_instructions);
    t64_machine_destroy(other);
    return false;
  }

  status = t64_cpu_step(other, NULL);
  if (status != T64_STATUS_OK || other->cpu.fpr[4] != expected_b ||
      other->cpu.fcr31 != UINT32_C(0x41) ||
      other->cpu.attempt_outcome != T64_CPU_ATTEMPT_RETIRED ||
      machine->cpu.fpr[2] != expected_a || machine->cpu.fcr31 != 0u) {
    (void)printf("# interleaved CVT.D.W expected B=%016llx observed B=%016llx "
                 "A=%016llx/%08x B-fcr31=%08x status=%u\n",
                 (unsigned long long)expected_b,
                 (unsigned long long)other->cpu.fpr[4],
                 (unsigned long long)machine->cpu.fpr[2],
                 (unsigned)machine->cpu.fcr31, (unsigned)other->cpu.fcr31,
                 (unsigned)status);
    t64_machine_destroy(other);
    return false;
  }

  machine->cpu.pc = (uint32_t)cvt_pc;
  machine->cpu.pc_full = cvt_pc;
  machine->cpu.cp0[12] &= ~status_fr;
  write_rdram_instruction(machine, UINT32_C(0x1000), instruction_fr0);
  clear_instruction_index(machine, cvt_pc);
  machine->cpu.fpr[0] = UINT32_C(0x80000000);
  machine->cpu.fpr[2] = destination_sentinel;
  machine->cpu.fpr[3] = UINT64_C(0x99aabbccddeeff00);
  retired_before = machine->cpu.retired_instructions;
  status = t64_cpu_step(machine, NULL);
  if (status != T64_STATUS_OK ||
      machine->cpu.fpr[2] != UINT64_C(0x1122334400000000) ||
      machine->cpu.fpr[3] != UINT64_C(0x99aabbccc1e00000) ||
      machine->cpu.fcr31 != 0u ||
      machine->cpu.last_opcode != instruction_fr0 ||
      machine->cpu.retired_instructions != retired_before + UINT64_C(1) ||
      machine->cpu.attempt_outcome != T64_CPU_ATTEMPT_RETIRED) {
    (void)printf("# FR=0 CVT.D.W expected paired FPR2/FPR3="
                 "1122334400000000/99aabbccc1e00000 observed "
                 "%016llx/%016llx fcr31=%08x opcode=%08x status=%u "
                 "outcome=%u retired=%llu/%llu\n",
                 (unsigned long long)machine->cpu.fpr[2],
                 (unsigned long long)machine->cpu.fpr[3],
                 (unsigned)machine->cpu.fcr31,
                 (unsigned)machine->cpu.last_opcode,
                 (unsigned)status,
                 (unsigned)machine->cpu.attempt_outcome,
                 (unsigned long long)machine->cpu.retired_instructions,
                 (unsigned long long)retired_before);
    t64_machine_destroy(other);
    return false;
  }

  other->cpu.pc = (uint32_t)cvt_pc;
  other->cpu.pc_full = cvt_pc;
  other->cpu.cp0[12] &= ~(uint64_t)coprocessor_usable;
  other->cpu.fpr[4] = destination_sentinel;
  retired_before = other->cpu.retired_instructions;
  status = t64_cpu_step(other, NULL);
  if (status != T64_STATUS_OK || other->cpu.fpr[4] != destination_sentinel ||
      other->cpu.fcr31 != UINT32_C(0x41) ||
      ((uint32_t)other->cpu.cp0[13] & UINT32_C(0x7c)) !=
          (UINT32_C(11) << 2) ||
      (((uint32_t)other->cpu.cp0[13] >> 28) & UINT32_C(3)) != 1u ||
      other->cpu.cp0[14] != cvt_pc ||
      other->cpu.retired_instructions != retired_before ||
      other->cpu.attempt_outcome != T64_CPU_ATTEMPT_GUEST_EXCEPTION) {
    (void)printf("# CU1-disabled CVT.D.W expected destination=%016llx "
                 "observed=%016llx cause=%08x epc=%016llx fcr31=%08x "
                 "status=%u retired=%llu/%llu\n",
                 (unsigned long long)destination_sentinel,
                 (unsigned long long)other->cpu.fpr[4],
                 (unsigned)other->cpu.cp0[13],
                 (unsigned long long)other->cpu.cp0[14],
                 (unsigned)other->cpu.fcr31, (unsigned)status,
                 (unsigned long long)other->cpu.retired_instructions,
                 (unsigned long long)retired_before);
    t64_machine_destroy(other);
    return false;
  }

  t64_machine_destroy(other);
  return other_allocator_state.outstanding == 0u;
}

static bool test_vr4300_cop1_transfer_conversion_memory_tracer(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  const uint64_t virtual_code = UINT64_C(0xffffffff80001000);
  const uint64_t virtual_data = UINT64_C(0xffffffff80002000);
  const uint64_t expected_value = UINT64_C(0xc1e0000000000000);
  const uint32_t status_cu1_fr = (UINT32_C(1) << 29) | (UINT32_C(1) << 26);
  const uint32_t program[] = {
      encode_mtc1(8u, 1u),
      encode_cvt_d_w(1u, 2u),
      encode_cop1_memory(UINT32_C(0x3d), 9u, 2u, 0u),
      encode_cop1_memory(UINT32_C(0x35), 9u, 3u, 0u),
      encode_dmfc1(10u, 3u),
  };
  const uint64_t unusable_pc = UINT64_C(0xffffffff80003000);
  const uint32_t unusable_opcode = encode_mtc1(8u, 5u);
  const uint32_t handler_opcode = UINT32_C(0x240a005a);
  allocator_state handler_allocator_state = {0u};
  const t64_host_allocator handler_allocator = {
      test_allocate, test_release, &handler_allocator_state};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *handler_machine = NULL;
  const t64_vr4300_dcache_line *data_line;
  t64_status status;
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_mutated[T64_DIGEST_SIZE];
  uint8_t digest_restored[T64_DIGEST_SIZE];
  uint32_t stored_high;
  uint32_t stored_low;
  uint64_t retired_before;
  size_t index;

  if (machine == NULL || asset == NULL ||
      t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK) {
    (void)puts("# COP1 tracer fixture setup failed");
    return false;
  }
  (void)memset(machine->ram + UINT32_C(0x2000), 0, 16u);
  for (index = 0u; index < sizeof program / sizeof program[0]; index++) {
    write_rdram_instruction(machine, UINT32_C(0x1000) +
                                         (uint32_t)(index * 4u),
                            program[index]);
    clear_instruction_index(machine, virtual_code + (uint64_t)(index * 4u));
  }
  machine->cpu.pc = UINT32_C(0x80001000);
  machine->cpu.pc_full = virtual_code;
  machine->cpu.cp0[12] |= status_cu1_fr;
  machine->cpu.gpr[8] = UINT64_C(0x80000000);
  machine->cpu.gpr[9] = virtual_data;
  for (index = 0u; index < sizeof program / sizeof program[0]; index++) {
    status = t64_cpu_step(machine, NULL);
    if (status != T64_STATUS_OK ||
        machine->cpu.last_opcode != program[index] ||
        machine->cpu.attempt_outcome != T64_CPU_ATTEMPT_RETIRED) {
      (void)printf("# COP1 tracer step=%zu expected opcode=%08x retired=1 "
                   "observed opcode=%08x status=%u cause=%08x "
                   "pc=%016llx outcome=%u retired=%llu\n",
                   index, program[index], machine->cpu.last_opcode,
                   (unsigned)status, (unsigned)machine->cpu.cp0[13],
                   (unsigned long long)machine->cpu.last_pc_full,
                   (unsigned)machine->cpu.attempt_outcome,
                   (unsigned long long)machine->cpu.retired_instructions);
      return false;
    }
  }
  data_line = &machine->cpu.dcache[(UINT32_C(0x2000) >> 4) &
                                   (T64_VR4300_DCACHE_LINE_COUNT - 1u)];
  stored_high = read_be32_bytes(data_line->data);
  stored_low = read_be32_bytes(data_line->data + 4u);
  if (machine->cpu.fpr[2] != expected_value ||
      machine->cpu.fpr[3] != expected_value ||
      machine->cpu.gpr[10] != expected_value || !data_line->valid ||
      !data_line->dirty || data_line->physical_tag != UINT32_C(0x2000) ||
      stored_high != UINT32_C(0xc1e00000) ||
      stored_low != UINT32_C(0x00000000)) {
    (void)printf("# COP1 tracer expected fpr2/fpr3/gpr10=%016llx cache="
                 "%08x%08x valid/dirty/tag=%u/%u/%08x observed="
                 "%016llx/%016llx/%016llx cache=%08x%08x %u/%u/%08x\n",
                 (unsigned long long)expected_value, UINT32_C(0xc1e00000),
                 UINT32_C(0), data_line->valid ? 1u : 0u,
                 data_line->dirty ? 1u : 0u, UINT32_C(0x2000),
                 (unsigned long long)machine->cpu.fpr[2],
                 (unsigned long long)machine->cpu.fpr[3],
                 (unsigned long long)machine->cpu.gpr[10],
                 stored_high, stored_low,
                 data_line->valid ? 1u : 0u,
                 data_line->dirty ? 1u : 0u, data_line->physical_tag);
    return false;
  }
  if (t64_machine_digest(machine, digest_before) != T64_STATUS_OK) {
    (void)puts("# COP1 digest baseline failed");
    return false;
  }
  machine->cpu.fpr[20] ^= UINT64_C(1);
  status = t64_machine_digest(machine, digest_mutated);
  machine->cpu.fpr[20] ^= UINT64_C(1);
  if (status != T64_STATUS_OK ||
      t64_machine_digest(machine, digest_restored) != T64_STATUS_OK ||
      memcmp(digest_before, digest_mutated, sizeof digest_before) == 0 ||
      memcmp(digest_before, digest_restored, sizeof digest_before) != 0) {
    (void)puts("# COP1 FPR digest change/restore was not deterministic");
    return false;
  }

  if (t64_profile_lookup("ntsc-cart-4m-openipl3-vr4300-v1", &profile) !=
          T64_STATUS_OK ||
      profile == NULL ||
      t64_machine_create(&handler_allocator, profile, &handler_machine) !=
          T64_STATUS_OK ||
      handler_machine == NULL ||
      t64_machine_admit_rom(handler_machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK) {
    (void)puts("# CU1-disabled handler fixture setup failed");
    if (handler_machine != NULL) {
      t64_machine_destroy(handler_machine);
    }
    return false;
  }
  write_rdram_instruction(handler_machine, UINT32_C(0x3000),
                          unusable_opcode);
  write_rdram_instruction(handler_machine, UINT32_C(0x0180), handler_opcode);
  clear_instruction_index(handler_machine, unusable_pc);
  clear_instruction_index(handler_machine,
                          UINT64_C(0xffffffff80000180));
  handler_machine->cpu.pc = UINT32_C(0x80003000);
  handler_machine->cpu.pc_full = unusable_pc;
  handler_machine->cpu.cp0[12] |= status_cu1_fr;
  handler_machine->cpu.cp0[12] &= ~(uint64_t)(UINT32_C(1) << 29);
  handler_machine->cpu.gpr[8] = UINT64_C(0x12345678);
  handler_machine->cpu.fpr[5] = UINT64_C(0x1122334455667788);
  retired_before = handler_machine->cpu.retired_instructions;
  status = t64_cpu_step(handler_machine, NULL);
  if (status != T64_STATUS_OK ||
      handler_machine->cpu.last_opcode != unusable_opcode ||
      handler_machine->cpu.fpr[5] != UINT64_C(0x1122334455667788) ||
      ((uint32_t)handler_machine->cpu.cp0[13] & UINT32_C(0x7c)) !=
          (UINT32_C(11) << 2) ||
      (((uint32_t)handler_machine->cpu.cp0[13] >> 28) & UINT32_C(3)) != 1u ||
      handler_machine->cpu.cp0[14] != unusable_pc ||
      handler_machine->cpu.retired_instructions != retired_before ||
      handler_machine->cpu.attempt_outcome !=
          T64_CPU_ATTEMPT_GUEST_EXCEPTION ||
      handler_machine->cpu.pc != UINT32_C(0x80000180)) {
    (void)printf("# CU1-disabled MTC1 expected cause=0000002c epc=%016llx "
                 "pc=ffffffff80000180 destination=1122334455667788 "
                 "observed opcode=%08x cause=%08x epc=%016llx "
                 "pc=%016llx destination=%016llx status=%u outcome=%u\n",
                 (unsigned long long)unusable_pc,
                 handler_machine->cpu.last_opcode,
                 (unsigned)handler_machine->cpu.cp0[13],
                 (unsigned long long)handler_machine->cpu.cp0[14],
                 (unsigned long long)handler_machine->cpu.pc_full,
                 (unsigned long long)handler_machine->cpu.fpr[5],
                 (unsigned)status,
                 (unsigned)handler_machine->cpu.attempt_outcome);
    t64_machine_destroy(handler_machine);
    return false;
  }
  status = t64_cpu_step(handler_machine, NULL);
  if (status != T64_STATUS_OK || handler_machine->cpu.gpr[10] !=
                                     UINT64_C(0x5a)) {
    (void)printf("# CU1 handler expected gpr10=000000000000005a observed="
                 "%016llx status=%u pc=%016llx opcode=%08x\n",
                 (unsigned long long)handler_machine->cpu.gpr[10],
                 (unsigned)status,
                 (unsigned long long)handler_machine->cpu.last_pc_full,
                 handler_machine->cpu.last_opcode);
    t64_machine_destroy(handler_machine);
    return false;
  }
  t64_machine_destroy(handler_machine);
  return handler_allocator_state.outstanding == 0u;
}

static bool test_vr4300_cop1_rounding_trap_recovery_and_e_cause(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  const uint32_t add_opcode = encode_add_s(1u, 2u, 3u);
  const uint32_t invalid_format =
      (UINT32_C(0x11) << 26) | (UINT32_C(0x10) << 21) |
      (UINT32_C(8) << 11) | (UINT32_C(9) << 6) | UINT32_C(0x20);
  const uint32_t handler[] = {encode_ctc1(8u, 31u), /* CTC1 t0,FCR31 */
                              UINT32_C(0x42000018)}; /* ERET */
  const uint64_t guest_pc = UINT64_C(0xffffffff80001000);
  const uint64_t handler_pc = UINT64_C(0xffffffff80000180);
  const uint32_t status_cu1_fr =
      (UINT32_C(1) << 29) | (UINT32_C(1) << 26);
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(1)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_status status;
  size_t i;

  if (machine == NULL || asset == NULL ||
      t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK) {
    (void)puts("# COP1 arithmetic fixture setup failed");
    return false;
  }
  write_rdram_instruction(machine, UINT32_C(0x1000), add_opcode);
  write_rdram_instruction(machine, UINT32_C(0x1004), invalid_format);
  clear_instruction_index(machine, guest_pc);
  clear_instruction_index(machine, guest_pc + UINT64_C(4));
  for (i = 0u; i < sizeof handler / sizeof handler[0]; i++) {
    write_rdram_instruction(machine, UINT32_C(0x0180) + (uint32_t)(i * 4u),
                            handler[i]);
    clear_instruction_index(machine, handler_pc + (uint64_t)(i * 4u));
  }
  machine->cpu.pc = UINT32_C(0x80001000);
  machine->cpu.pc_full = guest_pc;
  machine->cpu.cp0[12] = status_cu1_fr;
  machine->cpu.gpr[8] = UINT64_C(0); /* Clear Cause/Enable before retry. */
  machine->cpu.fpr[1] = UINT64_C(0x3f800000); /* 1 */
  machine->cpu.fpr[2] = UINT64_C(0x33800000); /* 2^-24, half an ulp at 1 */
  machine->cpu.fpr[3] = UINT64_C(0x1122334455667788);
  machine->cpu.fpr[9] = UINT64_C(0xaabbccddeeff0011);
  machine->cpu.fcr31 = UINT32_C(0x00010090); /* old V cause, O flag, I enable */

  status = t64_machine_advance(machine, &request, &result);
  if (status != T64_STATUS_OK || result.reason != T64_STOP_REASON_BOUND ||
      result.tick != UINT64_C(1) || machine->cpu.last_opcode != add_opcode ||
      machine->cpu.last_pc_full != guest_pc ||
      machine->cpu.attempt_outcome != T64_CPU_ATTEMPT_GUEST_EXCEPTION ||
      machine->cpu.pc_full != handler_pc ||
      machine->cpu.cp0[14] != guest_pc ||
      ((uint32_t)machine->cpu.cp0[13] & UINT32_C(0x7c)) !=
          (UINT32_C(15) << 2) ||
      (machine->cpu.fcr31 & UINT32_C(0x0003f000)) != UINT32_C(0x00001000) ||
      (machine->cpu.fcr31 & UINT32_C(0x0000007c)) != UINT32_C(0x00000010) ||
      machine->cpu.fpr[3] != UINT64_C(0x1122334455667788)) {
    (void)printf("# inexact trap expected opcode=%08x pc=%016llx cp0-cause=0000003c "
                 "fcr31-cause/flags=00001000/10 fpr3=1122334455667788; "
                 "observed opcode=%08x pc=%016llx epc=%016llx "
                 "cp0-cause=%08x fcr31=%08x fpr3=%016llx "
                 "status=%u outcome=%u\n",
                 add_opcode, (unsigned long long)guest_pc,
                 machine->cpu.last_opcode,
                 (unsigned long long)machine->cpu.last_pc_full,
                 (unsigned long long)machine->cpu.cp0[14],
                 (uint32_t)machine->cpu.cp0[13], machine->cpu.fcr31,
                 (unsigned long long)machine->cpu.fpr[3], (unsigned)status,
                 machine->cpu.attempt_outcome);
    return false;
  }

  request.value = UINT64_C(4);
  status = t64_machine_advance(machine, &request, &result);
  if (status != T64_STATUS_OK || result.reason != T64_STOP_REASON_BOUND ||
      result.tick != UINT64_C(4) || machine->cpu.last_opcode != add_opcode ||
      machine->cpu.last_pc_full != guest_pc ||
      machine->cpu.attempt_outcome != T64_CPU_ATTEMPT_RETIRED ||
      machine->cpu.fpr[3] != UINT64_C(0x112233443f800000) ||
      (machine->cpu.fcr31 & UINT32_C(0x0003f000)) != UINT32_C(0x00001000) ||
      (machine->cpu.fcr31 & UINT32_C(0x0000007c)) != UINT32_C(0x00000004)) {
    (void)printf("# resumed ADD.S expected fpr3=112233443f800000 "
                 "fcr31 cause/flags=00001000/04 observed=%016llx fcr31=%08x "
                 "opcode=%08x pc=%016llx outcome=%u status=%u\n",
                 (unsigned long long)machine->cpu.fpr[3], machine->cpu.fcr31,
                 machine->cpu.last_opcode,
                 (unsigned long long)machine->cpu.last_pc_full,
                 machine->cpu.attempt_outcome, (unsigned)status);
    return false;
  }

  request.value = UINT64_C(5);
  status = t64_machine_advance(machine, &request, &result);
  if (status != T64_STATUS_OK || result.reason != T64_STOP_REASON_BOUND ||
      result.tick != UINT64_C(5) ||
      machine->cpu.last_opcode != invalid_format ||
      machine->cpu.last_pc_full != guest_pc + UINT64_C(4) ||
      machine->cpu.attempt_outcome != T64_CPU_ATTEMPT_GUEST_EXCEPTION ||
      machine->cpu.pc_full != handler_pc ||
      machine->cpu.cp0[14] != guest_pc + UINT64_C(4) ||
      (machine->cpu.fcr31 & UINT32_C(0x0003f000)) != UINT32_C(0x00020000) ||
      (machine->cpu.fcr31 & UINT32_C(0x0000007c)) != UINT32_C(0x00000004) ||
      machine->cpu.fpr[9] != UINT64_C(0xaabbccddeeff0011)) {
    (void)printf("# E-only expected opcode=%08x pc=%016llx cause=00020000 "
                 "flags=04 destination=aabbccddeeff0011 observed opcode=%08x "
                 "pc=%016llx epc=%016llx cp0-cause=%08x fcr31=%08x "
                 "fpr9=%016llx status=%u outcome=%u\n",
                 invalid_format, (unsigned long long)(guest_pc + UINT64_C(4)),
                 machine->cpu.last_opcode,
                 (unsigned long long)machine->cpu.last_pc_full,
                 (unsigned long long)machine->cpu.cp0[14],
                 (uint32_t)machine->cpu.cp0[13], machine->cpu.fcr31,
                 (unsigned long long)machine->cpu.fpr[9], (unsigned)status,
                 machine->cpu.attempt_outcome);
    return false;
  }
  return true;
}

static bool test_vr4300_instruction_cache_stale_then_invalidated(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {
      UINT32_C(0x24080001), /* addiu t0,zero,1; backing changes to 2 */
      UINT32_C(0xac890000), /* sw t1,0(a0): KSEG1 uncached alias */
      UINT32_C(0x254a0001), /* addiu t2,t2,1 */
      UINT32_C(0x114bfffc), /* beq t2,t3,first instruction */
      UINT32_C(0x00000000), /* delay slot */
      UINT32_C(0xbcb00000), /* CACHE Hit_Invalidate I,0(a1) */
      UINT32_C(0x08000400), /* j 0x80001000 */
      UINT32_C(0x00000000), /* delay slot */
  };
  const uint32_t virtual_pc = UINT32_C(0x80001000);
  const uint32_t changed_instruction = UINT32_C(0x24080002);
  t64_status status = T64_STATUS_OK;
  size_t index;

  if (machine == NULL || asset == NULL ||
      t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK) {
    (void)puts("# I-cache tracer fixture setup failed");
    return false;
  }
  for (index = 0u; index < sizeof program / sizeof program[0]; index++) {
    write_rdram_instruction(machine, UINT32_C(0x1000) +
                                        (uint32_t)(index * 4u),
                            program[index]);
  }
  machine->cpu.pc = virtual_pc;
  machine->cpu.pc_full = t64_cpu_canonical_address(virtual_pc);
  machine->cpu.gpr[4] = t64_cpu_canonical_address(UINT32_C(0xa0001000));
  machine->cpu.gpr[9] = (uint64_t)changed_instruction;
  machine->cpu.gpr[11] = UINT64_C(1);
  machine->cpu.gpr[5] = t64_cpu_canonical_address(virtual_pc);

  /* Retire the original instruction, uncached guest store, loop counter,
   * branch, delay slot, and second execution of the now-stale instruction. */
  for (index = 0u; index < 6u; index++) {
    status = t64_cpu_step(machine, NULL);
    if (status != T64_STATUS_OK) {
      (void)printf("# I-cache tracer stopped before stale assertion at step=%zu "
                   "status=%u pc=%016llx last_pc=%016llx outcome=%u\n",
                   index + 1u, (unsigned)status,
                   (unsigned long long)machine->cpu.pc_full,
                   (unsigned long long)machine->cpu.last_pc_full,
                   machine->cpu.attempt_outcome);
      return false;
    }
  }
  if (machine->cpu.gpr[8] != UINT64_C(1) ||
      machine->ram[0x1000u] != UINT8_C(0x24) ||
      machine->ram[0x1001u] != UINT8_C(0x08) ||
      machine->ram[0x1002u] != UINT8_C(0x00) ||
      machine->ram[0x1003u] != UINT8_C(0x02)) {
    (void)printf("# I-cache stale assertion expected gpr8=0000000000000001 "
                 "backing=24080002; observed gpr8=%016llx "
                 "backing=%02x%02x%02x%02x last_pc=%016llx "
                 "outcome=%u\n",
                 (unsigned long long)machine->cpu.gpr[8],
                 machine->ram[0x1000u], machine->ram[0x1001u],
                 machine->ram[0x1002u], machine->ram[0x1003u],
                 (unsigned long long)machine->cpu.last_pc_full,
                 machine->cpu.attempt_outcome);
    return false;
  }

  for (index = 0u; index < 8u; index++) {
    status = t64_cpu_step(machine, NULL);
    if (status != T64_STATUS_OK) {
      (void)printf("# I-cache invalidation/reload stopped at step=%zu "
                   "status=%u pc=%016llx last_pc=%016llx outcome=%u\n",
                   index + 7u, (unsigned)status,
                   (unsigned long long)machine->cpu.pc_full,
                   (unsigned long long)machine->cpu.last_pc_full,
                   machine->cpu.attempt_outcome);
      return false;
    }
  }
  if (machine->cpu.gpr[8] != UINT64_C(2) ||
      machine->cpu.last_pc_full != t64_cpu_canonical_address(virtual_pc)) {
    (void)printf("# I-cache reload expected gpr8=2 from opcode=%08x "
                 "last_pc=%016llx; observed gpr8=%016llx "
                 "last_pc=%016llx\n",
                 changed_instruction,
                 (unsigned long long)t64_cpu_canonical_address(virtual_pc),
                 (unsigned long long)machine->cpu.gpr[8],
                 (unsigned long long)machine->cpu.last_pc_full);
    return false;
  }
  return true;
}

static bool test_vr4300_cached_store_waits_for_hit_writeback(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  const uint32_t data_physical = UINT32_C(0x00002000);
  const uint32_t data_virtual = UINT32_C(0x80002000);
  const uint32_t index = (data_physical >> 4) &
                         (T64_VR4300_DCACHE_LINE_COUNT - 1u);
  const uint32_t expected = UINT32_C(0x12345678);
  const uint32_t writeback_instruction =
      UINT32_C(0xbc000000) | (UINT32_C(5) << 21) |
      (UINT32_C(0x19) << 16);
  t64_vr4300_dcache_line *line;
  uint32_t backing;
  t64_status status;

  if (machine == NULL || asset == NULL ||
      t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK) {
    (void)puts("# D-cache tracer fixture setup failed");
    return false;
  }
  write_rdram_instruction(machine, UINT32_C(0x1000), UINT32_C(0xac880000));
  write_rdram_instruction(machine, UINT32_C(0x1004), writeback_instruction);
  machine->cpu.gpr[4] = t64_cpu_canonical_address(data_virtual);
  machine->cpu.gpr[5] = t64_cpu_canonical_address(data_virtual);
  machine->cpu.gpr[8] = expected;
  machine->cpu.linked_valid = true;
  machine->cpu.linked_physical_address = UINT32_C(0x00004000);
  machine->cpu.linked_width = UINT32_C(4);
  machine->cpu.pc = UINT32_C(0x80001000);
  machine->cpu.pc_full = t64_cpu_canonical_address(machine->cpu.pc);

  status = t64_cpu_step(machine, NULL);
  line = &machine->cpu.dcache[index];
  backing = ((uint32_t)machine->ram[data_physical] << 24) |
            ((uint32_t)machine->ram[data_physical + 1u] << 16) |
            ((uint32_t)machine->ram[data_physical + 2u] << 8) |
            (uint32_t)machine->ram[data_physical + 3u];
  if (status != T64_STATUS_OK || backing != 0u || !line->valid ||
      !line->dirty || line->physical_tag != data_physical ||
      !machine->cpu.linked_valid ||
      machine->cpu.linked_physical_address != UINT32_C(0x00004000) ||
      machine->cpu.linked_width != UINT32_C(4) ||
      (((uint32_t)line->data[0] << 24) |
       ((uint32_t)line->data[1] << 16) |
       ((uint32_t)line->data[2] << 8) |
       (uint32_t)line->data[3]) != expected) {
    (void)printf("# cached-store expected backing=00000000 dirty-line="
                 "valid:1 dirty:1 tag:%08x data:%08x; observed status=%u "
                 "backing=%08x valid=%u dirty=%u tag=%08x data=%08x "
                 "pc=%016llx last_pc=%016llx opcode=%08x outcome=%u "
                 "retired=%llu stopped=%u gpr4=%016llx gpr8=%016llx "
                 "cause=%08x badvaddr=%016llx link=%u/%08x/%u\n",
                 data_physical, expected, (unsigned)status, backing,
                 line->valid ? 1u : 0u, line->dirty ? 1u : 0u,
                 line->physical_tag,
                 ((uint32_t)line->data[0] << 24) |
                     ((uint32_t)line->data[1] << 16) |
                     ((uint32_t)line->data[2] << 8) |
                     (uint32_t)line->data[3],
                 (unsigned long long)machine->cpu.pc_full,
                 (unsigned long long)machine->cpu.last_pc_full,
                 machine->cpu.last_opcode, machine->cpu.attempt_outcome,
                 (unsigned long long)machine->cpu.retired_instructions,
                 machine->cpu.stopped ? 1u : 0u,
                 (unsigned long long)machine->cpu.gpr[4],
                 (unsigned long long)machine->cpu.gpr[8],
                 (uint32_t)machine->cpu.cp0[13],
                 (unsigned long long)machine->cpu.cp0[8],
                 machine->cpu.linked_valid ? 1u : 0u,
                 machine->cpu.linked_physical_address,
                 machine->cpu.linked_width);
    return false;
  }

  status = t64_cpu_step(machine, NULL);
  backing = ((uint32_t)machine->ram[data_physical] << 24) |
            ((uint32_t)machine->ram[data_physical + 1u] << 16) |
            ((uint32_t)machine->ram[data_physical + 2u] << 8) |
            (uint32_t)machine->ram[data_physical + 3u];
  if (status != T64_STATUS_OK || backing != expected || !line->valid ||
      line->dirty || !machine->cpu.linked_valid ||
      machine->cpu.linked_physical_address != UINT32_C(0x00004000) ||
      machine->cpu.linked_width != UINT32_C(4)) {
    (void)printf("# hit-writeback expected backing=%08x valid=1 dirty=0; "
                 "observed status=%u backing=%08x valid=%u dirty=%u "
                 "link=%u/%08x/%u\n",
                 expected, (unsigned)status, backing,
                 line->valid ? 1u : 0u, line->dirty ? 1u : 0u,
                 machine->cpu.linked_valid ? 1u : 0u,
                 machine->cpu.linked_physical_address,
                 machine->cpu.linked_width);
    return false;
  }
  return true;
}

static bool test_vr4300_instruction_cache_failed_fill_preserves_line(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  const uint32_t first_pc = UINT32_C(0x80001000);
  const uint32_t refused_pc = UINT32_C(0x80401000);
  const uint32_t line_index = (first_pc >> 5) &
                              (T64_VR4300_ICACHE_LINE_COUNT - 1u);
  const uint32_t initial_instruction = UINT32_C(0x24080007);
  t64_vr4300_icache_line line_before;
  t64_status status;

  if (machine == NULL || asset == NULL ||
      t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK) {
    (void)puts("# failed-fill I-cache fixture setup failed");
    return false;
  }
  write_rdram_instruction(machine, UINT32_C(0x1000), initial_instruction);
  machine->cpu.pc = first_pc;
  machine->cpu.pc_full = t64_cpu_canonical_address(first_pc);
  status = t64_cpu_step(machine, NULL);
  if (status != T64_STATUS_OK || machine->cpu.gpr[8] != UINT64_C(7) ||
      !machine->cpu.icache[line_index].valid ||
      machine->cpu.icache[line_index].physical_tag != UINT32_C(0x1000)) {
    (void)printf("# failed-fill cache seed status=%u gpr8=%016llx "
                 "valid=%u tag=%08x\n",
                 (unsigned)status, (unsigned long long)machine->cpu.gpr[8],
                 machine->cpu.icache[line_index].valid ? 1u : 0u,
                 machine->cpu.icache[line_index].physical_tag);
    return false;
  }
  line_before = machine->cpu.icache[line_index];
  machine->cpu.pc = refused_pc;
  machine->cpu.pc_full = t64_cpu_canonical_address(refused_pc);
  status = t64_cpu_step(machine, NULL);
  if (status != T64_STATUS_RESOURCE_EXHAUSTED ||
      !machine->cpu.stopped ||
      machine->cpu.stop_reason != T64_STOP_REASON_CPU_UNSUPPORTED ||
      machine->cpu.stop_pc != refused_pc ||
      machine->cpu.stop_opcode != 0u ||
      machine->cpu.pc_full != t64_cpu_canonical_address(refused_pc) ||
      machine->cpu.stop_pc_full != t64_cpu_canonical_address(refused_pc) ||
      machine->cpu.last_opcode != 0u ||
      machine->cpu.retired_instructions != UINT64_C(1) ||
      memcmp(&line_before, &machine->cpu.icache[line_index],
             sizeof line_before) != 0) {
    (void)printf("# failed-fill result status=%u stopped=%u reason=%u "
                 "stop_pc=%08x stop_opcode=%08x pc_full=%016llx "
                 "last_opcode=%08x retired=%llu line_same=%u\n",
                 (unsigned)status, machine->cpu.stopped ? 1u : 0u,
                 machine->cpu.stop_reason, machine->cpu.stop_pc,
                 machine->cpu.stop_opcode,
                 (unsigned long long)machine->cpu.pc_full,
                 machine->cpu.last_opcode,
                 (unsigned long long)machine->cpu.retired_instructions,
                 memcmp(&line_before, &machine->cpu.icache[line_index],
                        sizeof line_before) == 0
                     ? 1u
                     : 0u);
    return false;
  }
  return true;
}

static bool test_vr4300_tlb_refill_and_retry(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  const uint32_t load_pc = UINT32_C(0x80001000);
  const uint32_t refill_pc = UINT32_C(0x80000000);
  const uint32_t virtual_data = UINT32_C(0x00004000);
  const uint32_t physical_data = UINT32_C(0x00003000);
  const uint64_t expected_value = UINT64_C(0x12345678);
  const uint32_t entry_lo = UINT32_C(0x000000df);
  static const uint32_t refill_handler[] = {
      UINT32_C(0x341a00df), /* ori k0,zero,EntryLo */
      UINT32_C(0x409a1000), /* mtc0 k0,EntryLo0 */
      UINT32_C(0x409a1800), /* mtc0 k0,EntryLo1 */
      UINT32_C(0x42000002), /* tlbwi */
      UINT32_C(0x42000018), /* eret */
  };
  t64_vr4300_dcache_line data_line_before;
  uint8_t bytes_before[4];
  uint32_t linked_address_before;
  uint32_t linked_width_before;
  bool linked_valid_before;
  bool refill_state_ok;
  bool failed_access_state_ok;
  t64_status status;
  size_t index;

  if (machine == NULL || asset == NULL ||
      t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK) {
    (void)puts("# TLB refill fixture setup failed");
    return false;
  }
  for (index = 0u; index < sizeof refill_handler / sizeof refill_handler[0];
       index++) {
    write_rdram_instruction(machine, (uint32_t)(index * 4u),
                            refill_handler[index]);
  }
  write_rdram_instruction(machine, UINT32_C(0x1000), UINT32_C(0x8c880000));
  write_rdram_instruction(machine, UINT32_C(0x1004), UINT32_C(0x24090007));
  machine->ram[physical_data] = UINT8_C(0x12);
  machine->ram[physical_data + 1u] = UINT8_C(0x34);
  machine->ram[physical_data + 2u] = UINT8_C(0x56);
  machine->ram[physical_data + 3u] = UINT8_C(0x78);
  machine->cpu.pc = load_pc;
  machine->cpu.pc_full = t64_cpu_canonical_address(load_pc);
  machine->cpu.cp0[0] = UINT64_C(0);  /* Index selects entry zero. */
  machine->cpu.cp0[12] = UINT64_C(0); /* Kernel, BEV=0, 32-bit mode. */
  machine->cpu.gpr[4] = (uint64_t)virtual_data;
  machine->cpu.gpr[8] = UINT64_C(0xfeedface);
  machine->cpu.linked_valid = true;
  machine->cpu.linked_width = UINT32_C(4);
  machine->cpu.linked_physical_address = UINT32_C(0x80);
  data_line_before = machine->cpu.dcache[0];
  (void)memcpy(bytes_before, machine->ram + physical_data,
               sizeof bytes_before);
  linked_address_before = machine->cpu.linked_physical_address;
  linked_width_before = machine->cpu.linked_width;
  linked_valid_before = machine->cpu.linked_valid;

  status = t64_cpu_step(machine, NULL);
  refill_state_ok =
      status == T64_STATUS_OK && machine->cpu.pending_exception_valid &&
      machine->cpu.pc_full == t64_cpu_canonical_address(refill_pc) &&
      (uint32_t)machine->cpu.cp0[13] == (UINT32_C(2) << 2) &&
      machine->cpu.cp0[8] == (uint64_t)virtual_data &&
      machine->cpu.cp0[10] == (uint64_t)virtual_data;
  failed_access_state_ok =
      machine->cpu.gpr[8] == UINT64_C(0xfeedface) &&
      memcmp(bytes_before, machine->ram + physical_data,
             sizeof bytes_before) == 0 &&
      memcmp(&data_line_before, &machine->cpu.dcache[0],
             sizeof data_line_before) == 0 &&
      machine->cpu.linked_valid == linked_valid_before &&
      machine->cpu.linked_width == linked_width_before &&
      machine->cpu.linked_physical_address == linked_address_before;
  if (!refill_state_ok || !failed_access_state_ok) {
    (void)printf("# TLB miss observed status=%u pc=%016llx code=%u "
                 "badvaddr=%016llx entryhi=%016llx gpr8=%016llx "
                 "retired=%llu refill_ok=%u no_partial_effects=%u\n",
                 (unsigned)status,
                 (unsigned long long)machine->cpu.pc_full,
                 ((uint32_t)machine->cpu.cp0[13] >> 2) & UINT32_C(31),
                 (unsigned long long)machine->cpu.cp0[8],
                 (unsigned long long)machine->cpu.cp0[10],
                 (unsigned long long)machine->cpu.gpr[8],
                 (unsigned long long)machine->cpu.retired_instructions,
                 refill_state_ok ? 1u : 0u,
                 failed_access_state_ok ? 1u : 0u);
    return false;
  }

  for (index = 0u; index < 8u && machine->cpu.gpr[8] != expected_value;
       index++) {
    status = t64_cpu_step(machine, NULL);
    if (status != T64_STATUS_OK) {
      break;
    }
  }
  if (status == T64_STATUS_OK && machine->cpu.gpr[8] == expected_value) {
    status = t64_cpu_step(machine, NULL);
  }
  if (status != T64_STATUS_OK || machine->cpu.gpr[8] != expected_value ||
      machine->cpu.gpr[9] != UINT64_C(7) ||
      machine->cpu.tlb[0].entry_lo0 != (uint64_t)entry_lo ||
      machine->cpu.tlb[0].entry_lo1 != (uint64_t)entry_lo ||
      machine->cpu.pending_exception_valid) {
    (void)printf("# TLB retry observed status=%u pc=%016llx gpr8=%016llx "
                 "gpr9=%016llx tlblo0=%016llx tlblo1=%016llx "
                 "pending=%u\n",
                 (unsigned)status,
                 (unsigned long long)machine->cpu.pc_full,
                 (unsigned long long)machine->cpu.gpr[8],
                 (unsigned long long)machine->cpu.gpr[9],
                 (unsigned long long)machine->cpu.tlb[0].entry_lo0,
                 (unsigned long long)machine->cpu.tlb[0].entry_lo1,
                 machine->cpu.pending_exception_valid ? 1u : 0u);
    return false;
  }
  return true;
}

static bool test_vr4300_protected_cp0_guest_handler(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {
      UINT32_C(0x40886000), /* mtc0 t0,c0_status */
      UINT32_C(0x400a6000), /* mfc0 t2,c0_status */
      UINT32_C(0x402b7000), /* dmfc0 t3,c0_epc */
      UINT32_C(0x40896000), /* mtc0 t1,c0_status: enter user mode */
      UINT32_C(0x400d6000), /* mfc0 t5,c0_status: CU0 fault */
  };
  const uint32_t handler_pc = UINT32_C(0x80000180);
  const uint32_t expected_status = UINT32_C(0x90200001);
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(100)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};
  t64_status status;
  uint32_t observed_status;
  uint32_t cause;
  size_t index;

  if (machine == NULL || asset == NULL ||
      t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK) {
    (void)puts("# protected CP0 fixture setup failed");
    return false;
  }
  (void)memcpy(machine->cpu.gpr, (uint64_t[32]){0}, sizeof machine->cpu.gpr);
  machine->cpu.cp0[12] = UINT32_C(0x34200000); /* Keep hardware TS set. */
  machine->cpu.cp0[14] = UINT64_C(0x0123456789abcdef);
  machine->cpu.gpr[8] = UINT32_C(0x98000001);
  machine->cpu.gpr[9] = UINT32_C(0x00000010);
  machine->cpu.gpr[13] = UINT64_C(0x1122334455667788);
  for (index = 1u; index < T64_VR4300_TLB_ENTRY_COUNT; index++) {
    machine->cpu.tlb[index].asid = UINT32_C(1);
  }
  machine->cpu.tlb[0].vpn2 = UINT64_C(0x1000);
  machine->cpu.tlb[0].asid = UINT32_C(0);
  machine->cpu.tlb[0].entry_lo0 = UINT64_C(0x5e);
  machine->cpu.tlb[0].entry_lo1 = UINT64_C(0x5e);
  write_rdram_instruction(machine, UINT32_C(0x0180),
                          UINT32_C(0x240e005a)); /* addiu t6,zero,0x5a */
  write_rdram_instruction(machine, UINT32_C(0x0184),
                          UINT32_C(0xbc1c0000)); /* undefined CACHE stop */
  for (index = 0u; index < sizeof program / sizeof program[0]; index++) {
    write_rdram_instruction(machine, UINT32_C(0x1000) +
                                        (uint32_t)(index * 4u),
                            program[index]);
  }
  /* Earlier guest cases ran through the same virtual indices. This case
   * replaces their synthetic code and must begin from its own fetched image. */
  clear_instruction_index(machine, UINT64_C(0x1000));
  clear_instruction_index(machine,
                          t64_cpu_canonical_address(handler_pc));
  machine->cpu.pc = UINT32_C(0x1000);
  machine->cpu.pc_full = UINT64_C(0x1000);

  status = t64_machine_advance(machine, &request, &result);
  (void)t64_machine_cpu_stop_detail(machine, &detail);
  observed_status = (uint32_t)machine->cpu.cp0[12];
  cause = (uint32_t)machine->cpu.cp0[13];
  if (status != T64_STATUS_OK ||
      result.reason != T64_STOP_REASON_CPU_UNSUPPORTED ||
      detail.pc != handler_pc + UINT32_C(4) ||
      machine->cpu.cp0[14] != UINT64_C(0x1010) ||
      ((cause >> 2) & UINT32_C(0x1f)) != UINT32_C(11) ||
      machine->cpu.gpr[10] != UINT64_C(0xffffffff90200001) ||
      machine->cpu.gpr[11] != UINT64_C(0x0123456789abcdef) ||
      machine->cpu.gpr[13] != UINT64_C(0x1122334455667788) ||
      machine->cpu.gpr[14] != UINT64_C(0x5a) ||
      observed_status != UINT32_C(0x00200012) ||
      detail.retired_instructions != UINT64_C(5)) {
    (void)printf("# expected status=0x%08x mfc0=0xffffffff%08x "
                 "dmfc0_epc=0x0123456789abcdef "
                 "unchanged_t5=0x1122334455667788 "
                 "actual status=%u reason=%u stop_pc=0x%08x opcode=0x%08x "
                 "cp0_status=0x%08x cause=0x%08x epc=0x%016llx "
                 "mfc0_t2=0x%016llx dmfc0_t3=0x%016llx t5=0x%016llx "
                 "t6=0x%016llx retired=%llu\n",
                 expected_status, expected_status,
                 (unsigned)status, result.reason, detail.pc, detail.opcode,
                 observed_status, cause,
                 (unsigned long long)machine->cpu.cp0[14],
                 (unsigned long long)machine->cpu.gpr[10],
                 (unsigned long long)machine->cpu.gpr[11],
                 (unsigned long long)machine->cpu.gpr[13],
                 (unsigned long long)machine->cpu.gpr[14],
                 (unsigned long long)detail.retired_instructions);
    return false;
  }
  return true;
}

static bool test_vr4300_timer_interrupt_handler(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t main_program[] = {
      UINT32_C(0x40885800), /* mtc0 t0,c0_compare */
      UINT32_C(0x240a0066), /* addiu t2,zero,0x66 */
      UINT32_C(0x240b0077), /* addiu t3,zero,0x77 */
      UINT32_C(0x240d0055), /* interrupted then resumed */
  };
  static const uint32_t handler_program[] = {
      UINT32_C(0x40895800), /* mtc0 t1,c0_compare: clear IP7 */
      UINT32_C(0x240c004a), /* addiu t4,zero,0x4a */
      UINT32_C(0x42000018), /* eret */
  };
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(8)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_status status;
  uint32_t cause;
  uint32_t status_register;
  size_t i;

  if (machine == NULL || asset == NULL ||
      t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK) {
    (void)puts("# timer interrupt fixture setup failed");
    return false;
  }
  for (i = 0u; i < sizeof main_program / sizeof main_program[0]; i++) {
    write_rdram_instruction(machine, UINT32_C(0x1000) + (uint32_t)(i * 4u),
                            main_program[i]);
  }
  for (i = 0u; i < sizeof handler_program / sizeof handler_program[0]; i++) {
    write_rdram_instruction(machine, UINT32_C(0x0180) + (uint32_t)(i * 4u),
                            handler_program[i]);
  }
  machine->cpu.pc = UINT32_C(0x80001000);
  machine->cpu.pc_full = t64_cpu_canonical_address(machine->cpu.pc);
  machine->cpu.gpr[8] = UINT64_C(2);
  machine->cpu.gpr[9] = UINT64_C(100);
  machine->cpu.cp0[12] = UINT32_C(0x00008001); /* IE and timer IM7. */

  status = t64_machine_advance(machine, &request, &result);
  cause = (uint32_t)machine->cpu.cp0[13];
  status_register = (uint32_t)machine->cpu.cp0[12];
  if (status != T64_STATUS_OK ||
      result.reason != T64_STOP_REASON_BOUND ||
      result.tick != UINT64_C(8) || machine->cpu.gpr[10] != UINT64_C(0x66) ||
      machine->cpu.gpr[11] != UINT64_C(0x77) ||
      machine->cpu.gpr[12] != UINT64_C(0x4a) ||
      machine->cpu.gpr[13] != UINT64_C(0x55) ||
      machine->cpu.cp0[14] != t64_cpu_canonical_address(
                                  UINT32_C(0x8000100c)) ||
      ((cause >> 2) & UINT32_C(0x1f)) != 0u ||
      (cause & T64_CP0_CAUSE_TIMER_MASK) != 0u ||
      (status_register & UINT32_C(2)) != 0u ||
      (uint32_t)machine->cpu.cp0[9] != UINT32_C(4) ||
      (uint32_t)machine->cpu.cp0[11] != UINT32_C(100)) {
    (void)printf("# timer handler status=%u reason=%u tick=%llu "
                 "retired=%llu pc=%016llx cause=%08x epc=%016llx "
                 "status=%08x count=%08x compare=%08x t2=%016llx "
                 "t3=%016llx t4=%016llx t5=%016llx\n",
                 (unsigned)status, result.reason,
                 (unsigned long long)result.tick,
                 (unsigned long long)machine->cpu.retired_instructions,
                 (unsigned long long)machine->cpu.pc_full, cause,
                 (unsigned long long)machine->cpu.cp0[14], status_register,
                 (uint32_t)machine->cpu.cp0[9],
                 (uint32_t)machine->cpu.cp0[11],
                 (unsigned long long)machine->cpu.gpr[10],
                 (unsigned long long)machine->cpu.gpr[11],
                 (unsigned long long)machine->cpu.gpr[12],
                 (unsigned long long)machine->cpu.gpr[13]);
    return false;
  }
  return true;
}

static bool test_vr4300_integer_delay_fault_and_full_width_jalr(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {
      UINT32_C(0x012a402d), /* daddu t0,t1,t2 -> 0x00000001fffffffe */
      UINT32_C(0x010c5821), /* addu t3,t0,t4 -> sign-extended 0xffffffff */
      UINT32_C(0x10000002), /* beq zero,zero,target */
      UINT32_C(0x01ae7820), /* delay slot: add t7,t5,t6 overflows */
      UINT32_C(0x00000000), /* skipped */
      UINT32_C(0x7000003f), /* branch target */
  };
  const uint64_t branch_pc = UINT64_C(0xffffffff80001008);
  const uint64_t target = UINT64_C(0x00000001fffffffe);
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(6)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_status status;
  uint32_t cause;
  uint32_t stored;
  const t64_vr4300_dcache_line *stored_line;
  size_t i;

  if (machine == NULL || asset == NULL ||
      t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK) {
    (void)puts("# integer-delay fixture setup failed");
    return false;
  }
  for (i = 0u; i < sizeof program / sizeof program[0]; i++) {
    write_rdram_instruction(machine, UINT32_C(0x1000) + (uint32_t)(i * 4u),
                            program[i]);
  }
  write_rdram_instruction(machine, UINT32_C(0x0180), UINT32_C(0xae0b0000));
  write_rdram_instruction(machine, UINT32_C(0x0184), UINT32_C(0x01004009));
  write_rdram_instruction(machine, UINT32_C(0x0188), UINT32_C(0x00000000));
  machine->cpu.pc = UINT32_C(0x80001000);
  machine->cpu.pc_full = UINT64_C(0xffffffff80001000);
  machine->cpu.gpr[9] = UINT64_C(0x00000001fffffff0);
  machine->cpu.gpr[10] = UINT64_C(0x000000000000000e);
  machine->cpu.gpr[12] = UINT64_C(1);
  machine->cpu.gpr[13] = UINT64_C(0x7fffffff);
  machine->cpu.gpr[14] = UINT64_C(1);
  machine->cpu.gpr[16] = UINT64_C(0xffffffff80002000);

  status = t64_machine_advance(machine, &request, &result);
  cause = (uint32_t)machine->cpu.cp0[13];
  stored_line = &machine->cpu.dcache[(UINT32_C(0x2000) >> 4) &
                                     (T64_VR4300_DCACHE_LINE_COUNT - 1u)];
  stored = read_be32_bytes(stored_line->data);
  if (status != T64_STATUS_OK || result.reason != T64_STOP_REASON_BOUND ||
      result.tick != UINT64_C(6) ||
      machine->cpu.retired_instructions != UINT64_C(5) ||
      machine->cpu.gpr[8] != UINT64_C(0xffffffff8000018c) ||
      machine->cpu.gpr[11] != UINT64_C(0xffffffffffffffff) ||
      machine->cpu.cp0[14] != branch_pc ||
      (cause & UINT32_C(0x8000007c)) != UINT32_C(0x80000030) ||
      !stored_line->valid || !stored_line->dirty ||
      stored_line->physical_tag != UINT32_C(0x00002000) ||
      read_be32_bytes(machine->ram + UINT32_C(0x2000)) != 0u ||
      stored != UINT32_MAX || machine->cpu.pc_full !=
                                  UINT64_C(0xffffffff80000188)) {
    (void)printf("# integer-delay status=%u reason=%u tick=%llu retired=%llu "
                 "t0=%016llx t3=%016llx pc=%016llx cause=%08x "
                 "epc=%016llx store=%08x line=%u/%u/%08x/%08x "
                 "base=%016llx\n",
                 (unsigned)status, result.reason,
                 (unsigned long long)result.tick,
                 (unsigned long long)machine->cpu.retired_instructions,
                 (unsigned long long)machine->cpu.gpr[8],
                 (unsigned long long)machine->cpu.gpr[11],
                 (unsigned long long)machine->cpu.pc_full, cause,
                 (unsigned long long)machine->cpu.cp0[14], stored,
                 stored_line->valid ? 1u : 0u,
                 stored_line->dirty ? 1u : 0u, stored_line->physical_tag,
                 read_be32_bytes(stored_line->data),
                 (unsigned long long)machine->cpu.gpr[16]);
    return false;
  }
  if (machine->cpu.delay_target_full != target) {
    (void)printf("# jalr source==destination target expected=%016llx "
                 "observed=%016llx link=%016llx slot=%016llx\n",
                 (unsigned long long)target,
                 (unsigned long long)machine->cpu.delay_target_full,
                 (unsigned long long)machine->cpu.gpr[8],
                 (unsigned long long)machine->cpu.pc_full);
    return false;
  }
  return true;
}

static bool test_vr4300_not_taken_likely_annuls_faulting_slot(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {
      UINT32_C(0x50080002), /* beql zero,t0,target (not taken) */
      UINT32_C(0x8d4b0000), /* annulled lw t3,0(t2) from unmapped KUSEG */
      UINT32_C(0x240c0007), /* addiu t4,zero,7 */
      UINT32_C(0x7000003f), /* explicit unsupported stop */
  };
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(2)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  size_t i;

  if (machine == NULL || asset == NULL ||
      t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK) {
    (void)puts("# branch-likely fixture setup failed");
    return false;
  }
  for (i = 0u; i < sizeof program / sizeof program[0]; i++) {
    write_rdram_instruction(machine, UINT32_C(0x3000) + (uint32_t)(i * 4u),
                            program[i]);
  }
  machine->cpu.pc = UINT32_C(0x80003000);
  machine->cpu.pc_full = UINT64_C(0xffffffff80003000);
  machine->cpu.gpr[8] = UINT64_C(1);
  machine->cpu.gpr[10] = UINT64_C(0x100);
  machine->cpu.gpr[11] = UINT64_C(0x1234);

  if (t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
      result.reason != T64_STOP_REASON_BOUND || result.tick != UINT64_C(2) ||
      machine->cpu.retired_instructions != UINT64_C(2) ||
      machine->cpu.gpr[11] != UINT64_C(0x1234) ||
      machine->cpu.gpr[12] != UINT64_C(7) ||
      machine->cpu.pc_full != UINT64_C(0xffffffff8000300c) ||
      machine->cpu.attempt_outcome != T64_CPU_ATTEMPT_RETIRED ||
      ((uint32_t)machine->cpu.cp0[13] & UINT32_C(0x8000007c)) != 0u) {
    (void)printf("# likely-annul reason=%u tick=%llu retired=%llu "
                 "pc=%016llx t3=%016llx t4=%016llx cause=%08x outcome=%u\n",
                 result.reason, (unsigned long long)result.tick,
                 (unsigned long long)machine->cpu.retired_instructions,
                 (unsigned long long)machine->cpu.pc_full,
                 (unsigned long long)machine->cpu.gpr[11],
                 (unsigned long long)machine->cpu.gpr[12],
                 (uint32_t)machine->cpu.cp0[13], machine->cpu.attempt_outcome);
    return false;
  }
  return true;
}

static bool stop_at_unsupported(t64_machine *machine,
                                t64_stop_result *result_out,
                                t64_cpu_stop_detail *detail_out) {
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(100)};

  return t64_machine_advance(machine, &request, result_out) ==
             T64_STATUS_OK &&
         result_out->reason == T64_STOP_REASON_CPU_UNSUPPORTED &&
         t64_machine_cpu_stop_detail(machine, detail_out) == T64_STATUS_OK &&
         result_out->tick == detail_out->retired_instructions + UINT64_C(1);
}

static bool test_real_entry_and_next_boundary(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(2)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};
  t64_status status;

  if (t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK ||
      t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
      result.reason != T64_STOP_REASON_BOUND || result.tick != UINT64_C(2) ||
      machine->cpu.pc != IPL3_ENTRY_PC + 8u ||
      machine->cpu.retired_instructions != UINT64_C(2) ||
      machine->instruction_count != UINT64_C(2) ||
      machine->cpu.gpr[29] != UINT64_C(0xffffffffa4000000)) {
    (void)printf("# initial entry state pc=0x%08x retired=%llu gpr29=0x%016llx\n",
                 machine->cpu.pc,
                 (unsigned long long)machine->cpu.retired_instructions,
                 (unsigned long long)machine->cpu.gpr[29]);
    return false;
  }
  request.value = UINT64_C(100);
  status = t64_machine_advance(machine, &request, &result);
  if (status != T64_STATUS_OK ||
      result.reason != T64_STOP_REASON_BOUND ||
      result.tick != UINT64_C(100) ||
      machine->cpu.retired_instructions != UINT64_C(100) ||
      machine->cpu.pc != UINT32_C(0xa4000354) ||
      machine->cpu.last_pc != UINT32_C(0xa4000350) ||
      machine->cpu.last_opcode != UINT32_C(0x240af000) ||
      t64_machine_cpu_stop_detail(machine, &detail) != T64_STATUS_NOT_READY) {
    (void)printf("# actual boundary status=%u reason=%u tick=%llu retired=%llu "
                 "pc=0x%08x last_pc=0x%08x last_opcode=0x%08x\n",
                 (unsigned)status, result.reason,
                 (unsigned long long)result.tick,
                 (unsigned long long)machine->cpu.retired_instructions,
                 machine->cpu.pc, machine->cpu.last_pc,
                 machine->cpu.last_opcode);
    return false;
  }
  return true;
}

static bool test_integer_sign_extension_and_bal_delay(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {
      UINT32_C(0x3c088000), /* lui t0,0x8000 */
      UINT32_C(0x2508ffff), /* addiu t0,t0,-1 */
      UINT32_C(0x01004821), /* addu t1,t0,zero */
      UINT32_C(0x00095040), /* sll t2,t1,1 */
      UINT32_C(0x000a5843), /* sra t3,t2,1 */
      UINT32_C(0x00086078), /* dsll t4,t0,1 */
      UINT32_C(0x000c687b), /* dsra t5,t4,1 */
      UINT32_C(0x04110002), /* bal target */
      UINT32_C(0x240e1234), /* delay slot */
      UINT32_C(0x240e0001), /* skipped by the taken BAL */
      UINT32_C(0x7000003f), /* explicit unsupported stop */
  };
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};

  if (!install_program(machine, asset, program,
                       sizeof program / sizeof program[0])) {
    return false;
  }
  return stop_at_unsupported(machine, &result, &detail) &&
         detail.retired_instructions == UINT64_C(9) &&
         detail.pc == IPL3_ENTRY_PC + UINT32_C(40) &&
         detail.opcode == UINT32_C(0x7000003f) &&
         machine->cpu.gpr[8] == UINT64_C(0x000000007fffffff) &&
         machine->cpu.gpr[9] == UINT64_C(0x000000007fffffff) &&
         machine->cpu.gpr[10] == UINT64_C(0xfffffffffffffffe) &&
         machine->cpu.gpr[11] == UINT64_C(0xffffffffffffffff) &&
         machine->cpu.gpr[12] == UINT64_C(0x00000000fffffffe) &&
         machine->cpu.gpr[13] == UINT64_C(0x000000007fffffff) &&
         machine->cpu.gpr[14] == UINT64_C(0x1234) &&
         machine->cpu.gpr[31] == UINT64_C(0xffffffffa4000064);
}

static bool test_signed_multiply_and_hilo(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {
      UINT32_C(0x01090018), /* mult t0,t1 */
      UINT32_C(0x00005012), /* mflo t2 */
      UINT32_C(0x00005810), /* mfhi t3 */
      UINT32_C(0x7000003f), /* explicit unsupported stop */
  };
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};

  if (!install_program(machine, asset, program,
                       sizeof program / sizeof program[0])) {
    return false;
  }
  machine->cpu.gpr[8] = UINT64_C(0xffffffff80000002);
  machine->cpu.gpr[9] = UINT64_C(3);
  if (!stop_at_unsupported(machine, &result, &detail)) {
    return false;
  }
  if (detail.retired_instructions != UINT64_C(3) ||
      detail.pc != IPL3_ENTRY_PC + UINT32_C(12) ||
      machine->cpu.hi != UINT64_C(0xfffffffffffffffe) ||
      machine->cpu.lo != UINT64_C(0xffffffff80000006) ||
      machine->cpu.gpr[10] != machine->cpu.lo ||
      machine->cpu.gpr[11] != machine->cpu.hi) {
    (void)printf("# actual retired=%llu pc=0x%08x opcode=0x%08x "
                 "hi=0x%016llx lo=0x%016llx\n",
                 (unsigned long long)detail.retired_instructions,
                 detail.pc, detail.opcode,
                 (unsigned long long)machine->cpu.hi,
                 (unsigned long long)machine->cpu.lo);
    return false;
  }
  return true;
}

static bool test_beq_delay_slot(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {
      UINT32_C(0x10000002), /* beq zero,zero,target */
      UINT32_C(0x24080007), /* delay slot executes */
      UINT32_C(0x24090001), /* skipped by the taken branch */
      UINT32_C(0x7000003f), /* target */
  };
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};

  if (!install_program(machine, asset, program,
                       sizeof program / sizeof program[0])) {
    return false;
  }
  return stop_at_unsupported(machine, &result, &detail) &&
         detail.retired_instructions == UINT64_C(2) &&
         detail.pc == IPL3_ENTRY_PC + UINT32_C(12) &&
         machine->cpu.gpr[8] == UINT64_C(7) &&
         machine->cpu.gpr[9] == UINT64_C(0);
}

static bool test_indirect_jump_and_link(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t jr_program[] = {
      UINT32_C(0x01000008), /* jr t0 */
      UINT32_C(0x240b0007), /* delay slot */
      UINT32_C(0x240c0001), /* skipped */
      UINT32_C(0x7000003f), /* indirect target */
  };
  static const uint32_t jalr_program[] = {
      UINT32_C(0x01004809), /* jalr t1,t0 */
      UINT32_C(0x240b0007), /* delay slot */
      UINT32_C(0x240c0001), /* skipped */
      UINT32_C(0x7000003f), /* indirect target */
  };
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};

  if (!install_program(machine, asset, jr_program,
                       sizeof jr_program / sizeof jr_program[0])) {
    return false;
  }
  machine->cpu.gpr[8] = UINT64_C(0xffffffffa400004c);
  if (!stop_at_unsupported(machine, &result, &detail) ||
      detail.retired_instructions != UINT64_C(2) ||
      detail.pc != IPL3_ENTRY_PC + UINT32_C(12) ||
      machine->cpu.gpr[11] != UINT64_C(7) ||
      machine->cpu.gpr[12] != UINT64_C(0)) {
    return false;
  }

  if (!install_program(machine, asset, jalr_program,
                       sizeof jalr_program / sizeof jalr_program[0])) {
    return false;
  }
  machine->cpu.gpr[8] = UINT64_C(0xffffffffa400004c);
  return stop_at_unsupported(machine, &result, &detail) &&
         detail.retired_instructions == UINT64_C(2) &&
         detail.pc == IPL3_ENTRY_PC + UINT32_C(12) &&
         machine->cpu.gpr[9] == UINT64_C(0xffffffffa4000048) &&
         machine->cpu.gpr[11] == UINT64_C(7) &&
         machine->cpu.gpr[12] == UINT64_C(0);
}

static bool test_kseg_endian_and_partial_memory(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {
      UINT32_C(0x3c048000), /* lui a0,0x8000 */
      UINT32_C(0x34840100), /* ori a0,a0,0x100 */
      UINT32_C(0x8c8f0000), /* lw t7,0(a0) */
      UINT32_C(0xac8f0004), /* sw t7,4(a0) */
      UINT32_C(0x3c05a000), /* lui a1,0xa000 */
      UINT32_C(0x34a50100), /* ori a1,a1,0x100 */
      UINT32_C(0x8cb80000), /* lw t8,0(a1), KSEG1 alias */
      UINT32_C(0x50180002), /* beql zero,t8,target (not taken) */
      UINT32_C(0x24190001), /* annulled delay slot */
      UINT32_C(0x24190002), /* executes after not-taken likely branch */
      UINT32_C(0x3c0faabb), /* lui t7,0xaabb */
      UINT32_C(0x35efccdd), /* ori t7,t7,0xccdd */
      UINT32_C(0xa88f0001), /* swl t7,1(a0) */
      UINT32_C(0xb88f0005), /* swr t7,5(a0) */
      UINT32_C(0xb08d0009), /* sdl t5,9(a0) */
      UINT32_C(0xb48d0011), /* sdr t5,17(a0) */
      UINT32_C(0x88880001), /* lwl t0,1(a0) */
      UINT32_C(0x98890002), /* lwr t1,2(a0) */
      UINT32_C(0x7000003f), /* explicit unsupported stop */
  };
  uint32_t kseg0_physical = UINT32_MAX;
  uint32_t kseg1_physical = UINT32_MAX;
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};
  size_t i;

  if (!install_program(machine, asset, program,
                       sizeof program / sizeof program[0])) {
    return false;
  }
  for (i = 0u; i < 24u; i++) {
    machine->ram[0x100u + i] = (uint8_t)i;
  }
  machine->ram[0x100u] = UINT8_C(0x11);
  machine->ram[0x101u] = UINT8_C(0x22);
  machine->ram[0x102u] = UINT8_C(0x33);
  machine->ram[0x103u] = UINT8_C(0x44);
  machine->cpu.gpr[13] = UINT64_C(0x0123456789abcdef);

  if (!t64_cpu_translate(UINT64_C(0xffffffff80000100), &kseg0_physical) ||
      !t64_cpu_translate(UINT64_C(0xffffffffa0000100), &kseg1_physical) ||
      kseg0_physical != UINT32_C(0x100) ||
      kseg1_physical != kseg0_physical ||
      !stop_at_unsupported(machine, &result, &detail)) {
    return false;
  }
  return detail.retired_instructions == UINT64_C(17) &&
         detail.pc == IPL3_ENTRY_PC + UINT32_C(72) &&
         detail.opcode == UINT32_C(0x7000003f) &&
         machine->cpu.gpr[15] == UINT64_C(0xffffffffaabbccdd) &&
         machine->cpu.gpr[24] == UINT64_C(0x11223344) &&
         machine->cpu.gpr[25] == UINT64_C(2) &&
         machine->cpu.gpr[8] == UINT64_C(0xffffffffaabbcc00) &&
         machine->cpu.gpr[9] == UINT64_C(0x00bbcc44) &&
         machine->ram[0x100u] == UINT8_C(0xaa) &&
         machine->ram[0x101u] == UINT8_C(0xbb) &&
         machine->ram[0x102u] == UINT8_C(0xcc) &&
         machine->ram[0x103u] == UINT8_C(0x44) &&
         machine->ram[0x104u] == UINT8_C(0x11) &&
         machine->ram[0x105u] == UINT8_C(0x22) &&
         machine->ram[0x106u] == UINT8_C(0xcc) &&
         machine->ram[0x107u] == UINT8_C(0xdd) &&
         machine->ram[0x108u] == UINT8_C(0x01) &&
         machine->ram[0x109u] == UINT8_C(0x23) &&
         machine->ram[0x10au] == UINT8_C(0x45) &&
         machine->ram[0x10bu] == UINT8_C(0x67) &&
         machine->ram[0x10cu] == UINT8_C(0x89) &&
         machine->ram[0x10du] == UINT8_C(0xab) &&
         machine->ram[0x10eu] == UINT8_C(0xcd) &&
         machine->ram[0x10fu] == UINT8_C(0x0f) &&
         machine->ram[0x116u] == UINT8_C(0xcd) &&
         machine->ram[0x117u] == UINT8_C(0xef);
}

static bool test_unsupported_alignment_and_address_modes(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t misaligned_program[] = {
      UINT32_C(0x8c880001), UINT32_C(0x7000003f)};
  static const uint32_t unsupported_address_program[] = {
      UINT32_C(0x8c880000), UINT32_C(0x7000003f)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};
  uint32_t rejected_output = UINT32_C(0xdeadbeef);

  if (!t64_cpu_translate(UINT64_C(0xffffffff80000100), &rejected_output) ||
      t64_cpu_translate(UINT64_C(0x0000000080000100), &rejected_output) ||
      rejected_output != UINT32_C(0x100)) {
    return false;
  }
  if (!install_program(machine, asset, misaligned_program,
                       sizeof misaligned_program /
                           sizeof misaligned_program[0])) {
    return false;
  }
  machine->cpu.gpr[4] = UINT64_C(0xffffffff80000100);
  if (!stop_at_unsupported(machine, &result, &detail) ||
      detail.retired_instructions != UINT64_C(0) ||
      detail.pc != IPL3_ENTRY_PC ||
      detail.opcode != UINT32_C(0x8c880001)) {
    return false;
  }
  if (!install_program(machine, asset, unsupported_address_program,
                       sizeof unsupported_address_program /
                           sizeof unsupported_address_program[0])) {
    return false;
  }
  machine->cpu.gpr[4] = UINT64_C(0x100);
  return stop_at_unsupported(machine, &result, &detail) &&
         detail.retired_instructions == UINT64_C(0) &&
         detail.pc == IPL3_ENTRY_PC &&
         detail.opcode == UINT32_C(0x8c880000);
}

static bool test_overflow_and_unsupported_opcode(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t addi_program[] = {
      UINT32_C(0x21280001), UINT32_C(0x7000003f)};
  static const uint32_t daddi_program[] = {
      UINT32_C(0x616a0001), UINT32_C(0x7000003f)};
  static const uint32_t unsupported_program[] = {UINT32_C(0x7000003f)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};

  if (!install_program(machine, asset, addi_program,
                       sizeof addi_program / sizeof addi_program[0])) {
    return false;
  }
  machine->cpu.gpr[8] = UINT64_C(0x55);
  machine->cpu.gpr[9] = UINT64_C(0x7fffffff);
  if (!stop_at_unsupported(machine, &result, &detail) ||
      detail.retired_instructions != UINT64_C(0) ||
      detail.pc != IPL3_ENTRY_PC ||
      detail.opcode != UINT32_C(0x21280001) ||
      machine->cpu.gpr[8] != UINT64_C(0x55)) {
    return false;
  }
  if (!install_program(machine, asset, daddi_program,
                       sizeof daddi_program / sizeof daddi_program[0])) {
    return false;
  }
  machine->cpu.gpr[10] = UINT64_C(0x66);
  machine->cpu.gpr[11] = UINT64_C(0x7fffffffffffffff);
  if (!stop_at_unsupported(machine, &result, &detail) ||
      detail.retired_instructions != UINT64_C(0) ||
      detail.pc != IPL3_ENTRY_PC ||
      detail.opcode != UINT32_C(0x616a0001) ||
      machine->cpu.gpr[10] != UINT64_C(0x66)) {
    return false;
  }
  if (!install_program(machine, asset, unsupported_program,
                       sizeof unsupported_program /
                           sizeof unsupported_program[0])) {
    return false;
  }
  return stop_at_unsupported(machine, &result, &detail) &&
         detail.retired_instructions == UINT64_C(0) &&
         detail.pc == IPL3_ENTRY_PC &&
         detail.opcode == UINT32_C(0x7000003f);
}

static bool test_cp0_count_wrap_and_compare(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {
      UINT32_C(0x400a4800), /* mfc0 t2,c0_count */
      UINT32_C(0x00000000), /* nop */
      UINT32_C(0x00000000), /* nop */
      UINT32_C(0x7000003f), /* explicit unsupported stop */
  };
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};

  if (!install_program(machine, asset, program,
                       sizeof program / sizeof program[0])) {
    return false;
  }
  machine->cpu.cp0[9] = UINT64_C(0xfffffffe);
  machine->cpu.cp0[11] = UINT64_C(0);
  if (!stop_at_unsupported(machine, &result, &detail)) {
    return false;
  }
  if (detail.retired_instructions != UINT64_C(3) ||
      detail.pc != IPL3_ENTRY_PC + UINT32_C(12) ||
      machine->cpu.gpr[10] != UINT64_C(0xfffffffffffffffe) ||
      machine->cpu.cp0[9] != UINT64_C(1) ||
      (machine->cpu.cp0[13] & T64_CP0_CAUSE_TIMER_MASK) == 0u) {
    (void)printf("# actual retired=%llu pc=0x%08x opcode=0x%08x "
                 "mfc0_t2=0x%016llx count=0x%08llx cause=0x%08llx\n",
                 (unsigned long long)detail.retired_instructions,
                 detail.pc, detail.opcode,
                 (unsigned long long)machine->cpu.gpr[10],
                 (unsigned long long)machine->cpu.cp0[9],
                 (unsigned long long)machine->cpu.cp0[13]);
    return false;
  }
  return true;
}

static bool test_cp0_register_masks(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {
      UINT32_C(0x40886800), /* mtc0 t0,c0_cause */
      UINT32_C(0x40899000), /* mtc0 t1,c0_watchlo */
      UINT32_C(0x408ae000), /* mtc0 t2,c0_taglo */
      UINT32_C(0x408be800), /* mtc0 t3,c0_taghi */
      UINT32_C(0x7000003f), /* explicit unsupported stop */
  };
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};

  if (!install_program(machine, asset, program,
                       sizeof program / sizeof program[0])) {
    return false;
  }
  machine->cpu.gpr[8] = UINT64_C(0xffffffff);
  machine->cpu.gpr[9] = UINT64_C(0xffffffff);
  machine->cpu.gpr[10] = UINT64_C(0xffffffff);
  machine->cpu.gpr[11] = UINT64_C(0xffffffff);
  machine->cpu.cp0[13] = T64_CP0_CAUSE_TIMER_MASK;
  return stop_at_unsupported(machine, &result, &detail) &&
         detail.retired_instructions == UINT64_C(4) &&
         machine->cpu.cp0[13] ==
             (T64_CP0_CAUSE_TIMER_MASK | T64_CP0_CAUSE_SOFTWARE_MASK) &&
         machine->cpu.cp0[18] == UINT64_C(0xfffffffb) &&
         machine->cpu.cp0[28] == UINT64_C(0x0fffffc0) &&
         machine->cpu.cp0[29] == UINT64_C(0);
}

static bool test_cp0_compare_write_clears_timer(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {
      UINT32_C(0x40885800), /* mtc0 t0,c0_compare */
      UINT32_C(0x7000003f), /* explicit unsupported stop */
  };
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};

  if (!install_program(machine, asset, program,
                       sizeof program / sizeof program[0])) {
    return false;
  }
  machine->cpu.gpr[8] = UINT64_C(7);
  machine->cpu.cp0[13] =
      T64_CP0_CAUSE_TIMER_MASK | T64_CP0_CAUSE_SOFTWARE_MASK;
  return stop_at_unsupported(machine, &result, &detail) &&
         detail.retired_instructions == UINT64_C(1) &&
         machine->cpu.cp0[11] == UINT64_C(7) &&
         machine->cpu.cp0[13] == T64_CP0_CAUSE_SOFTWARE_MASK;
}

static bool test_cache_operations_are_coherent(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const struct {
    uint32_t opcode;
    uint32_t operation;
    uint64_t virtual_base;
    uint32_t physical_address;
  } cache_cases[] = {
      {UINT32_C(0xbd080000), UINT32_C(0x08),
       UINT64_C(0xffffffff80000100), UINT32_C(0x100)}, /* index store tag I */
      {UINT32_C(0xbd090000), UINT32_C(0x09),
       UINT64_C(0xffffffff80000100), UINT32_C(0x100)}, /* index store tag D */
      {UINT32_C(0xbd110000), UINT32_C(0x11),
       UINT64_C(0xffffffff807f8000), UINT32_C(0x007f8000)}, /* hit invalidate D */
      {UINT32_C(0xbd150000), UINT32_C(0x15),
       UINT64_C(0xffffffff80000100), UINT32_C(0x100)}, /* hit WB/invalidate D */
  };
  size_t i;

  for (i = 0u; i < sizeof cache_cases / sizeof cache_cases[0]; i++) {
    uint32_t program[] = {UINT32_C(0xad490000), cache_cases[i].opcode,
                          UINT32_C(0x7000003f)};
    t64_stop_result result = {UINT32_MAX, UINT64_MAX};
    t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};

    if (!install_program(machine, asset, program,
                         sizeof program / sizeof program[0])) {
      return false;
    }
    machine->cpu.gpr[8] = cache_cases[i].virtual_base;
    machine->cpu.gpr[9] = UINT64_C(0x12345678);
    machine->cpu.gpr[10] = UINT64_C(0xffffffff80000100);
    if (!stop_at_unsupported(machine, &result, &detail) ||
        detail.retired_instructions != UINT64_C(2) ||
        detail.pc != IPL3_ENTRY_PC + UINT32_C(8) ||
        machine->cache.policy_version != UINT32_C(1) ||
        machine->cache.last_operation != cache_cases[i].operation ||
        machine->cache.last_pc != IPL3_ENTRY_PC + UINT32_C(4) ||
        machine->cache.last_address != cache_cases[i].physical_address ||
        !machine->cache.operation_seen || machine->ram[0x100u] != 0x12u ||
        machine->ram[0x101u] != 0x34u || machine->ram[0x102u] != 0x56u ||
        machine->ram[0x103u] != 0x78u) {
      (void)printf("# cache case=%zu stop=%llu pc=0x%08x opcode=0x%08x "
                   "op=0x%x cache_pc=0x%08x cache_addr=0x%08x "
                   "seen=%d ram=%02x%02x%02x%02x\n",
                   i, (unsigned long long)detail.retired_instructions,
                   detail.pc, detail.opcode, machine->cache.last_operation,
                   machine->cache.last_pc, machine->cache.last_address,
                   machine->cache.operation_seen ? 1 : 0,
                   machine->ram[0x100u], machine->ram[0x101u],
                   machine->ram[0x102u], machine->ram[0x103u]);
      (void)printf("# gpr8=0x%016llx gpr9=0x%016llx gpr10=0x%016llx "
                   "rdram=%llu tick=%llu bus_read=%d\n",
                   (unsigned long long)machine->cpu.gpr[8],
                   (unsigned long long)machine->cpu.gpr[9],
                   (unsigned long long)machine->cpu.gpr[10],
                   (unsigned long long)machine->profile.rdram_bytes,
                   (unsigned long long)machine->tick,
                   (int)t64_bus_read(machine, UINT32_C(0x100), 4u,
                                     &(uint32_t){0}));
      return false;
    }
  }
  return true;
}

static bool create_profile_machine(
    const t64_host_allocator *allocator, const char *profile_id,
    uint32_t expected_model_version, uint32_t expected_state_version,
    t64_machine **machine_out) {
  const t64_profile_descriptor *profile = NULL;

  return allocator != NULL && profile_id != NULL && machine_out != NULL &&
         t64_profile_lookup(profile_id, &profile) == T64_STATUS_OK &&
         profile != NULL &&
         profile->machine_model_version == expected_model_version &&
         profile->state_serialization_version == expected_state_version &&
         t64_machine_create(allocator, profile, machine_out) ==
             T64_STATUS_OK &&
         *machine_out != NULL;
}

static bool run_uncached_cache_index_writeback_case(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES],
    bool should_retire) {
  const uint32_t cache_opcode =
      encode_cache(T64_CACHE_INDEX_WRITEBACK_INVALIDATE_D, 8u, 12u);
  const uint32_t program[] = {cache_opcode};
  const uint64_t uncached_base =
      t64_cpu_canonical_address(UINT32_C(0xa0001000));
  const uint64_t expected_pc_full =
      t64_cpu_canonical_address(IPL3_ENTRY_PC);
  const uint32_t index =
      (uint32_t)((uncached_base + UINT64_C(12)) >> 4) &
      (T64_VR4300_DCACHE_LINE_COUNT - UINT32_C(1));
  t64_vr4300_dcache_line *line;
  t64_vr4300_dcache_line line_before;
  t64_cache_state cache_before;
  uint8_t backing_before[T64_VR4300_DCACHE_LINE_BYTES];
  uint64_t taglo_before;
  uint64_t retired_before;
  uint64_t cause_before;
  uint64_t status_before;
  t64_status status;
  size_t offset;

  if (!install_program(machine, asset, program,
                       sizeof program / sizeof program[0])) {
    return false;
  }
  machine->cpu.pc_full = expected_pc_full;
  machine->cpu.gpr[8] = uncached_base;
  line = &machine->cpu.dcache[index];
  memset(machine->ram + UINT32_C(0x1000), UINT8_C(0x11),
         sizeof backing_before);
  memcpy(backing_before, machine->ram + UINT32_C(0x1000),
         sizeof backing_before);
  line->physical_tag = UINT32_C(0x1000);
  line->valid = true;
  line->dirty = true;
  for (offset = 0u; offset < sizeof line->data; offset++) {
    line->data[offset] = (uint8_t)(UINT8_C(0xa0) + (uint8_t)offset);
  }
  machine->cpu.cp0[T64_CP0_TAGLO] = UINT64_C(0x12345678);
  machine->cache.last_operation = UINT32_C(0x1111);
  machine->cache.last_pc = UINT32_C(0x2222);
  machine->cache.last_address = UINT32_C(0x3333);
  machine->cache.operation_seen = false;
  line_before = *line;
  cache_before = machine->cache;
  taglo_before = machine->cpu.cp0[T64_CP0_TAGLO];
  retired_before = machine->cpu.retired_instructions;
  cause_before = machine->cpu.cp0[13];
  status_before = machine->cpu.cp0[12];

  status = t64_cpu_step(machine, NULL);
  if (should_retire) {
    if (status != T64_STATUS_OK ||
        machine->cpu.attempt_outcome != T64_CPU_ATTEMPT_RETIRED ||
        machine->cpu.last_opcode != cache_opcode ||
        machine->cpu.last_pc_full != expected_pc_full ||
        machine->cpu.cp0[13] != cause_before ||
        machine->cpu.cp0[12] != status_before ||
        machine->cpu.retired_instructions != retired_before + UINT64_C(1) ||
        line->valid || line->dirty ||
        memcmp(machine->ram + UINT32_C(0x1000), line_before.data,
               sizeof line_before.data) != 0 ||
        machine->cache.last_operation !=
            T64_CACHE_INDEX_WRITEBACK_INVALIDATE_D ||
        machine->cache.last_pc != IPL3_ENTRY_PC ||
        machine->cache.last_address != UINT32_C(0x100c) ||
        !machine->cache.operation_seen ||
        machine->cpu.cp0[T64_CP0_TAGLO] != taglo_before) {
      (void)printf("# compatibility-v2 uncached CACHE expected fetched "
                   "writeback retirement; status=%u fetched=%08x/%016llx "
                   "expected=%08x/%016llx cause=%016llx/%016llx "
                   "status-cp0=%016llx/%016llx retired=%llu "
                   "line=%u/%u/%08x op=%u address=%08x\n",
                   (unsigned)status,
                   machine->cpu.last_opcode,
                   (unsigned long long)machine->cpu.last_pc_full,
                   cache_opcode,
                   (unsigned long long)expected_pc_full,
                   (unsigned long long)machine->cpu.cp0[13],
                   (unsigned long long)cause_before,
                   (unsigned long long)machine->cpu.cp0[12],
                   (unsigned long long)status_before,
                   (unsigned long long)machine->cpu.retired_instructions,
                   line->valid ? 1u : 0u, line->dirty ? 1u : 0u,
                   line->physical_tag, machine->cache.last_operation,
                   machine->cache.last_address);
      return false;
    }
    return true;
  }

  if (status != T64_STATUS_RESOURCE_EXHAUSTED || !machine->cpu.stopped ||
      machine->cpu.stop_opcode != cache_opcode ||
      machine->cpu.stop_pc != IPL3_ENTRY_PC ||
      machine->cpu.last_opcode != cache_opcode ||
      machine->cpu.last_pc_full != expected_pc_full ||
      machine->cpu.cp0[13] != cause_before ||
      machine->cpu.cp0[12] != status_before ||
      machine->cpu.retired_instructions != retired_before ||
      memcmp(line, &line_before, sizeof line_before) != 0 ||
      memcmp(&machine->cache, &cache_before, sizeof cache_before) != 0 ||
      machine->cpu.cp0[T64_CP0_TAGLO] != taglo_before ||
      memcmp(machine->ram + UINT32_C(0x1000), backing_before,
             sizeof backing_before) != 0) {
    (void)printf("# strict/compat-v1 uncached CACHE refusal was not atomic; "
                 "fetched=%08x/%016llx expected=%08x/%016llx "
                 "cause=%016llx/%016llx status-cp0=%016llx/%016llx\n",
                 machine->cpu.last_opcode,
                 (unsigned long long)machine->cpu.last_pc_full, cache_opcode,
                 (unsigned long long)expected_pc_full,
                 (unsigned long long)machine->cpu.cp0[13],
                 (unsigned long long)cause_before,
                 (unsigned long long)machine->cpu.cp0[12],
                 (unsigned long long)status_before);
    return false;
  }
  return true;
}

static bool test_compat_v2_uncached_cache_profile_boundary(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  allocator_state nested_allocator_state = {0u};
  const t64_host_allocator nested_allocator = {
      test_allocate, test_release, &nested_allocator_state};
  t64_machine *strict_machine = NULL;
  t64_machine *compat_v1_machine = NULL;
  bool okay;

  okay = machine != NULL &&
         machine->profile.machine_model_version == 5u &&
         machine->profile.state_serialization_version == 11u &&
         run_uncached_cache_index_writeback_case(machine, asset, true);
  if (!create_profile_machine(
          &nested_allocator, "ntsc-cart-4m-openipl3-vr4300-v1", 3u, 9u,
          &strict_machine) ||
      !run_uncached_cache_index_writeback_case(strict_machine, asset, false)) {
    okay = false;
  }
  if (!create_profile_machine(
          &nested_allocator,
          "ntsc-cart-4m-openipl3-vr4300-compat-v1", 4u, 10u,
          &compat_v1_machine) ||
      !run_uncached_cache_index_writeback_case(compat_v1_machine, asset,
                                               false)) {
    okay = false;
  }
  if (strict_machine != NULL) {
    t64_machine_destroy(strict_machine);
  }
  if (compat_v1_machine != NULL) {
    t64_machine_destroy(compat_v1_machine);
  }
  if (nested_allocator_state.outstanding != 0u) {
    (void)puts("# cache profile boundary fixture leaked allocations");
    okay = false;
  }
  return okay;
}

static bool run_compat_v2_uncached_tlb_cache_case(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES],
    bool address64) {
  const uint64_t virtual_base = address64
                                    ? UINT64_C(0x0000000100400000)
                                    : UINT64_C(0x00400000);
  const uint64_t virtual_address = virtual_base + UINT64_C(12);
  const uint64_t physical_base = UINT64_C(0x2000);
  const uint32_t cache_opcode =
      encode_cache(T64_CACHE_INDEX_WRITEBACK_INVALIDATE_D, 8u, 0u);
  const uint32_t index =
      (uint32_t)(virtual_address >> 4) &
      (T64_VR4300_DCACHE_LINE_COUNT - UINT32_C(1));
  const uint32_t program[] = {cache_opcode};
  t64_vr4300_dcache_line *line;
  t64_status status;
  uint64_t retired_before;
  size_t offset;

  if (!install_program(machine, asset, program,
                       sizeof program / sizeof program[0])) {
    return false;
  }
  machine->cpu.pc_full = t64_cpu_canonical_address(IPL3_ENTRY_PC);
  if (address64) {
    machine->cpu.cp0[T64_CP0_STATUS] |= UINT64_C(1) << 7;
  } else {
    machine->cpu.cp0[T64_CP0_STATUS] &= ~(UINT64_C(1) << 7);
  }
  install_tlb_pair(machine, virtual_base, physical_base, 2u);
  machine->cpu.gpr[8] = virtual_address;
  line = &machine->cpu.dcache[index];
  memset(machine->ram + physical_base, UINT8_C(0x11), sizeof line->data);
  line->physical_tag = (uint32_t)physical_base;
  line->valid = true;
  line->dirty = true;
  for (offset = 0u; offset < sizeof line->data; offset++) {
    line->data[offset] = (uint8_t)(UINT8_C(0xc0) + (uint8_t)offset);
  }
  retired_before = machine->cpu.retired_instructions;
  status = t64_cpu_step(machine, NULL);
  if (status != T64_STATUS_OK ||
      machine->cpu.attempt_outcome != T64_CPU_ATTEMPT_RETIRED ||
      machine->cpu.retired_instructions != retired_before + UINT64_C(1) ||
      line->valid || line->dirty ||
      memcmp(machine->ram + physical_base, line->data,
             sizeof line->data) != 0 ||
      machine->cache.last_operation !=
          T64_CACHE_INDEX_WRITEBACK_INVALIDATE_D ||
      machine->cache.last_address != UINT32_C(0x200c) ||
      !machine->cache.operation_seen) {
    (void)printf("# compatibility-v2 %s TLB C=2 CACHE expected writeback; "
                 "status=%u retired=%llu line=%u/%u op=%u address=%08x\n",
                 address64 ? "64-bit" : "32-bit", (unsigned)status,
                 (unsigned long long)machine->cpu.retired_instructions,
                 line->valid ? 1u : 0u, line->dirty ? 1u : 0u,
                 machine->cache.last_operation, machine->cache.last_address);
    return false;
  }
  return true;
}

static bool test_compat_v2_uncached_tlb_cache_32bit(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  return run_compat_v2_uncached_tlb_cache_case(machine, asset, false);
}

static bool test_compat_v2_uncached_tlb_cache_64bit(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  return run_compat_v2_uncached_tlb_cache_case(machine, asset, true);
}

static bool test_compat_v2_uncached_instruction_cache_ops(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  const uint64_t virtual_address =
      t64_cpu_canonical_address(UINT32_C(0xa0002000));
  const uint32_t cache_program[] = {
      encode_cache(T64_CACHE_INDEX_LOAD_TAG_I, 8u, 0u),
      encode_cache(T64_CACHE_INDEX_INVALIDATE_I, 8u, 0u),
      encode_cache(T64_CACHE_INDEX_LOAD_TAG_I, 8u, 0u),
      encode_cache(T64_CACHE_FILL_I, 8u, 0u),
      encode_cache(T64_CACHE_HIT_INVALIDATE_I, 8u, 0u),
      encode_cache(T64_CACHE_INDEX_LOAD_TAG_I, 8u, 0u),
      encode_cache(T64_CACHE_INDEX_STORE_TAG_I, 8u, 0u),
      encode_cache(T64_CACHE_INDEX_LOAD_TAG_I, 8u, 0u),
  };
  const uint32_t index =
      (uint32_t)(virtual_address >> 5) &
      (T64_VR4300_ICACHE_LINE_COUNT - UINT32_C(1));
  t64_vr4300_icache_line *line;
  size_t offset;
  size_t instruction;

  if (!install_program(machine, asset, cache_program,
                       sizeof cache_program / sizeof cache_program[0])) {
    return false;
  }
  machine->cpu.pc_full = t64_cpu_canonical_address(IPL3_ENTRY_PC);
  machine->cpu.gpr[8] = virtual_address;
  line = &machine->cpu.icache[index];
  memset(machine->ram + UINT32_C(0x2000), UINT8_C(0x11), sizeof line->data);
  line->physical_tag = UINT32_C(0x2000);
  line->valid = true;
  for (offset = 0u; offset < sizeof line->data; offset++) {
    line->data[offset] = (uint8_t)(UINT8_C(0xa0) + (uint8_t)offset);
  }

  for (instruction = 0u; instruction < 8u; instruction++) {
    if (t64_cpu_step(machine, NULL) != T64_STATUS_OK ||
        machine->cpu.attempt_outcome != T64_CPU_ATTEMPT_RETIRED ||
        machine->cpu.last_opcode != cache_program[instruction]) {
      (void)printf("# compatibility-v2 uncached I-cache op step=%zu "
                   "expected=%08x observed=%08x outcome=%u\n",
                   instruction, cache_program[instruction],
                   machine->cpu.last_opcode,
                   (unsigned)machine->cpu.attempt_outcome);
      return false;
    }
    if (instruction == 0u &&
        (machine->cpu.cp0[T64_CP0_TAGLO] != UINT64_C(0x280) ||
         machine->cpu.cp0[T64_CP0_TAGHI] != 0u)) {
      (void)puts("# uncached Index_Load_Tag I did not expose the valid PTag");
      return false;
    }
    if (instruction == 1u && line->valid) {
      (void)puts("# uncached Index_Invalidate I left its selected line valid");
      return false;
    }
    if (instruction == 2u &&
        machine->cpu.cp0[T64_CP0_TAGLO] != UINT64_C(0x200)) {
      (void)puts("# uncached Index_Load_Tag I did not expose invalid PState");
      return false;
    }
    if (instruction == 3u &&
        (!line->valid || line->physical_tag != UINT32_C(0x2000) ||
         memcmp(line->data, machine->ram + UINT32_C(0x2000),
                sizeof line->data) != 0)) {
      (void)puts("# uncached I Fill did not populate the selected line");
      return false;
    }
    if (instruction == 4u && line->valid) {
      (void)puts("# uncached Hit_Invalidate I left its matching line valid");
      return false;
    }
    if (instruction == 5u &&
        machine->cpu.cp0[T64_CP0_TAGLO] != UINT64_C(0x200)) {
      (void)puts("# uncached Index_Load_Tag I after hit invalidate mismatched");
      return false;
    }
    if (instruction == 5u) {
      machine->cpu.cp0[T64_CP0_TAGLO] = UINT64_C(0x340);
    }
    if (instruction == 6u &&
        (line->valid || line->physical_tag != UINT32_C(0x3000))) {
      (void)puts("# uncached Index_Store_Tag I PState=01 did not store invalid tag");
      return false;
    }
    if (instruction == 7u &&
        machine->cpu.cp0[T64_CP0_TAGLO] != UINT64_C(0x300)) {
      (void)puts("# uncached Index_Load_Tag I did not normalize PState=01");
      return false;
    }
  }
  return true;
}

static bool test_compat_v2_zero_data_read_and_store_guard(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  const uint32_t program[] = {
      (UINT32_C(0x23) << 26) | (UINT32_C(4) << 21) | (UINT32_C(8) << 16),
      (UINT32_C(0x2b) << 26) | (UINT32_C(4) << 21) | (UINT32_C(9) << 16),
  };
  const uint64_t virtual_base = UINT64_C(0x00400000);
  uint32_t open_bus_line;
  uint64_t virtual_address;
  uint32_t index;
  t64_vr4300_dcache_line *line;
  t64_vr4300_dcache_line line_before_store;
  uint64_t gpr8_before_store;
  uint64_t gpr9_before_store;
  uint64_t retired_before;
  uint64_t cause_before_uncached_load;
  uint64_t status_before_uncached_load;
  uint64_t cause_before_store;
  uint64_t epc_before_store;
  uint64_t badvaddr_before_store;
  bool linked_before_store;
  uint32_t linked_address_before_store;
  uint32_t linked_width_before_store;
  uint32_t cause;
  t64_status uncached_load_status;
  t64_status cached_load_status;
  t64_mmu_request cached_load_request = {
      UINT64_C(0), UINT32_C(4), T64_MMU_ACCESS_READ};
  t64_mmu_result cached_load_translation = {0};
  bool cached_load_translated;
  const uint64_t expected_load_pc_full =
      t64_cpu_canonical_address(IPL3_ENTRY_PC);

  if (!install_program(machine, asset, program,
                       sizeof program / sizeof program[0]) ||
      !find_compat_zero_data_line(machine, T64_VR4300_ICACHE_LINE_BYTES,
                                  &open_bus_line)) {
    (void)puts("# compatibility-v2 open-bus read fixture setup failed");
    return false;
  }
  virtual_address = virtual_base + (uint64_t)(open_bus_line & UINT32_C(0xfff));
  index = (uint32_t)(virtual_address >> 4) &
          (T64_VR4300_DCACHE_LINE_COUNT - UINT32_C(1));
  machine->cpu.pc_full = t64_cpu_canonical_address(IPL3_ENTRY_PC);
  machine->cpu.gpr[4] = virtual_address;
  machine->cpu.gpr[9] = UINT64_C(0xdeadbeef);
  install_tlb_pair(machine, virtual_base,
                   (uint64_t)(open_bus_line & UINT32_C(0xfffff000)), 2u);
  cause_before_uncached_load = machine->cpu.cp0[13];
  status_before_uncached_load = machine->cpu.cp0[12];
  uncached_load_status = t64_cpu_step(machine, NULL);
  if (uncached_load_status != T64_STATUS_OK ||
      machine->cpu.last_opcode != program[0] ||
      machine->cpu.last_pc_full != expected_load_pc_full ||
      machine->cpu.cp0[13] != cause_before_uncached_load ||
      machine->cpu.cp0[12] != status_before_uncached_load ||
      machine->cpu.gpr[8] != 0u ||
      machine->cpu.attempt_outcome != T64_CPU_ATTEMPT_RETIRED ||
      machine->cpu.dcache[index].valid) {
    (void)printf("# uncached open-bus LW expected fetched zero read without D "
                 "fill; result=%u fetched=%08x/%016llx "
                 "expected=%08x/%016llx cause=%016llx/%016llx "
                 "status-cp0=%016llx/%016llx gpr=%016llx line=%u "
                 "outcome=%u\n",
                 (unsigned)uncached_load_status,
                 machine->cpu.last_opcode,
                 (unsigned long long)machine->cpu.last_pc_full, program[0],
                 (unsigned long long)expected_load_pc_full,
                 (unsigned long long)machine->cpu.cp0[13],
                 (unsigned long long)cause_before_uncached_load,
                 (unsigned long long)machine->cpu.cp0[12],
                 (unsigned long long)status_before_uncached_load,
                 (unsigned long long)machine->cpu.gpr[8],
                 machine->cpu.dcache[index].valid ? 1u : 0u,
                 (unsigned)machine->cpu.attempt_outcome);
    return false;
  }

  machine->cpu.tlb[0].entry_lo0 = cacheable_tlb_entry_lo(
      (uint64_t)(open_bus_line & UINT32_C(0xfffff000)), 3u);
  machine->cpu.tlb[0].entry_lo1 = machine->cpu.tlb[0].entry_lo0;
  machine->cpu.pc = IPL3_ENTRY_PC;
  machine->cpu.pc_full = t64_cpu_canonical_address(IPL3_ENTRY_PC);
  machine->cpu.gpr[8] = UINT64_C(0x1122334455667788);
  cached_load_request.virtual_address = virtual_address;
  cached_load_translated = t64_mmu_translate(
      machine, &cached_load_request, &cached_load_translation);
  cached_load_status = t64_cpu_step(machine, NULL);
  if (cached_load_status != T64_STATUS_OK ||
      machine->cpu.gpr[8] != 0u ||
      machine->cpu.attempt_outcome != T64_CPU_ATTEMPT_RETIRED) {
    (void)printf("# cached open-bus LW expected zero; va=%016llx "
                 "translated=%u pa=%016llx cca=%u cacheable=%u "
                 "tlb-entry=%u entrylo=%08x fetched=%08x/%016llx "
                 "expected-fetch=%08x/%016llx result=%u gpr=%016llx "
                 "outcome=%u cause=%016llx status-cp0=%016llx dline="
                 "%u/%u/%08x\n",
                 (unsigned long long)virtual_address,
                 cached_load_translated ? 1u : 0u,
                 (unsigned long long)cached_load_translation.physical_address,
                 (unsigned)cached_load_translation.cache_algorithm,
                 cached_load_translation.cacheable ? 1u : 0u,
                 cached_load_translation.matched_entry,
                 cached_load_translation.entry_lo,
                 machine->cpu.last_opcode,
                 (unsigned long long)machine->cpu.last_pc_full,
                 program[0], (unsigned long long)expected_load_pc_full,
                 (unsigned)cached_load_status,
                 (unsigned long long)machine->cpu.gpr[8],
                 (unsigned)machine->cpu.attempt_outcome,
                 (unsigned long long)machine->cpu.cp0[13],
                 (unsigned long long)machine->cpu.cp0[12],
                 machine->cpu.dcache[index].valid ? 1u : 0u,
                 machine->cpu.dcache[index].dirty ? 1u : 0u,
                 machine->cpu.dcache[index].physical_tag);
    return false;
  }
  line = &machine->cpu.dcache[index];
  if (!line->valid || line->dirty ||
      line->physical_tag != (open_bus_line & ~UINT32_C(0xf)) ||
      memcmp(line->data,
             (const uint8_t[T64_VR4300_DCACHE_LINE_BYTES]){0},
             sizeof line->data) != 0) {
    (void)puts("# cached open-bus LW did not retain a clean zero line");
    return false;
  }
  line_before_store = *line;
  machine->cpu.linked_valid = true;
  machine->cpu.linked_physical_address = UINT32_C(0x00003004);
  machine->cpu.linked_width = 4u;
  line_before_store = *line;
  gpr8_before_store = machine->cpu.gpr[8];
  gpr9_before_store = machine->cpu.gpr[9];
  cause_before_store = machine->cpu.cp0[13];
  epc_before_store = machine->cpu.cp0[14];
  badvaddr_before_store = machine->cpu.cp0[8];
  linked_before_store = machine->cpu.linked_valid;
  linked_address_before_store = machine->cpu.linked_physical_address;
  linked_width_before_store = machine->cpu.linked_width;
  retired_before = machine->cpu.retired_instructions;
  if (t64_cpu_step(machine, NULL) != T64_STATUS_RESOURCE_EXHAUSTED ||
      !machine->cpu.stopped ||
      machine->cpu.stop_opcode != program[1] ||
      machine->cpu.stop_pc != IPL3_ENTRY_PC + UINT32_C(4) ||
      machine->cpu.attempt_outcome != T64_CPU_ATTEMPT_MODEL_UNSUPPORTED ||
      machine->cpu.retired_instructions != retired_before ||
      memcmp(line, &line_before_store, sizeof line_before_store) != 0 ||
      machine->cpu.gpr[8] != gpr8_before_store ||
      machine->cpu.gpr[9] != gpr9_before_store ||
      machine->cpu.cp0[13] != cause_before_store ||
      machine->cpu.cp0[14] != epc_before_store ||
      machine->cpu.cp0[8] != badvaddr_before_store ||
      machine->cpu.linked_valid != linked_before_store ||
      machine->cpu.linked_physical_address != linked_address_before_store ||
      machine->cpu.linked_width != linked_width_before_store) {
    cause = (uint32_t)machine->cpu.cp0[13];
    (void)printf("# resident zero-line SW expected atomic model stop; status, "
                 "retirement, GPR/CP0/link or cache state changed; cause=%08x "
                 "retired=%llu line=%u/%u/%08x\n",
                 cause,
                 (unsigned long long)machine->cpu.retired_instructions,
                 line->valid ? 1u : 0u, line->dirty ? 1u : 0u,
                 line->physical_tag);
    return false;
  }
  return true;
}

static bool test_cp0_cache_unsupported_encodings(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t unsupported_cp0[] = {
      UINT32_C(0x40004801), /* mfc0 t0,count,sel=1 */
      UINT32_C(0x40880000), /* mtc0 t0,c0_index */
      UINT32_C(0x45010002), /* warm-v1 keeps COP1 branches refused */
  };
  static const uint32_t unsupported_cache[] = {
      UINT32_C(0xbd100000), /* undefined/unqualified CACHE operation */
      UINT32_C(0xbd150000), /* warm-v1 direct-only CACHE subset */
  };
  size_t i;

  for (i = 0u; i < sizeof unsupported_cp0 / sizeof unsupported_cp0[0];
       i++) {
    t64_stop_result result = {UINT32_MAX, UINT64_MAX};
    t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};

    if (!install_program(machine, asset, &unsupported_cp0[i], 1u) ||
        !stop_at_unsupported(machine, &result, &detail) ||
        detail.retired_instructions != UINT64_C(0) ||
        detail.pc != IPL3_ENTRY_PC ||
        detail.opcode != unsupported_cp0[i]) {
      return false;
    }
  }
  for (i = 0u; i < sizeof unsupported_cache / sizeof unsupported_cache[0];
       i++) {
    t64_stop_result result = {UINT32_MAX, UINT64_MAX};
    t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};

    if (!install_program(machine, asset, &unsupported_cache[i], 1u)) {
      return false;
    }
    machine->cpu.gpr[8] = i == 0u ? UINT64_C(0xffffffff80000100)
                                  : UINT64_C(0x100);
    if (!stop_at_unsupported(machine, &result, &detail) ||
        detail.retired_instructions != UINT64_C(0) ||
        detail.pc != IPL3_ENTRY_PC ||
        detail.opcode != unsupported_cache[i] ||
        machine->cache.operation_seen) {
      return false;
    }
  }
  return true;
}

static bool test_software_debug_stop_break(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t completion_program[] = {
      T64_CPU_SOFTWARE_DEBUG_STOP_BREAK,
  };
  static const uint32_t unrecognized_break_program[] = {
      UINT32_C(0x0000004d), /* BREAK with a nonzero code field */
  };
  static const uint32_t delay_slot_program[] = {
      UINT32_C(0x10000001), /* beq zero,zero,+1 */
      T64_CPU_SOFTWARE_DEBUG_STOP_BREAK,
  };
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(100)};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};
  t64_trace_record records[16];
  size_t count = 0u;
  uint64_t dropped = UINT64_MAX;
  bool saturated = true;
  bool saw_completion = false;
  bool retired_break = false;
  size_t i;

  if (!install_program(machine, asset, completion_program,
                       sizeof completion_program / sizeof completion_program[0]) ||
      t64_machine_trace_set_enabled(machine, true) != T64_STATUS_OK ||
      t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
      result.reason != T64_STOP_REASON_GUEST_COMPLETION ||
      result.tick != UINT64_C(1) || machine->cpu.retired_instructions != 0u ||
      machine->cpu.pc != IPL3_ENTRY_PC ||
      t64_machine_cpu_stop_detail(machine, &detail) != T64_STATUS_OK ||
      detail.retired_instructions != 0u || detail.pc != IPL3_ENTRY_PC ||
      detail.opcode != T64_CPU_SOFTWARE_DEBUG_STOP_BREAK ||
      t64_machine_trace_drain(machine, records,
                              sizeof records / sizeof records[0], &count,
                              &dropped, &saturated) != T64_STATUS_OK ||
      dropped != 0u || saturated) {
    return false;
  }
  for (i = 0u; i < count; i++) {
    if (records[i].device == T64_TRACE_DEVICE_MACHINE &&
        records[i].kind == T64_TRACE_KIND_CPU_COMPLETION &&
        records[i].value ==
            (((uint64_t)IPL3_ENTRY_PC << 32) |
             T64_CPU_SOFTWARE_DEBUG_STOP_BREAK)) {
      saw_completion = true;
    }
    if (records[i].device == T64_TRACE_DEVICE_MACHINE &&
        records[i].kind == T64_TRACE_KIND_CPU_RETIREMENT &&
        records[i].value ==
            (((uint64_t)IPL3_ENTRY_PC << 32) |
             T64_CPU_SOFTWARE_DEBUG_STOP_BREAK)) {
      retired_break = true;
    }
  }
  if (!saw_completion || retired_break ||
      !install_program(machine, asset, unrecognized_break_program,
                       sizeof unrecognized_break_program /
                           sizeof unrecognized_break_program[0]) ||
      t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
      result.reason != T64_STOP_REASON_CPU_UNSUPPORTED ||
      result.tick != UINT64_C(1) ||
      t64_machine_cpu_stop_detail(machine, &detail) != T64_STATUS_OK ||
      detail.retired_instructions != 0u || detail.pc != IPL3_ENTRY_PC ||
      detail.opcode != unrecognized_break_program[0] ||
      !install_program(machine, asset, delay_slot_program,
                       sizeof delay_slot_program / sizeof delay_slot_program[0]) ||
      t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
      result.reason != T64_STOP_REASON_CPU_UNSUPPORTED ||
      result.tick != UINT64_C(2) ||
      t64_machine_cpu_stop_detail(machine, &detail) != T64_STATUS_OK ||
      detail.retired_instructions != UINT64_C(1) ||
      detail.pc != IPL3_ENTRY_PC + UINT32_C(4) ||
      detail.opcode != T64_CPU_SOFTWARE_DEBUG_STOP_BREAK) {
    return false;
  }
  return true;
}

static bool test_cp0_cache_state_is_in_digest(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  uint8_t baseline[T64_DIGEST_SIZE];
  uint8_t changed[T64_DIGEST_SIZE];
  uint64_t cp0_value;
  t64_cache_state cache_value;
  bool cp0_hashed;
  bool cache_hashed;

  if (t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK ||
      t64_machine_digest(machine, baseline) != T64_STATUS_OK) {
    return false;
  }
  cp0_value = machine->cpu.cp0[9];
  machine->cpu.cp0[9] = cp0_value ^ UINT64_C(1);
  cp0_hashed = t64_machine_digest(machine, changed) == T64_STATUS_OK &&
               memcmp(baseline, changed, sizeof baseline) != 0;
  machine->cpu.cp0[9] = cp0_value;
  cache_value = machine->cache;
  machine->cache.last_operation ^= UINT32_C(1);
  cache_hashed = t64_machine_digest(machine, changed) == T64_STATUS_OK &&
                 memcmp(baseline, changed, sizeof baseline) != 0;
  machine->cache = cache_value;
  return cp0_hashed && cache_hashed;
}

/* NEC VR43XX Users Manual U10504EJ7V0UM00, §§16 (pp. 463, 519–522),
 * https://hack64.net/docs/VR43XX.pdf, SHA-256
 * b7c1900f1707a4b238ebd82af1669e9628e4df4875ceabfa012a9d95b4716b75.
 * Expected lane bytes are read from the big-endian instruction tables. */
static bool test_vr4300_page_edge_unaligned_merges(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {
      UINT32_C(0x88880000), /* lwl t0,0(a0) */
      UINT32_C(0x98880003), /* lwr t0,3(a0) */
      UINT32_C(0xa8890000), /* swl t1,0(a0) */
      UINT32_C(0xb8890003), /* swr t1,3(a0) */
      UINT32_C(0x888a0000), /* lwl t2,0(a0) */
      UINT32_C(0x988a0003), /* lwr t2,3(a0) */
  };
  const uint64_t address = UINT64_C(0x00400ffe);
  const uint64_t second_address = UINT64_C(0x00401001);
  t64_vr4300_tlb_pair *mapping;
  t64_status status;
  size_t index;
  uint64_t first_merge;
  uint64_t faulting_merge;

  if (machine == NULL || asset == NULL ||
      !install_program(machine, asset, program,
                       sizeof program / sizeof program[0])) {
    (void)puts("# page-edge merge fixture setup failed");
    return false;
  }
  machine->cpu.pc_full = t64_cpu_canonical_address(IPL3_ENTRY_PC);
  machine->cpu.cp0[10] = 0u;
  machine->cpu.cp0[12] = 0u;
  for (index = 0u; index < T64_VR4300_TLB_ENTRY_COUNT; index++) {
    machine->cpu.tlb[index] = (t64_vr4300_tlb_pair){0};
    machine->cpu.tlb[index].asid = UINT32_C(0xff);
  }
  mapping = &machine->cpu.tlb[7];
  mapping->vpn2 = UINT64_C(0x00400000);
  mapping->asid = 0u;
  mapping->entry_lo0 = UINT32_C(0x00000096); /* PA 0x2000, V/D, C=2. */
  mapping->entry_lo1 = UINT32_C(0x000000d6); /* PA 0x3000, V/D, C=2. */
  machine->cpu.gpr[4] = address;
  machine->cpu.gpr[8] = UINT64_C(0x1122334455667788);
  machine->cpu.gpr[9] = UINT64_C(0x00000000a1b2c3d4);
  machine->ram[UINT32_C(0x2ffc)] = UINT8_C(0x11);
  machine->ram[UINT32_C(0x2ffd)] = UINT8_C(0x22);
  machine->ram[UINT32_C(0x2ffe)] = UINT8_C(0xde);
  machine->ram[UINT32_C(0x2fff)] = UINT8_C(0xad);
  machine->ram[UINT32_C(0x3000)] = UINT8_C(0xbe);
  machine->ram[UINT32_C(0x3001)] = UINT8_C(0xef);
  machine->ram[UINT32_C(0x3002)] = UINT8_C(0x33);
  machine->ram[UINT32_C(0x3003)] = UINT8_C(0x44);

  status = t64_cpu_step(machine, NULL);
  first_merge = machine->cpu.gpr[8];
  if (status != T64_STATUS_OK ||
      first_merge != UINT64_C(0xffffffffdead7788)) {
    (void)printf("# page-edge LWL status=%u expected=ffffffffdead7788 "
                 "observed=%016llx\n",
                 (unsigned)status, (unsigned long long)first_merge);
    return false;
  }
  if (t64_cpu_step(machine, NULL) != T64_STATUS_OK ||
      machine->cpu.gpr[8] != UINT64_C(0xffffffffdeadbeef)) {
    (void)printf("# page-edge LWR expected=ffffffffdeadbeef "
                 "observed=%016llx\n",
                 (unsigned long long)machine->cpu.gpr[8]);
    return false;
  }
  if (t64_cpu_step(machine, NULL) != T64_STATUS_OK ||
      t64_cpu_step(machine, NULL) != T64_STATUS_OK ||
      machine->ram[UINT32_C(0x2ffc)] != UINT8_C(0x11) ||
      machine->ram[UINT32_C(0x2ffd)] != UINT8_C(0x22) ||
      machine->ram[UINT32_C(0x2ffe)] != UINT8_C(0xa1) ||
      machine->ram[UINT32_C(0x2fff)] != UINT8_C(0xb2) ||
      machine->ram[UINT32_C(0x3000)] != UINT8_C(0xc3) ||
      machine->ram[UINT32_C(0x3001)] != UINT8_C(0xd4) ||
      machine->ram[UINT32_C(0x3002)] != UINT8_C(0x33) ||
      machine->ram[UINT32_C(0x3003)] != UINT8_C(0x44)) {
    (void)printf("# page-edge stores bytes=%02x%02x%02x%02x%02x%02x%02x%02x\n",
                 machine->ram[UINT32_C(0x2ffc)],
                 machine->ram[UINT32_C(0x2ffd)],
                 machine->ram[UINT32_C(0x2ffe)],
                 machine->ram[UINT32_C(0x2fff)],
                 machine->ram[UINT32_C(0x3000)],
                 machine->ram[UINT32_C(0x3001)],
                 machine->ram[UINT32_C(0x3002)],
                 machine->ram[UINT32_C(0x3003)]);
    return false;
  }

  machine->cpu.gpr[10] = UINT64_C(0x0000000055667788);
  if (t64_cpu_step(machine, NULL) != T64_STATUS_OK) {
    return false;
  }
  faulting_merge = machine->cpu.gpr[10];
  mapping->entry_lo1 &= ~UINT32_C(0x00000002);
  status = t64_cpu_step(machine, NULL);
  if (status != T64_STATUS_OK || machine->cpu.gpr[10] != faulting_merge ||
      machine->cpu.retired_instructions != UINT64_C(5) ||
      (uint32_t)machine->cpu.cp0[13] !=
          (UINT32_C(2) << 2) ||
      machine->cpu.cp0[8] != second_address ||
      machine->cpu.pc_full !=
          t64_cpu_canonical_address(UINT32_C(0x80000180))) {
    (void)printf("# protected second LWR status=%u retired=%llu "
                 "gpr10=%016llx expected=%016llx cause=%08x "
                 "badvaddr=%016llx pc=%016llx\n",
                 (unsigned)status,
                 (unsigned long long)machine->cpu.retired_instructions,
                 (unsigned long long)machine->cpu.gpr[10],
                 (unsigned long long)faulting_merge,
                 (uint32_t)machine->cpu.cp0[13],
                 (unsigned long long)machine->cpu.cp0[8],
                 (unsigned long long)machine->cpu.pc_full);
    return false;
  }
  return true;
}

static bool test_vr4300_ll_sc_one_shot(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  static const uint32_t program[] = {
      UINT32_C(0xc0880000), /* ll t0,0(a0) */
      UINT32_C(0xe0890000), /* sc t1,0(a0) */
      UINT32_C(0xe08a0000), /* sc t2,0(a0) */
  };
  const uint32_t virtual_code = UINT32_C(0x80001000);
  const uint64_t virtual_data = UINT64_C(0xffffffff80002000);
  const uint32_t physical_data = UINT32_C(0x2000);
  t64_status status;
  uint64_t observed;
  const t64_vr4300_dcache_line *data_line;

  if (machine == NULL || asset == NULL ||
      t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK) {
    (void)puts("# LL/SC fixture setup failed");
    return false;
  }
  (void)memset(machine->ram + physical_data, 0, 8u);
  machine->ram[physical_data] = UINT8_C(0x12);
  machine->ram[physical_data + 1u] = UINT8_C(0x34);
  machine->ram[physical_data + 2u] = UINT8_C(0x56);
  machine->ram[physical_data + 3u] = UINT8_C(0x78);
  write_rdram_instruction(machine, UINT32_C(0x1000), program[0]);
  write_rdram_instruction(machine, UINT32_C(0x1004), program[1]);
  write_rdram_instruction(machine, UINT32_C(0x1008), program[2]);
  machine->cpu.pc = virtual_code;
  machine->cpu.pc_full = t64_cpu_canonical_address(virtual_code);
  machine->cpu.gpr[4] = virtual_data;
  machine->cpu.gpr[9] = UINT64_C(0xaabbccdd);
  machine->cpu.gpr[10] = UINT64_C(0xcafebabe);
  data_line = &machine->cpu.dcache[(physical_data >> 4) &
                                   (T64_VR4300_DCACHE_LINE_COUNT - 1u)];

  status = t64_cpu_step(machine, NULL);
  observed = machine->cpu.gpr[8];
  if (status != T64_STATUS_OK || observed != UINT64_C(0x12345678)) {
    (void)printf("# LL status expected=%u observed=%u value expected="
                 "0000000012345678 observed=%016llx\n",
                 (unsigned)T64_STATUS_OK, (unsigned)status,
                 (unsigned long long)observed);
    return false;
  }
  status = t64_cpu_step(machine, NULL);
  if (status != T64_STATUS_OK || machine->cpu.gpr[9] != UINT64_C(1) ||
      read_be32_bytes(data_line->data) != UINT32_C(0xaabbccdd) ||
      !data_line->valid || !data_line->dirty ||
      data_line->physical_tag != physical_data ||
      machine->ram[physical_data] != UINT8_C(0x12) ||
      machine->ram[physical_data + 1u] != UINT8_C(0x34) ||
      machine->ram[physical_data + 2u] != UINT8_C(0x56) ||
      machine->ram[physical_data + 3u] != UINT8_C(0x78)) {
    (void)printf("# first SC status=%u rt=%016llx cache=%08x "
                 "backing=%02x%02x%02x%02x\n",
                 (unsigned)status, (unsigned long long)machine->cpu.gpr[9],
                 read_be32_bytes(data_line->data),
                 machine->ram[physical_data],
                 machine->ram[physical_data + 1u],
                 machine->ram[physical_data + 2u],
                 machine->ram[physical_data + 3u]);
    return false;
  }
  status = t64_cpu_step(machine, NULL);
  if (status != T64_STATUS_OK || machine->cpu.gpr[10] != UINT64_C(0) ||
      read_be32_bytes(data_line->data) != UINT32_C(0xaabbccdd) ||
      !data_line->valid || !data_line->dirty ||
      data_line->physical_tag != physical_data ||
      machine->ram[physical_data] != UINT8_C(0x12) ||
      machine->ram[physical_data + 1u] != UINT8_C(0x34) ||
      machine->ram[physical_data + 2u] != UINT8_C(0x56) ||
      machine->ram[physical_data + 3u] != UINT8_C(0x78)) {
    (void)printf("# repeat SC status=%u rt=%016llx cache=%08x "
                 "backing=%02x%02x%02x%02x\n",
                 (unsigned)status, (unsigned long long)machine->cpu.gpr[10],
                 read_be32_bytes(data_line->data),
                 machine->ram[physical_data],
                 machine->ram[physical_data + 1u],
                 machine->ram[physical_data + 2u],
                 machine->ram[physical_data + 3u]);
    return false;
  }
  return true;
}

static bool test_vr4300_ll_sc_scheduled_dma(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES],
    bool sp_to_rdram) {
  const uint32_t physical_code = UINT32_C(0x1000);
  const uint32_t physical_data = UINT32_C(0x2000);
  const uint32_t virtual_code = UINT32_C(0x80001000);
  const uint64_t virtual_data = UINT64_C(0xffffffff80002000);
  const uint64_t uncached_data = UINT64_C(0xffffffffa0002000);
  const uint32_t nop_count = UINT32_C(30);
  const uint32_t program_count = nop_count + UINT32_C(4);
  uint32_t program[36] = {0};
  const uint32_t expected_data[4] = {
      sp_to_rdram ? UINT32_C(0xa0) : (uint32_t)asset[0],
      sp_to_rdram ? UINT32_C(0xa1) : (uint32_t)asset[1],
      sp_to_rdram ? UINT32_C(0xa2) : (uint32_t)asset[2],
      sp_to_rdram ? UINT32_C(0xa3) : (uint32_t)asset[3],
  };
  const uint32_t device_base =
      sp_to_rdram ? UINT32_C(0x04040000) : UINT32_C(0x04600000);
  t64_stop_request request = {T64_STOP_INSTRUCTION_COUNT, 0u};
  t64_stop_result result = {0u, 0u};
  t64_status status;
  uint32_t observed[4];
  uint32_t expected_uncached_word;
  const t64_vr4300_dcache_line *data_line;
  size_t index;

  if (machine == NULL || asset == NULL ||
      t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK) {
    (void)puts("# LL/SC DMA fixture setup failed");
    return false;
  }
  program[0] = UINT32_C(0xc0880000); /* ll t0,0(a0) */
  program[program_count - 3u] = UINT32_C(0x8c8a0000); /* lw t2,0(a0) */
  program[program_count - 2u] = UINT32_C(0x8cab0000); /* lw t3,0(a1) */
  program[program_count - 1u] = UINT32_C(0xe0890000); /* sc t1,0(a0) */
  for (index = 0u; index < program_count; index++) {
    write_rdram_instruction(machine,
                            physical_code + (uint32_t)(index * 4u),
                            program[index]);
  }
  (void)memset(machine->ram + physical_data, 0, 8u);
  machine->ram[physical_data] = UINT8_C(0x11);
  machine->ram[physical_data + 1u] = UINT8_C(0x22);
  machine->ram[physical_data + 2u] = UINT8_C(0x33);
  machine->ram[physical_data + 3u] = UINT8_C(0x44);
  if (sp_to_rdram) {
    for (index = 0u; index < 8u; index++) {
      machine->sp_dmem[index] = (uint8_t)(UINT8_C(0xa0) + (uint8_t)index);
    }
  }
  machine->cpu.pc = virtual_code;
  machine->cpu.pc_full = t64_cpu_canonical_address(virtual_code);
  machine->cpu.gpr[4] = virtual_data;
  machine->cpu.gpr[5] = uncached_data;
  machine->cpu.gpr[9] = UINT64_C(0xdeadbeef);
  data_line = &machine->cpu.dcache[(physical_data >> 4) &
                                   (T64_VR4300_DCACHE_LINE_COUNT - 1u)];

  status = t64_cpu_step(machine, NULL);
  if (status != T64_STATUS_OK) {
    (void)printf("# %s LL expected-status=%u observed-status=%u value="
                 "%016llx\n",
                 sp_to_rdram ? "SP" : "PI", (unsigned)T64_STATUS_OK,
                 (unsigned)status,
                 (unsigned long long)machine->cpu.gpr[8]);
    return false;
  }
  if (sp_to_rdram) {
    status = t64_machine_bus_write(machine, device_base, 4u, 0u);
    if (status == T64_STATUS_OK) {
      status = t64_machine_bus_write(machine, device_base + 4u, 4u,
                                     physical_data);
    }
    if (status == T64_STATUS_OK) {
      status = t64_machine_bus_write(machine, device_base + 12u, 4u,
                                     UINT32_C(7));
    }
  } else {
    status = t64_machine_bus_write(machine, device_base, 4u, physical_data);
    if (status == T64_STATUS_OK) {
      status = t64_machine_bus_write(machine, device_base + 4u, 4u,
                                     UINT32_C(0x10000000));
    }
    if (status == T64_STATUS_OK) {
      status = t64_machine_bus_write(machine, device_base + 12u, 4u,
                                     UINT32_C(7));
    }
  }
  if (status != T64_STATUS_OK) {
    (void)printf("# %s DMA request failed status=%u\n",
                 sp_to_rdram ? "SP" : "PI", (unsigned)status);
    return false;
  }
  request.value = (uint64_t)program_count;
  status = t64_machine_advance(machine, &request, &result);
  expected_uncached_word =
      (expected_data[0] << 24) | (expected_data[1] << 16) |
      (expected_data[2] << 8) | expected_data[3];
  for (index = 0u; index < 4u; index++) {
    observed[index] = machine->ram[physical_data + (uint32_t)index];
  }
  if (status != T64_STATUS_OK ||
      result.reason != T64_STOP_REASON_INSTRUCTION_COUNT ||
      result.tick < (sp_to_rdram ? UINT64_C(13) : UINT64_C(5)) ||
      machine->cpu.gpr[9] != 0u || machine->cpu.linked_valid ||
      machine->cpu.gpr[10] != UINT64_C(0x11223344) ||
      machine->cpu.gpr[11] !=
          t64_cpu_canonical_address(expected_uncached_word) ||
      !data_line->valid || data_line->dirty ||
      data_line->physical_tag != physical_data ||
      read_be32_bytes(data_line->data) != UINT32_C(0x11223344) ||
      observed[0] != expected_data[0] || observed[1] != expected_data[1] ||
      observed[2] != expected_data[2] || observed[3] != expected_data[3]) {
    (void)printf("# %s DMA/SC status=%u reason=%u instructions=%llu "
                 "tick=%llu rt=%016llx link=%u bytes=%02x%02x%02x%02x\n",
                 sp_to_rdram ? "SP" : "PI", (unsigned)status,
                 result.reason,
                 (unsigned long long)machine->instruction_count,
                 (unsigned long long)result.tick,
                 (unsigned long long)machine->cpu.gpr[9],
                 machine->cpu.linked_valid ? 1u : 0u,
                 observed[0], observed[1], observed[2], observed[3]);
    return false;
  }
  return true;
}

static bool test_vr4300_pi_dma_breaks_link(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  return test_vr4300_ll_sc_scheduled_dma(machine, asset, false);
}

static bool test_vr4300_sp_dma_breaks_link(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  return test_vr4300_ll_sc_scheduled_dma(machine, asset, true);
}

static bool test_vr4300_sp_dma_does_not_snoop_dcache(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES],
    bool dram_to_sp) {
  const uint32_t physical_code = UINT32_C(0x1000);
  const uint32_t physical_dram = UINT32_C(0x2000);
  const uint32_t virtual_code = UINT32_C(0x80001000);
  const uint32_t physical_cached_target =
      dram_to_sp ? UINT32_C(0x04000000) : physical_dram;
  const uint64_t cached_target = dram_to_sp
      ? UINT64_C(0xffffffff84000000)
      : UINT64_C(0xffffffff80002000);
  const uint64_t uncached_target = dram_to_sp
      ? UINT64_C(0xffffffffa4000000)
      : UINT64_C(0xffffffffa0002000);
  const uint32_t program_count = UINT32_C(33);
  const uint32_t cache_index =
      (physical_cached_target >> 4) &
      (T64_VR4300_DCACHE_LINE_COUNT - UINT32_C(1));
  const t64_stop_request request = {T64_STOP_INSTRUCTION_COUNT,
                                    UINT64_C(33)};
  t64_stop_result result = {0u, 0u};
  t64_vr4300_dcache_line *line;
  t64_status status;
  uint32_t index;
  bool dma_bytes_match = true;

  const uint32_t expected_cached = UINT32_C(0x11223344);

  if (machine == NULL || asset == NULL ||
      t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK) {
    (void)puts("# SP D-cache no-snoop fixture setup failed");
    return false;
  }
  for (index = 0u; index < program_count; index++) {
    uint32_t opcode = UINT32_C(0);
    if (index == 0u) {
      opcode = UINT32_C(0x8c880000); /* lw t0,0(a0) */
    } else if (index == program_count - 2u) {
      opcode = UINT32_C(0x8c890000); /* lw t1,0(a0) */
    } else if (index == program_count - 1u) {
      opcode = UINT32_C(0x8caa0000); /* lw t2,0(a1) */
    }
    write_rdram_instruction(machine,
                            physical_code + index * UINT32_C(4), opcode);
  }
  for (index = 0u; index < program_count; index++) {
    clear_instruction_index(machine,
                            t64_cpu_canonical_address(virtual_code) +
                                (uint64_t)index * UINT64_C(4));
  }
  if (dram_to_sp) {
    machine->sp_dmem[0] = UINT8_C(0x11);
    machine->sp_dmem[1] = UINT8_C(0x22);
    machine->sp_dmem[2] = UINT8_C(0x33);
    machine->sp_dmem[3] = UINT8_C(0x44);
    for (index = 0u; index < 8u; index++) {
      machine->ram[physical_dram + index] = (uint8_t)(UINT8_C(0xa0) + index);
    }
  } else {
    machine->ram[physical_dram] = UINT8_C(0x11);
    machine->ram[physical_dram + 1u] = UINT8_C(0x22);
    machine->ram[physical_dram + 2u] = UINT8_C(0x33);
    machine->ram[physical_dram + 3u] = UINT8_C(0x44);
    for (index = 0u; index < 8u; index++) {
      machine->sp_dmem[index] = (uint8_t)(UINT8_C(0xa0) + index);
    }
  }
  machine->cpu.pc = virtual_code;
  machine->cpu.pc_full = t64_cpu_canonical_address(virtual_code);
  machine->cpu.gpr[4] = cached_target;
  machine->cpu.gpr[5] = uncached_target;
  line = &machine->cpu.dcache[cache_index];

  status = t64_cpu_step(machine, NULL);
  if (status != T64_STATUS_OK ||
      machine->cpu.gpr[8] != expected_cached || !line->valid ||
      line->dirty || line->physical_tag != physical_cached_target ||
      read_be32_bytes(line->data) != expected_cached) {
    (void)printf("# SP DMEM cache fill status=%u gpr8=%016llx "
                 "valid=%u dirty=%u tag=%08x data=%08x\n",
                 (unsigned)status, (unsigned long long)machine->cpu.gpr[8],
                 line->valid ? 1u : 0u, line->dirty ? 1u : 0u,
                 line->physical_tag, read_be32_bytes(line->data));
    return false;
  }
  status = t64_machine_bus_write(machine, UINT32_C(0x04040000), 4u, 0u);
  if (status == T64_STATUS_OK) {
    status = t64_machine_bus_write(machine, UINT32_C(0x04040004), 4u,
                                   physical_dram);
  }
  if (status == T64_STATUS_OK) {
    status = t64_machine_bus_write(machine,
                                   dram_to_sp ? UINT32_C(0x04040008)
                                              : UINT32_C(0x0404000c),
                                   4u, UINT32_C(7));
  }
  if (status != T64_STATUS_OK) {
    (void)printf("# SP %s DMA request status=%u\n",
                 dram_to_sp ? "DRAM-to-DMEM" : "DMEM-to-DRAM",
                 (unsigned)status);
    return false;
  }
  status = t64_machine_advance(machine, &request, &result);
  for (index = 0u; index < 8u; index++) {
    uint8_t observed = dram_to_sp
                          ? machine->sp_dmem[index]
                          : machine->ram[physical_dram + index];
    dma_bytes_match = dma_bytes_match &&
                      observed == (uint8_t)(UINT8_C(0xa0) + index);
  }
  if (status != T64_STATUS_OK ||
      result.reason != T64_STOP_REASON_INSTRUCTION_COUNT ||
      result.tick < UINT64_C(13) ||
      machine->cpu.gpr[9] != expected_cached ||
      machine->cpu.gpr[10] != UINT64_C(0xffffffffa0a1a2a3) ||
      !dma_bytes_match ||
      !line->valid || line->dirty ||
      line->physical_tag != physical_cached_target ||
      read_be32_bytes(line->data) != expected_cached) {
    (void)printf("# SP %s no-snoop status=%u reason=%u tick=%llu "
                 "cached=%016llx uncached=%016llx dmem=%02x%02x%02x%02x "
                 "line=%08x\n",
                 dram_to_sp ? "DRAM-to-DMEM" : "DMEM-to-DRAM",
                 (unsigned)status, result.reason,
                 (unsigned long long)result.tick,
                 (unsigned long long)machine->cpu.gpr[9],
                 (unsigned long long)machine->cpu.gpr[10],
                 machine->sp_dmem[0], machine->sp_dmem[1],
                 machine->sp_dmem[2], machine->sp_dmem[3],
                 read_be32_bytes(line->data));
    return false;
  }
  return true;
}

static bool test_vr4300_sp_dram_to_dmem_does_not_snoop_dcache(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  return test_vr4300_sp_dma_does_not_snoop_dcache(machine, asset, true);
}

static bool test_vr4300_sp_dmem_to_dram_does_not_snoop_dcache(
    t64_machine *machine, const uint8_t asset[IPL3_ASSET_BYTES]) {
  return test_vr4300_sp_dma_does_not_snoop_dcache(machine, asset, false);
}

typedef bool (*cpu_test_fn)(t64_machine *machine,
                            const uint8_t asset[IPL3_ASSET_BYTES]);

typedef struct {
  const char *name;
  cpu_test_fn run;
} cpu_test_case;

int main(int argc, char **argv) {
  allocator_state allocator_state_value = {0u};
  const t64_host_allocator allocator = {
      test_allocate, test_release, &allocator_state_value};
  static const cpu_test_case cpu_path_cases[] = {
      {"qualified IPL3 integer path reaches the MI device boundary",
       test_real_entry_and_next_boundary},
      {"MIPS III sign extension, shifts and BAL delay slot",
       test_integer_sign_extension_and_bal_delay},
      {"signed 32-bit multiply writes sign-extended HI/LO words",
       test_signed_multiply_and_hilo},
      {"taken conditional branch retires its delay slot",
       test_beq_delay_slot},
      {"indirect jump and link preserve delay-slot behavior",
       test_indirect_jump_and_link},
      {"KSEG aliases, endian loads and partial load/store lanes",
       test_kseg_endian_and_partial_memory},
      {"alignment and unsupported address modes stop without retirement",
       test_unsupported_alignment_and_address_modes},
      {"guest overflow and unsupported opcodes stop without retirement",
       test_overflow_and_unsupported_opcode},
      {"software debug-stop BREAK is exact, nonretiring and excluded in delay slots",
       test_software_debug_stop_break},
  };
  static const cpu_test_case cp0_cache_cases[] = {
      {"CP0 Count reads before its retirement tick and compares after wrap",
       test_cp0_count_wrap_and_compare},
      {"CP0 Cause, WatchLo, and cache tags apply documented masks",
       test_cp0_register_masks},
      {"MTC0 Compare clears the latched timer interrupt",
       test_cp0_compare_write_clears_timer},
      {"CACHE index tags and D hit writeback preserve coherent memory",
       test_cache_operations_are_coherent},
      {"unknown CP0 selects and warm-v1 CACHE/COP1 forms stop",
       test_cp0_cache_unsupported_encodings},
      {"CP0 and cache state participate in the v6 digest",
       test_cp0_cache_state_is_in_digest},
  };
  static const cpu_test_case vr4300_cases[] = {
      {"uncached code update stays stale until targeted I-cache invalidation",
       test_vr4300_instruction_cache_stale_then_invalidated},
      {"cached data store stays private until hit writeback",
       test_vr4300_cached_store_waits_for_hit_writeback},
      {"failed I-cache fill preserves prior line and does not publish an opcode",
       test_vr4300_instruction_cache_failed_fill_preserves_line},
      {"new-profile BREAK schedules a nonretiring RDRAM handler",
       test_vr4300_break_enters_rdram_handler},
      {"CVT.D.W converts exact signed words with isolated machine FPR/FCR31 state",
       test_vr4300_cvt_d_w_is_machine_local},
      {"MTC1/CVT.D.W/SDC1/LDC1/DMFC1 follows MMU and cached memory; CU1 traps to handler",
       test_vr4300_cop1_transfer_conversion_memory_tracer},
      {"ADD.S rounding modes trap precisely, recover, and preserve E-only Cause",
       test_vr4300_cop1_rounding_trap_recovery_and_e_cause},
      {"mapped load miss is repaired by a guest TLBWI handler and retried",
       test_vr4300_tlb_refill_and_retry},
      {"guest-time Count equality interrupts then resumes after Compare commit",
       test_vr4300_timer_interrupt_handler},
      {"protected CP0 transfers resume a guest handler without destination writes",
       test_vr4300_protected_cp0_guest_handler},
      {"DADDU/ADDU branch fault resumes handler and JALR keeps full target",
       test_vr4300_integer_delay_fault_and_full_width_jalr},
      {"not-taken branch-likely annuls a faulting delay slot",
       test_vr4300_not_taken_likely_annuls_faulting_slot},
      {"page-edge LWL/LWR and SWL/SWR preserve lanes and fault precisely",
       test_vr4300_page_edge_unaligned_merges},
      {"LL/SC succeeds once for an exact physical word reservation",
       test_vr4300_ll_sc_one_shot},
      {"scheduled overlapping PI DMA breaks an LL reservation",
       test_vr4300_pi_dma_breaks_link},
      {"scheduled overlapping SP DMA breaks an LL reservation",
       test_vr4300_sp_dma_breaks_link},
      {"SP DRAM-to-DMEM DMA changes backing without snooping D-cache",
       test_vr4300_sp_dram_to_dmem_does_not_snoop_dcache},
      {"SP DMEM-to-DRAM DMA changes backing without snooping D-cache",
       test_vr4300_sp_dmem_to_dram_does_not_snoop_dcache},
  };
  static const cpu_test_case compat_v2_cache_cases[] = {
      {"uncached KSEG CACHE writeback retires only in compat-v2",
       test_compat_v2_uncached_cache_profile_boundary},
      {"uncached 32-bit TLB CACHE alias keeps index writeback semantics",
       test_compat_v2_uncached_tlb_cache_32bit},
      {"uncached 64-bit TLB CACHE alias keeps index writeback semantics",
       test_compat_v2_uncached_tlb_cache_64bit},
      {"uncached CACHE index, hit and fill operations use I-cache state",
       test_compat_v2_uncached_instruction_cache_ops},
      {"uncached and cached open-bus reads zero; resident-line store faults",
       test_compat_v2_zero_data_read_and_store_guard},
  };
  const cpu_test_case *cases;
  size_t case_count;
  size_t existing_case_count;
  bool run_vr4300_exception;
  const t64_profile_descriptor *profile = NULL;
  const t64_profile_descriptor *vr4300_profile = NULL;
  t64_machine *machine = NULL;
  uint8_t asset[IPL3_ASSET_BYTES];
  size_t i;
  size_t failure_count = 0u;

  if (argc != 2 ||
      (strcmp(argv[1], "--cpu-path") != 0 &&
       strcmp(argv[1], "--cp0-cache") != 0)) {
    (void)puts("1..1");
    (void)puts("not ok 1 - select --cpu-path or --cp0-cache");
    return 2;
  }
  if (strcmp(argv[1], "--cp0-cache") == 0) {
    cases = cp0_cache_cases;
    case_count = sizeof cp0_cache_cases / sizeof cp0_cache_cases[0];
    run_vr4300_exception = false;
  } else {
    cases = cpu_path_cases;
    case_count = sizeof cpu_path_cases / sizeof cpu_path_cases[0];
    run_vr4300_exception = true;
  }
  existing_case_count = case_count;
  if (run_vr4300_exception) {
    case_count += sizeof vr4300_cases / sizeof vr4300_cases[0];
    case_count += sizeof compat_v2_cache_cases /
                  sizeof compat_v2_cache_cases[0];
  }
  if (!read_qualified_asset(asset) ||
      t64_profile_lookup("ntsc-cart-4m-openipl3-warm-v1", &profile) !=
          T64_STATUS_OK ||
      profile == NULL ||
      t64_machine_create(&allocator, profile, &machine) != T64_STATUS_OK ||
      machine == NULL) {
    (void)puts("1..1");
    (void)puts("not ok 1 - qualified IPL3 fixture setup");
    if (machine != NULL) {
      t64_machine_destroy(machine);
    }
    return 1;
  }
  (void)printf("1..%zu\n", case_count);
  for (i = 0u; i < existing_case_count; i++) {
    bool okay = cases[i].run(machine, asset);

    (void)printf("%s %zu - %s\n", okay ? "ok" : "not ok", i + 1u,
                 cases[i].name);
    if (!okay) {
      failure_count++;
    }
  }
  t64_machine_destroy(machine);
  if (run_vr4300_exception) {
    for (i = 0u; i < sizeof vr4300_cases / sizeof vr4300_cases[0]; i++) {
      t64_machine *vr4300_machine = NULL;
      bool okay = t64_profile_lookup(
                      "ntsc-cart-4m-openipl3-vr4300-v1", &vr4300_profile) ==
                      T64_STATUS_OK &&
                  vr4300_profile != NULL &&
                  t64_machine_create(&allocator, vr4300_profile,
                                     &vr4300_machine) == T64_STATUS_OK &&
                  vr4300_machine != NULL;

      if (okay) {
        okay = vr4300_cases[i].run(vr4300_machine, asset);
      } else {
        (void)puts("# new-profile CPU machine setup failed");
      }
      (void)printf("%s %zu - %s\n", okay ? "ok" : "not ok",
                   existing_case_count + i + 1u, vr4300_cases[i].name);
      if (!okay) {
        failure_count++;
      }
      if (vr4300_machine != NULL) {
        t64_machine_destroy(vr4300_machine);
      }
    }
    for (i = 0u; i < sizeof compat_v2_cache_cases /
                         sizeof compat_v2_cache_cases[0];
         i++) {
      t64_machine *compat_v2_machine = NULL;
      bool okay = create_profile_machine(
          &allocator, "ntsc-cart-4m-openipl3-vr4300-compat-v2", 5u, 11u,
          &compat_v2_machine);

      if (okay) {
        okay = compat_v2_cache_cases[i].run(compat_v2_machine, asset);
      } else {
        (void)puts("# compatibility-v2 cache machine setup failed");
      }
      (void)printf("%s %zu - %s\n", okay ? "ok" : "not ok",
                   existing_case_count +
                       sizeof vr4300_cases / sizeof vr4300_cases[0] + i + 1u,
                   compat_v2_cache_cases[i].name);
      if (!okay) {
        failure_count++;
      }
      if (compat_v2_machine != NULL) {
        t64_machine_destroy(compat_v2_machine);
      }
    }
  }
  if (allocator_state_value.outstanding != 0u) {
    (void)printf("# allocator leak: %zu allocations remain\n",
                 allocator_state_value.outstanding);
    failure_count++;
  }
  return failure_count == 0u ? 0 : 1;
}
