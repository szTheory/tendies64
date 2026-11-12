#include "t64_internal.h"
#include "tendies64/t64_machine.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_IPL3_BYTES 4096u
#define TEST_SP_DMEM_BASE UINT32_C(0x04000000)
#define TEST_SP_IMEM_BASE UINT32_C(0x04001000)
#define TEST_SP_REG_BASE UINT32_C(0x04040000)
#define TEST_CART_BASE UINT32_C(0x10000000)

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

static void check_u64(const char *name, uint64_t actual, uint64_t expected) {
  if (actual != expected) {
    (void)printf("# %s expected=%016llx actual=%016llx\n", name,
                 (unsigned long long)expected, (unsigned long long)actual);
    failure_count++;
  }
}

static void check_status(const char *name, t64_status actual,
                         t64_status expected) {
  check_u64(name, (uint64_t)actual, (uint64_t)expected);
}

static uint8_t dcache_byte(const t64_machine *machine,
                           uint32_t physical_address) {
  const uint32_t tag = physical_address & ~UINT32_C(0x0f);
  uint32_t index;

  for (index = 0u; index < T64_VR4300_DCACHE_LINE_COUNT; index++) {
    const t64_vr4300_dcache_line *line = &machine->cpu.dcache[index];
    if (line->valid && line->physical_tag == tag) {
      return line->data[physical_address & UINT32_C(0x0f)];
    }
  }
  return UINT8_MAX;
}

static void install_program(t64_machine *machine, const uint32_t *program,
                            size_t count) {
  size_t index;

  for (index = 0u; index < count; index++) {
    write_instruction(machine, index, program[index]);
  }
  start_kernel_program(machine);
}

static bool step(t64_machine *machine, const char *name) {
  t64_status status = t64_cpu_step(machine, NULL);

  if (status != T64_STATUS_OK) {
    (void)printf("# %s expected-status=0 actual-status=%u\n", name,
                 (unsigned)status);
    failure_count++;
    return false;
  }
  return true;
}

static void initialize_tlb(t64_machine *machine) {
  size_t index;

  machine->cpu.cp0[10] = 0u;
  for (index = 0u; index < T64_VR4300_TLB_ENTRY_COUNT; index++) {
    machine->cpu.tlb[index] = (t64_vr4300_tlb_pair){0};
    machine->cpu.tlb[index].asid = UINT32_C(0xff);
  }
}

static void map_alias_pair(t64_machine *machine) {
  initialize_tlb(machine);
  machine->cpu.tlb[0].vpn2 = UINT64_C(0x00400000);
  machine->cpu.tlb[0].asid = 0u;
  machine->cpu.tlb[0].entry_lo0 = UINT32_C(0x0000009e);
  machine->cpu.tlb[0].entry_lo1 = UINT32_C(0x000000de);
  machine->cpu.tlb[1].vpn2 = UINT64_C(0x00500000);
  machine->cpu.tlb[1].asid = 0u;
  machine->cpu.tlb[1].entry_lo0 = UINT32_C(0x0000009e);
  machine->cpu.tlb[1].entry_lo1 = UINT32_C(0x000000de);
}

static void map_data_page(t64_machine *machine, uint32_t physical_address) {
  initialize_tlb(machine);
  machine->cpu.tlb[0].vpn2 = UINT64_C(0x00400000);
  machine->cpu.tlb[0].asid = 0u;
  machine->cpu.tlb[0].entry_lo0 =
      ((physical_address >> 12) << 6) | UINT32_C(0x1e);
}

static bool run_memory_ll(t64_machine *machine, uint64_t virtual_address) {
  bool write_gpr = false;
  uint32_t destination = 0u;
  uint64_t value = 0u;

  machine->cpu.gpr[4] = virtual_address;
  if (!t64_memory_execute(machine, UINT32_C(0xc0880000), &write_gpr,
                          &destination, &value) ||
      !write_gpr || destination != 8u || !machine->cpu.linked_valid) {
    (void)printf("# direct LL failed value=%016llx destination=%u link=%u\n",
                 (unsigned long long)value, destination,
                 machine->cpu.linked_valid ? 1u : 0u);
    failure_count++;
    return false;
  }
  machine->cpu.gpr[destination] = value;
  return true;
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

static void test_adjacent_dcache_line_preserves_link(
    const uint8_t asset[TEST_IPL3_BYTES]) {
  static const uint32_t program[] = {
      UINT32_C(0xc0880000), /* ll t0,0(a0) */
      UINT32_C(0xbcb10000), /* cache 0x11,0(a1) */
      UINT32_C(0xe0890000), /* sc t1,0(a0) */
  };
  fixture value;
  size_t index;

  if (!fixture_init(&value, asset)) {
    (void)puts("# adjacent D-cache fixture setup failed");
    failure_count++;
    return;
  }
  install_program(value.machine, program,
                  sizeof program / sizeof program[0]);
  value.machine->cpu.gpr[4] = UINT64_C(0xffffffff80002004);
  value.machine->cpu.gpr[5] = UINT64_C(0xffffffff80002010);
  value.machine->cpu.gpr[9] = UINT64_C(0xdeadbeef);
  for (index = 0u; index < 4u; index++) {
    value.machine->ram[UINT32_C(0x2004) + index] =
        (uint8_t)(UINT8_C(0x11) + (uint8_t)index);
  }
  if (step(value.machine, "adjacent-line LL") &&
      step(value.machine, "adjacent-line CACHE")) {
    check_bool("adjacent physical D line preserves reservation",
               value.machine->cpu.linked_valid, true);
    if (step(value.machine, "adjacent-line SC")) {
      check_u64("adjacent-line SC result", value.machine->cpu.gpr[9], 1u);
    }
  }
  fixture_destroy(&value);
}

static void test_word_alias_and_width_mismatch(
    const uint8_t asset[TEST_IPL3_BYTES]) {
  static const uint32_t alias_program[] = {
      UINT32_C(0xc0880000), /* ll t0,0(a0) */
      UINT32_C(0xe0a90000), /* sc t1,0(a1) */
  };
  static const uint32_t width_program[] = {
      UINT32_C(0xc0880000), /* ll t0,0(a0) */
      UINT32_C(0xf0890000), /* scd t1,0(a0) */
  };
  static const uint32_t different_target_program[] = {
      UINT32_C(0xc0880000), /* ll t0,0(a0) */
      UINT32_C(0xe0890000), /* sc t1,0(a0) */
  };
  fixture value;

  if (!fixture_init(&value, asset)) {
    (void)puts("# physical-alias fixture setup failed");
    failure_count++;
    return;
  }
  map_alias_pair(value.machine);
  install_program(value.machine, alias_program,
                  sizeof alias_program / sizeof alias_program[0]);
  value.machine->cpu.gpr[4] = UINT64_C(0x00400000);
  value.machine->cpu.gpr[5] = UINT64_C(0x00500000);
  value.machine->cpu.gpr[9] = UINT64_C(0xaabbccdd);
  value.machine->ram[UINT32_C(0x2000)] = UINT8_C(0x12);
  value.machine->ram[UINT32_C(0x2001)] = UINT8_C(0x34);
  value.machine->ram[UINT32_C(0x2002)] = UINT8_C(0x56);
  value.machine->ram[UINT32_C(0x2003)] = UINT8_C(0x78);
  if (step(value.machine, "physical-alias LL") &&
      step(value.machine, "physical-alias SC")) {
    check_u64("same physical range through alias succeeds",
              value.machine->cpu.gpr[9], 1u);
      check_u64("physical-alias store commits", dcache_byte(value.machine, 0x2000),
              UINT64_C(0xaa));
  }
  fixture_destroy(&value);

  if (!fixture_init(&value, asset)) {
    (void)puts("# width-mismatch fixture setup failed");
    failure_count++;
    return;
  }
  install_program(value.machine, width_program,
                  sizeof width_program / sizeof width_program[0]);
  value.machine->cpu.gpr[4] = UINT64_C(0xffffffff80002000);
  value.machine->cpu.gpr[9] = UINT64_C(0xaabbccddeeff0011);
  value.machine->ram[UINT32_C(0x2000)] = UINT8_C(0x12);
  value.machine->ram[UINT32_C(0x2001)] = UINT8_C(0x34);
  value.machine->ram[UINT32_C(0x2002)] = UINT8_C(0x56);
  value.machine->ram[UINT32_C(0x2003)] = UINT8_C(0x78);
  if (step(value.machine, "width-mismatch LL")) {
    check_u64("LL sign-extends the linked word",
              value.machine->cpu.gpr[8],
              UINT64_C(0x0000000012345678));
    if (step(value.machine, "width-mismatch SCD")) {
      check_u64("different width fails conditional store",
                value.machine->cpu.gpr[9], 0u);
      check_bool("different-width SC leaves original word",
                 value.machine->ram[UINT32_C(0x2000)] == UINT8_C(0x12),
                 true);
      check_bool("different-width decision consumes reservation",
                 value.machine->cpu.linked_valid, false);
    }
  }
  fixture_destroy(&value);

  if (!fixture_init(&value, asset)) {
    (void)puts("# different-target fixture setup failed");
    failure_count++;
    return;
  }
  install_program(value.machine, different_target_program,
                  sizeof different_target_program /
                      sizeof different_target_program[0]);
  value.machine->cpu.gpr[4] = UINT64_C(0xffffffff80002000);
  value.machine->cpu.gpr[9] = UINT64_C(0xaabbccdd);
  if (step(value.machine, "different-target LL")) {
    value.machine->cpu.gpr[4] += UINT64_C(4);
    if (step(value.machine, "different-target SC")) {
      check_u64("different physical start fails conditional store",
                value.machine->cpu.gpr[9], 0u);
      check_bool("different target decision consumes reservation",
                 value.machine->cpu.linked_valid, false);
    }
  }
  fixture_destroy(&value);
}

static void test_doubleword_and_rt_zero(
    const uint8_t asset[TEST_IPL3_BYTES]) {
  static const uint32_t doubleword_program[] = {
      UINT32_C(0xd0880000), /* lld t0,0(a0) */
      UINT32_C(0xf0890000), /* scd t1,0(a0) */
  };
  static const uint32_t zero_program[] = {
      UINT32_C(0xc0800000), /* ll zero,0(a0) */
      UINT32_C(0xe0800000), /* sc zero,0(a0) */
  };
  fixture value;
  size_t index;

  if (!fixture_init(&value, asset)) {
    (void)puts("# doubleword fixture setup failed");
    failure_count++;
    return;
  }
  install_program(value.machine, doubleword_program,
                  sizeof doubleword_program / sizeof doubleword_program[0]);
  value.machine->cpu.gpr[4] = UINT64_C(0xffffffff80002000);
  value.machine->cpu.gpr[9] = UINT64_C(0xaabbccddeeff0011);
  for (index = 0u; index < 8u; index++) {
    value.machine->ram[UINT32_C(0x2000) + index] =
        (uint8_t)(UINT8_C(0x11) + (uint8_t)index * UINT8_C(0x11));
  }
  if (step(value.machine, "LLD")) {
    check_u64("LLD returns big-endian doubleword",
              value.machine->cpu.gpr[8],
              UINT64_C(0x1122334455667788));
    if (step(value.machine, "SCD")) {
      check_u64("SCD exact doubleword succeeds", value.machine->cpu.gpr[9],
                1u);
      check_u64("SCD updates last byte", dcache_byte(value.machine, 0x2007),
                UINT64_C(0x11));
    }
  }
  fixture_destroy(&value);

  if (!fixture_init(&value, asset)) {
    (void)puts("# rt-zero fixture setup failed");
    failure_count++;
    return;
  }
  install_program(value.machine, zero_program,
                  sizeof zero_program / sizeof zero_program[0]);
  value.machine->cpu.gpr[4] = UINT64_C(0xffffffff80002000);
  value.machine->ram[UINT32_C(0x2000)] = UINT8_C(0xff);
  if (step(value.machine, "LL with rt zero") &&
      step(value.machine, "SC with rt zero")) {
    check_u64("rt zero remains hardwired", value.machine->cpu.gpr[0], 0u);
    check_u64("SC rt zero still performs the cached store",
              dcache_byte(value.machine, UINT32_C(0x2000)), 0u);
  }
  fixture_destroy(&value);
}

static void test_source_register_alias(
    const uint8_t asset[TEST_IPL3_BYTES]) {
  static const uint32_t program[] = {
      UINT32_C(0xc0880000), /* ll t0,0(a0) */
      UINT32_C(0xe1290000), /* sc a1,0(a1): rt is also base */
  };
  fixture value;

  if (!fixture_init(&value, asset)) {
    (void)puts("# source-alias fixture setup failed");
    failure_count++;
    return;
  }
  install_program(value.machine, program, sizeof program / sizeof program[0]);
  value.machine->cpu.gpr[4] = UINT64_C(0xffffffff80002000);
  value.machine->cpu.gpr[9] = UINT64_C(0xffffffff80002000);
  if (step(value.machine, "source-alias LL") &&
      step(value.machine, "source-alias SC")) {
    check_u64("SC rt/base alias succeeds", value.machine->cpu.gpr[9], 1u);
    check_u64("SC captured aliased source before result commit",
              ((uint64_t)dcache_byte(value.machine, 0x2000) << 24) |
                  ((uint64_t)dcache_byte(value.machine, 0x2001) << 16) |
                  ((uint64_t)dcache_byte(value.machine, 0x2002) << 8) |
                  (uint64_t)dcache_byte(value.machine, 0x2003),
              UINT64_C(0x80002000));
  }
  fixture_destroy(&value);
}

static void test_reverse_endian_user_and_lladdr(
    const uint8_t asset[TEST_IPL3_BYTES]) {
  static const uint32_t program[] = {
      UINT32_C(0xc0880000), /* ll t0,0(a0) */
      UINT32_C(0xe0890000), /* sc t1,0(a0) */
      UINT32_C(0xd08a0000), /* lld t2,0(a0), RI in 32-bit user mode */
  };
  fixture value;
  size_t index;
  t64_status status;

  if (!fixture_init(&value, asset)) {
    (void)puts("# reverse-endian fixture setup failed");
    failure_count++;
    return;
  }
  initialize_tlb(value.machine);
  value.machine->cpu.tlb[0].vpn2 = UINT64_C(0x00400000);
  value.machine->cpu.tlb[0].asid = 0u;
  value.machine->cpu.tlb[0].entry_lo0 = UINT32_C(0x0000005e);
  value.machine->cpu.tlb[0].entry_lo1 = UINT32_C(0x0000009e);
  for (index = 0u; index < sizeof program / sizeof program[0]; index++) {
    write_instruction(value.machine, index, program[index]);
  }
  value.machine->cpu.pc = UINT32_C(0x00400000);
  value.machine->cpu.pc_full = UINT64_C(0x00400000);
  value.machine->cpu.cp0[12] = UINT64_C(0x02000010); /* User + RE. */
  value.machine->cpu.cp0[16] = UINT64_C(0x00008000); /* Big Endian. */
  value.machine->cpu.gpr[4] = UINT64_C(0x00401000);
  value.machine->cpu.gpr[9] = UINT64_C(0xa1b2c3d4);
  for (index = 0u; index < 8u; index++) {
    value.machine->ram[UINT32_C(0x2000) + index] =
        (uint8_t)(UINT8_C(0x11) + (uint8_t)index * UINT8_C(0x11));
  }
  if (step(value.machine, "user RE LL")) {
    check_u64("user RE LL selects the inferred physical width-four lane",
              value.machine->cpu.gpr[8], UINT64_C(0x55667788));
    check_u64("LLAddr displays physical PA[31:4]",
              value.machine->cpu.cp0[17], UINT64_C(0x200));
    check_u64("user RE linked physical start", value.machine->cpu.linked_physical_address,
              UINT64_C(0x2004));
    if (step(value.machine, "user RE SC")) {
      check_u64("user RE SC succeeds at same selected lane",
                value.machine->cpu.gpr[9], 1u);
      check_u64("user RE SC first selected byte",
                dcache_byte(value.machine, UINT32_C(0x2004)), UINT64_C(0xa1));
      check_u64("user RE SC last selected byte",
                dcache_byte(value.machine, UINT32_C(0x2007)), UINT64_C(0xd4));
    }
  }
  value.machine->cpu.pc = UINT32_C(0x00400008);
  value.machine->cpu.pc_full = UINT64_C(0x00400008);
  status = t64_cpu_step(value.machine, NULL);
  check_status("LLD in 32-bit user mode raises architectural RI", status,
               T64_STATUS_OK);
  check_u64("LLD user RI Cause code",
            ((uint32_t)value.machine->cpu.cp0[13] >> 2) & UINT32_C(0x1f),
            UINT64_C(10));
  fixture_destroy(&value);
}

static void test_faults_precede_sc_decision(
    const uint8_t asset[TEST_IPL3_BYTES]) {
  static const uint32_t program[] = {
      UINT32_C(0xc0880000), /* ll t0,0(a0) */
      UINT32_C(0xe0890000), /* sc t1,0(a0) */
  };
  fixture value;
  uint32_t cause;

  if (!fixture_init(&value, asset)) {
    (void)puts("# alignment fault fixture setup failed");
    failure_count++;
    return;
  }
  install_program(value.machine, program, sizeof program / sizeof program[0]);
  value.machine->cpu.gpr[4] = UINT64_C(0xffffffff80002000);
  value.machine->cpu.gpr[9] = UINT64_C(0xdeadbeef);
  if (step(value.machine, "alignment LL")) {
    value.machine->cpu.gpr[4] += UINT64_C(1);
    (void)step(value.machine, "misaligned SC");
    cause = (uint32_t)value.machine->cpu.cp0[13];
    check_u64("misaligned SC raises AdES before result",
              (cause >> 2) & UINT32_C(0x1f), UINT64_C(5));
    check_bool("misaligned SC preserves link until exception policy",
               value.machine->cpu.linked_valid, true);
    check_u64("misaligned SC preserves source/result register",
              value.machine->cpu.gpr[9], UINT64_C(0xdeadbeef));
  }
  fixture_destroy(&value);

  if (!fixture_init(&value, asset)) {
    (void)puts("# watch fault fixture setup failed");
    failure_count++;
    return;
  }
  install_program(value.machine, program, sizeof program / sizeof program[0]);
  value.machine->cpu.gpr[4] = UINT64_C(0xffffffff80002000);
  value.machine->cpu.gpr[9] = UINT64_C(0xdeadbeef);
  if (step(value.machine, "watch LL")) {
    value.machine->cpu.cp0[18] = UINT64_C(0x00002001); /* W + PA */
    (void)step(value.machine, "Watch-matched SC");
    cause = (uint32_t)value.machine->cpu.cp0[13];
    check_u64("Watch fault precedes SC result", (cause >> 2) & 0x1fu,
              UINT64_C(23));
    check_bool("Watch fault preserves linked range",
               value.machine->cpu.linked_valid, true);
    check_u64("Watch fault leaves SC source register",
              value.machine->cpu.gpr[9], UINT64_C(0xdeadbeef));
  }
  fixture_destroy(&value);

  if (!fixture_init(&value, asset)) {
    (void)puts("# TLB fault fixture setup failed");
    failure_count++;
    return;
  }
  map_alias_pair(value.machine);
  install_program(value.machine, program, sizeof program / sizeof program[0]);
  value.machine->cpu.gpr[4] = UINT64_C(0x00400000);
  value.machine->cpu.gpr[9] = UINT64_C(0xdeadbeef);
  if (step(value.machine, "TLB LL")) {
    value.machine->cpu.tlb[0].entry_lo0 &= ~UINT32_C(2);
    (void)step(value.machine, "TLB-invalid SC");
    cause = (uint32_t)value.machine->cpu.cp0[13];
    check_u64("TLB invalid fault precedes SC result",
              (cause >> 2) & UINT32_C(0x1f), UINT64_C(3));
    check_bool("TLB invalid fault preserves reservation",
               value.machine->cpu.linked_valid, true);
    check_u64("TLB invalid fault leaves SC source register",
              value.machine->cpu.gpr[9], UINT64_C(0xdeadbeef));
  }
  fixture_destroy(&value);
}

static void test_external_write_intervals(
    const uint8_t asset[TEST_IPL3_BYTES]) {
  fixture value;

  if (!fixture_init(&value, asset)) {
    (void)puts("# interval fixture setup failed");
    failure_count++;
    return;
  }
  if (run_memory_ll(value.machine, UINT64_C(0xffffffff80002004))) {
    check_status("write immediately before linked range",
                 t64_bus_write(value.machine, UINT32_C(0x2003), 1u,
                               UINT32_C(0x51)),
                 T64_STATUS_OK);
    check_bool("one byte before preserves link", value.machine->cpu.linked_valid,
               true);
    check_status("write exactly after linked range",
                 t64_bus_write(value.machine, UINT32_C(0x2008), 1u,
                               UINT32_C(0x52)),
                 T64_STATUS_OK);
    check_bool("exact adjacency preserves link", value.machine->cpu.linked_valid,
               true);
    check_status("write last byte of linked range",
                 t64_bus_write(value.machine, UINT32_C(0x2007), 1u,
                               UINT32_C(0x53)),
                 T64_STATUS_OK);
    check_bool("one-byte end overlap breaks link", value.machine->cpu.linked_valid,
               false);
  }
  if (run_memory_ll(value.machine, UINT64_C(0xffffffff80002004))) {
    check_status("write first byte of linked range",
                 t64_bus_write(value.machine, UINT32_C(0x2004), 1u,
                               UINT32_C(0x54)),
                 T64_STATUS_OK);
    check_bool("one-byte start overlap breaks link", value.machine->cpu.linked_valid,
               false);
  }
  fixture_destroy(&value);

  if (!fixture_init(&value, asset)) {
    (void)puts("# masked-write fixture setup failed");
    failure_count++;
    return;
  }
  if (run_memory_ll(value.machine, UINT64_C(0xffffffff80002000))) {
    check_status("masked write outside linked bytes commits",
                 t64_bus_write_masked(value.machine, UINT32_C(0x2000), 8u,
                                      UINT8_C(0xf0),
                                      UINT64_C(0x0102030405060708)),
                 T64_STATUS_OK);
    check_bool("masked disjoint bytes preserve link",
               value.machine->cpu.linked_valid, true);
    check_status("masked write touching final linked byte commits",
                 t64_bus_write_masked(value.machine, UINT32_C(0x2000), 8u,
                                      UINT8_C(0x08),
                                      UINT64_C(0x0000000000000008)),
                 T64_STATUS_OK);
    check_bool("selected masked overlap breaks link",
               value.machine->cpu.linked_valid, false);
  }
  fixture_destroy(&value);
}

static void test_targeted_dcache_line_helper(
    const uint8_t asset[TEST_IPL3_BYTES]) {
  fixture value;

  if (!fixture_init(&value, asset)) {
    (void)puts("# targeted D-cache helper fixture setup failed");
    failure_count++;
    return;
  }
  if (run_memory_ll(value.machine, UINT64_C(0xffffffff80002004))) {
    t64_cpu_link_invalidate_dcache_line(value.machine, UINT32_C(0x200f));
    check_bool("targeted line helper invalidates linked bytes",
               value.machine->cpu.linked_valid, false);
  }
  if (run_memory_ll(value.machine, UINT64_C(0xffffffff80002004))) {
    t64_cpu_link_invalidate_dcache_line(value.machine, UINT32_C(0x2010));
    check_bool("adjacent targeted line preserves linked bytes",
               value.machine->cpu.linked_valid, true);
  }
  fixture_destroy(&value);
}

static void test_eret_and_rom_reset_clear_link(
    const uint8_t asset[TEST_IPL3_BYTES]) {
  static const uint32_t program[] = {
      UINT32_C(0xc0880000), /* ll t0,0(a0) */
      UINT32_C(0x42000018), /* eret */
      UINT32_C(0xe0890000), /* sc t1,0(a0) */
  };
  fixture value;

  if (!fixture_init(&value, asset)) {
    (void)puts("# ERET fixture setup failed");
    failure_count++;
    return;
  }
  install_program(value.machine, program, sizeof program / sizeof program[0]);
  value.machine->cpu.gpr[4] = UINT64_C(0xffffffff80002000);
  value.machine->cpu.gpr[9] = UINT64_C(0xdeadbeef);
  value.machine->cpu.cp0[12] |= UINT64_C(2); /* EXL. */
  value.machine->cpu.cp0[14] = UINT64_C(0xffffffff80001008);
  if (step(value.machine, "ERET LL")) {
    check_bool("LL establishes state before ERET", value.machine->cpu.linked_valid,
               true);
    if (step(value.machine, "ERET clears LL reservation")) {
      check_bool("ERET clears reservation", value.machine->cpu.linked_valid,
                 false);
      if (step(value.machine, "SC after ERET")) {
        check_u64("SC after ERET fails", value.machine->cpu.gpr[9], 0u);
        check_u64("failed post-ERET SC does not store",
                  value.machine->ram[UINT32_C(0x2000)], 0u);
      }
    }
  }
  fixture_destroy(&value);

  if (!fixture_init(&value, asset)) {
    (void)puts("# reset fixture setup failed");
    failure_count++;
    return;
  }
  if (run_memory_ll(value.machine, UINT64_C(0xffffffff80002000))) {
    check_bool("pre-reset reservation exists", value.machine->cpu.linked_valid,
               true);
    check_status("re-admitting the same image resets guest state",
                 t64_machine_admit_rom(value.machine, asset,
                                       TEST_IPL3_BYTES),
                 T64_STATUS_OK);
    check_bool("machine reset clears reservation", value.machine->cpu.linked_valid,
               false);
    check_u64("machine reset clears linked address",
              value.machine->cpu.linked_physical_address, 0u);
    check_u64("machine reset clears linked width",
              value.machine->cpu.linked_width, 0u);
  }
  fixture_destroy(&value);
}

static void test_rejected_pi_and_sp_preserve_link(
    const uint8_t asset[TEST_IPL3_BYTES]) {
  static const uint32_t program[] = {UINT32_C(0xc0880000)};
  fixture value;
  uint8_t linked_byte_before = 0u;

  if (!fixture_init(&value, asset)) {
    (void)puts("# rejected PI fixture setup failed");
    failure_count++;
    return;
  }
  install_program(value.machine, program, sizeof program / sizeof program[0]);
  value.machine->cpu.gpr[4] = UINT64_C(0xffffffff80002000);
  if (step(value.machine, "rejected PI LL")) {
    linked_byte_before = value.machine->ram[UINT32_C(0x2000)];
    check_status("PI invalid source range is refused before commit",
                 t64_machine_bus_write(value.machine, UINT32_C(0x04600000),
                                       4u, UINT32_C(0x2000)),
                 T64_STATUS_OK);
    check_status("PI source register accepts aligned out-of-range address",
                 t64_machine_bus_write(value.machine, UINT32_C(0x04600004),
                                       4u, UINT32_C(0x10001000)),
                 T64_STATUS_OK);
    check_status("PI rejected DMA has no committed bytes",
                 t64_machine_bus_write(value.machine, UINT32_C(0x0460000c),
                                       4u, UINT32_C(7)),
                 T64_STATUS_BUS_DEVICE_REFUSED);
    check_bool("rejected PI preserves reservation", value.machine->cpu.linked_valid,
               true);
    check_bool("rejected PI leaves linked bytes unchanged",
               value.machine->ram[UINT32_C(0x2000)] == linked_byte_before,
               true);
  }
  fixture_destroy(&value);

  if (!fixture_init(&value, asset)) {
    (void)puts("# rejected SP fixture setup failed");
    failure_count++;
    return;
  }
  install_program(value.machine, program, sizeof program / sizeof program[0]);
  value.machine->cpu.gpr[4] = UINT64_C(0xffffffff80002000);
  if (step(value.machine, "rejected SP LL")) {
    linked_byte_before = value.machine->ram[UINT32_C(0x2000)];
    check_status("SP internal address setup succeeds",
                 t64_machine_bus_write(value.machine, UINT32_C(0x04040000),
                                       4u, 0u),
                 T64_STATUS_OK);
    check_status("SP out-of-range RDRAM address setup succeeds",
                 t64_machine_bus_write(value.machine, UINT32_C(0x04040004),
                                       4u,
                                       value.machine->profile.rdram_bytes),
                 T64_STATUS_OK);
    check_status("SP rejected DMA has no committed bytes",
                 t64_machine_bus_write(value.machine, UINT32_C(0x0404000c),
                                       4u, UINT32_C(7)),
                 T64_STATUS_BUS_DEVICE_REFUSED);
    check_bool("rejected SP preserves reservation", value.machine->cpu.linked_valid,
               true);
    check_bool("rejected SP leaves linked bytes unchanged",
               value.machine->ram[UINT32_C(0x2000)] == linked_byte_before,
               true);
  }
  fixture_destroy(&value);
}

static void test_sp_bank_bus_writes_invalidate_link(
    const uint8_t asset[TEST_IPL3_BYTES], uint32_t physical_base,
    bool instruction_memory) {
  fixture value;
  uint8_t *bank;
  size_t index;

  if (!fixture_init(&value, asset)) {
    (void)puts("# SP backing bus-write fixture setup failed");
    failure_count++;
    return;
  }
  bank = instruction_memory ? value.machine->sp_imem : value.machine->sp_dmem;
  for (index = 0u; index < 8u; index++) {
    bank[index] = (uint8_t)(UINT8_C(0x20) + (uint8_t)index);
  }
  map_data_page(value.machine, physical_base);
  if (run_memory_ll(value.machine, UINT64_C(0x00400000))) {
    check_u64("SP scratchpad LL records physical address",
              value.machine->cpu.linked_physical_address, physical_base);
    check_status("bus write immediately after SP linked range commits",
                 t64_bus_write(value.machine, physical_base + 4u, 1u,
                               UINT32_C(0xaa)),
                 T64_STATUS_OK);
    check_bool("adjacent SP bus write preserves link",
               value.machine->cpu.linked_valid, true);
    check_status("overlapping SP bus write commits",
                 t64_bus_write(value.machine, physical_base + 3u, 1u,
                               UINT32_C(0xbb)),
                 T64_STATUS_OK);
    check_bool("overlapping SP bus write invalidates link",
               value.machine->cpu.linked_valid, false);
  }
  if (run_memory_ll(value.machine, UINT64_C(0x00400000))) {
    check_status("masked SP write after linked bytes commits",
                 t64_bus_write_masked(value.machine, physical_base, 8u,
                                      UINT8_C(0xf0),
                                      UINT64_C(0x0102030405060708)),
                 T64_STATUS_OK);
    check_bool("masked adjacent SP bytes preserve link",
               value.machine->cpu.linked_valid, true);
    check_status("masked SP write touching linked byte commits",
                 t64_bus_write_masked(value.machine, physical_base, 8u,
                                      UINT8_C(0x08),
                                      UINT64_C(0x0000000000000008)),
                 T64_STATUS_OK);
    check_bool("masked SP overlap invalidates link",
               value.machine->cpu.linked_valid, false);
  }
  fixture_destroy(&value);
}

static void test_scheduled_sp_dma_to_sp_bank_invalidates_link(
    const uint8_t asset[TEST_IPL3_BYTES], uint32_t physical_base,
    bool instruction_memory) {
  uint32_t program[32] = {0};
  t64_stop_request request = {T64_STOP_GUEST_TICK, UINT64_C(20)};
  t64_stop_result result = {0u, 0u};
  fixture value;
  t64_status status;
  uint8_t *bank;
  uint32_t index;

  if (!fixture_init(&value, asset)) {
    (void)puts("# SP-to-SP DMA fixture setup failed");
    failure_count++;
    return;
  }
  bank = instruction_memory ? value.machine->sp_imem : value.machine->sp_dmem;
  (void)memset(bank, 0, 8u);
  for (index = 0u; index < 8u; index++) {
    value.machine->ram[UINT32_C(0x3000) + index] =
        (uint8_t)(UINT8_C(0xa0) + (uint8_t)index);
  }
  map_data_page(value.machine, physical_base);
  if (!run_memory_ll(value.machine, UINT64_C(0x00400000))) {
    fixture_destroy(&value);
    return;
  }
  install_program(value.machine, program, sizeof program / sizeof program[0]);
  status = t64_machine_bus_write(value.machine, TEST_SP_REG_BASE, 4u,
                                 instruction_memory ? UINT32_C(0x1000) : 0u);
  if (status == T64_STATUS_OK) {
    status = t64_machine_bus_write(value.machine, TEST_SP_REG_BASE + 4u, 4u,
                                   UINT32_C(0x3000));
  }
  if (status == T64_STATUS_OK) {
    status = t64_machine_bus_write(value.machine, TEST_SP_REG_BASE + 8u, 4u,
                                   UINT32_C(7));
  }
  check_status(instruction_memory ? "SP read-to-IMEM DMA starts"
                                 : "SP read-to-DMEM DMA starts",
               status, T64_STATUS_OK);
  if (status == T64_STATUS_OK) {
    status = t64_machine_advance(value.machine, &request, &result);
    check_status(instruction_memory ? "SP read-to-IMEM DMA advances"
                                   : "SP read-to-DMEM DMA advances",
                 status, T64_STATUS_OK);
    check_u64("SP event path reaches requested guest tick", result.tick,
              UINT64_C(20));
    check_bool("accepted SP-to-SP DMA invalidates overlapping link",
               value.machine->cpu.linked_valid, false);
    for (index = 0u; index < 8u; index++) {
      check_u64(instruction_memory ? "SP IMEM committed byte"
                                   : "SP DMEM committed byte",
                bank[index], UINT64_C(0xa0) + index);
    }
  }
  fixture_destroy(&value);
}

static void test_refused_sc_store_preserves_reservation(
    const uint8_t asset[TEST_IPL3_BYTES]) {
  static const uint32_t program[] = {
      UINT32_C(0xc0880000), /* ll t0,0(a0) */
      UINT32_C(0xe0890000), /* sc t1,0(a0) */
  };
  fixture value;
  t64_status status;
  uint32_t observed = 0u;
  uint32_t expected;

  if (!fixture_init(&value, asset)) {
    (void)puts("# refused-SC fixture setup failed");
    failure_count++;
    return;
  }
  map_data_page(value.machine, TEST_CART_BASE);
  install_program(value.machine, program, sizeof program / sizeof program[0]);
  value.machine->cpu.gpr[4] = UINT64_C(0x00400100);
  value.machine->cpu.gpr[9] = UINT64_C(0xdeadbeef);
  if (step(value.machine, "cacheable cartridge LL")) {
    check_u64("cartridge LL records physical linked address",
              value.machine->cpu.linked_physical_address,
              TEST_CART_BASE + UINT32_C(0x100));
    status = t64_cpu_step(value.machine, NULL);
    check_status("SC to read-only cartridge is refused", status,
                 T64_STATUS_RESOURCE_EXHAUSTED);
    check_bool("refused SC preserves uncompleted reservation",
               value.machine->cpu.linked_valid, true);
    check_u64("refused SC leaves source/result register",
              value.machine->cpu.gpr[9], UINT64_C(0xdeadbeef));
    expected = ((uint32_t)asset[0x100] << 24) |
               ((uint32_t)asset[0x101] << 16) |
               ((uint32_t)asset[0x102] << 8) |
               (uint32_t)asset[0x103];
    check_status("read-only cartridge remains readable after refused SC",
                 t64_bus_read(value.machine, TEST_CART_BASE + UINT32_C(0x100),
                              4u, &observed),
                 T64_STATUS_OK);
    check_u64("refused SC does not alter cartridge bytes", observed,
              expected);
  }
  fixture_destroy(&value);
}

static void test_accepted_nonoverlap_dma_preserves_link(
    const uint8_t asset[TEST_IPL3_BYTES], bool sp_to_rdram) {
  uint32_t program[32] = {0};
  t64_vr4300_dcache_line dcache_before[T64_VR4300_DCACHE_LINE_COUNT];
  t64_stop_request request = {T64_STOP_INSTRUCTION_COUNT, 0u};
  t64_stop_result result = {0u, 0u};
  const uint32_t device_base =
      sp_to_rdram ? UINT32_C(0x04040000) : UINT32_C(0x04600000);
  fixture value;
  t64_status status;
  uint32_t index;

  if (!fixture_init(&value, asset)) {
    (void)puts("# accepted nonoverlap DMA fixture setup failed");
    failure_count++;
    return;
  }
  program[0] = UINT32_C(0xc0880000); /* ll t0,0(a0) */
  program[31] = UINT32_C(0xe0890000); /* sc t1,0(a0) */
  install_program(value.machine, program, 32u);
  value.machine->cpu.gpr[4] = UINT64_C(0xffffffff80002000);
  value.machine->cpu.gpr[9] = UINT64_C(0xdeadbeef);
  value.machine->ram[UINT32_C(0x2000)] = UINT8_C(0x11);
  value.machine->ram[UINT32_C(0x2001)] = UINT8_C(0x22);
  value.machine->ram[UINT32_C(0x2002)] = UINT8_C(0x33);
  value.machine->ram[UINT32_C(0x2003)] = UINT8_C(0x44);
  if (sp_to_rdram) {
    for (index = 0u; index < 8u; index++) {
      value.machine->sp_dmem[index] = (uint8_t)(UINT8_C(0xa0) + (uint8_t)index);
    }
  }
  if (!step(value.machine, sp_to_rdram ? "SP nonoverlap LL" : "PI nonoverlap LL")) {
    fixture_destroy(&value);
    return;
  }
  (void)memcpy(dcache_before, value.machine->cpu.dcache,
               sizeof dcache_before);
  if (sp_to_rdram) {
    status = t64_machine_bus_write(value.machine, device_base, 4u, 0u);
    if (status == T64_STATUS_OK) {
      status = t64_machine_bus_write(value.machine, device_base + 4u, 4u,
                                     UINT32_C(0x3000));
    }
    if (status == T64_STATUS_OK) {
      status = t64_machine_bus_write(value.machine, device_base + 12u, 4u,
                                     UINT32_C(7));
    }
  } else {
    status = t64_machine_bus_write(value.machine, device_base, 4u,
                                   UINT32_C(0x3000));
    if (status == T64_STATUS_OK) {
      status = t64_machine_bus_write(value.machine, device_base + 4u, 4u,
                                     UINT32_C(0x10000000));
    }
    if (status == T64_STATUS_OK) {
      status = t64_machine_bus_write(value.machine, device_base + 12u, 4u,
                                     UINT32_C(7));
    }
  }
  check_status(sp_to_rdram ? "SP nonoverlap DMA starts" : "PI nonoverlap DMA starts",
               status, T64_STATUS_OK);
  if (status == T64_STATUS_OK) {
    request.value = UINT64_C(31);
    status = t64_machine_advance(value.machine, &request, &result);
    check_status(sp_to_rdram ? "SP nonoverlap DMA advances" : "PI nonoverlap DMA advances",
                 status, T64_STATUS_OK);
    check_u64(sp_to_rdram ? "SP guest program reaches SC" : "PI guest program reaches SC",
              result.reason, T64_STOP_REASON_INSTRUCTION_COUNT);
    check_bool(sp_to_rdram ? "disjoint SP write preserves link before SC" : "disjoint PI write preserves link before SC",
               value.machine->cpu.linked_valid, true);
    check_bool(sp_to_rdram ? "disjoint SP write does not snoop D-cache" : "disjoint PI write does not snoop D-cache",
               memcmp(dcache_before, value.machine->cpu.dcache,
                      sizeof dcache_before) == 0,
               true);
    for (index = 0u; index < 8u; index++) {
      uint8_t expected = sp_to_rdram
                             ? (uint8_t)(UINT8_C(0xa0) + (uint8_t)index)
                             : asset[index];
      check_u64(sp_to_rdram ? "SP accepted write byte" : "PI accepted write byte",
                value.machine->ram[UINT32_C(0x3000) + index], expected);
    }
    if (step(value.machine, sp_to_rdram ? "SC after disjoint SP write"
                                        : "SC after disjoint PI write")) {
      check_u64(sp_to_rdram ? "SC succeeds after disjoint SP write"
                            : "SC succeeds after disjoint PI write",
                value.machine->cpu.gpr[9], 1u);
    }
  }
  fixture_destroy(&value);
}

static void test_link_state_is_digest_owned_and_machine_local(
    const uint8_t asset[TEST_IPL3_BYTES]) {
  fixture left = {0};
  fixture right = {0};
  uint8_t left_baseline[T64_DIGEST_SIZE];
  uint8_t left_changed[T64_DIGEST_SIZE];
  uint8_t right_digest[T64_DIGEST_SIZE];
  bool setup_ok = fixture_init(&left, asset) && fixture_init(&right, asset);

  if (!setup_ok) {
    (void)puts("# digest/inter-machine fixture setup failed");
    failure_count++;
    if (left.machine != NULL) {
      fixture_destroy(&left);
    }
    if (right.machine != NULL) {
      fixture_destroy(&right);
    }
    return;
  }
  check_status("left baseline digest", t64_machine_digest(left.machine,
                                                           left_baseline),
               T64_STATUS_OK);
  check_status("right baseline digest", t64_machine_digest(right.machine,
                                                            right_digest),
               T64_STATUS_OK);
  check_bool("equal fresh machines have equal digests",
             memcmp(left_baseline, right_digest, sizeof left_baseline) == 0,
             true);

  left.machine->cpu.linked_physical_address = UINT32_C(0x2000);
  check_status("digest after link-address mutation",
               t64_machine_digest(left.machine, left_changed), T64_STATUS_OK);
  check_bool("link address changes the v9 digest",
             memcmp(left_baseline, left_changed, sizeof left_baseline) != 0,
             true);
  left.machine->cpu.linked_physical_address = 0u;
  left.machine->cpu.linked_width = 4u;
  check_status("digest after link-width mutation",
               t64_machine_digest(left.machine, left_changed), T64_STATUS_OK);
  check_bool("link width changes the v9 digest",
             memcmp(left_baseline, left_changed, sizeof left_baseline) != 0,
             true);
  left.machine->cpu.linked_width = 0u;
  left.machine->cpu.linked_valid = true;
  check_status("digest after link-validity mutation",
               t64_machine_digest(left.machine, left_changed), T64_STATUS_OK);
  check_bool("link validity changes the v9 digest",
             memcmp(left_baseline, left_changed, sizeof left_baseline) != 0,
             true);
  check_status("right machine remains independently digestible",
               t64_machine_digest(right.machine, right_digest), T64_STATUS_OK);
  check_bool("left reservation does not leak into right machine",
             right.machine->cpu.linked_valid, false);
  check_bool("right machine digest remains at its baseline",
             memcmp(left_baseline, right_digest, sizeof left_baseline) == 0,
             true);
  fixture_destroy(&right);
  fixture_destroy(&left);
}

static void test_accepted_pi_nonoverlap(
    const uint8_t asset[TEST_IPL3_BYTES]) {
  test_accepted_nonoverlap_dma_preserves_link(asset, false);
}

static void test_accepted_sp_nonoverlap(
    const uint8_t asset[TEST_IPL3_BYTES]) {
  test_accepted_nonoverlap_dma_preserves_link(asset, true);
}

static void test_sp_dmem_bus_write(
    const uint8_t asset[TEST_IPL3_BYTES]) {
  test_sp_bank_bus_writes_invalidate_link(asset, TEST_SP_DMEM_BASE, false);
}

static void test_sp_imem_bus_write(
    const uint8_t asset[TEST_IPL3_BYTES]) {
  test_sp_bank_bus_writes_invalidate_link(asset, TEST_SP_IMEM_BASE, true);
}

static void test_sp_dma_to_dmem(
    const uint8_t asset[TEST_IPL3_BYTES]) {
  test_scheduled_sp_dma_to_sp_bank_invalidates_link(
      asset, TEST_SP_DMEM_BASE, false);
}

static void test_sp_dma_to_imem(
    const uint8_t asset[TEST_IPL3_BYTES]) {
  test_scheduled_sp_dma_to_sp_bank_invalidates_link(
      asset, TEST_SP_IMEM_BASE, true);
}

typedef void (*cpu_link_test_fn)(const uint8_t asset[TEST_IPL3_BYTES]);

typedef struct {
  const char *name;
  cpu_link_test_fn run;
} cpu_link_test_case;

int main(void) {
  static const cpu_link_test_case cases[] = {
      {"targeted D-cache invalidation clears overlapping reservation",
       test_targeted_dcache_invalidation},
      {"adjacent D-cache line preserves reservation",
       test_adjacent_dcache_line_preserves_link},
      {"physical aliases, width mismatch and word sign extension",
       test_word_alias_and_width_mismatch},
      {"LLD/SCD and rt zero conditional-store behavior",
       test_doubleword_and_rt_zero},
      {"SC captures source before aliased result write",
       test_source_register_alias},
      {"reverse-endian LL lane, LLAddr and user-mode LLD RI",
       test_reverse_endian_user_and_lladdr},
      {"SC alignment, Watch and TLB faults precede result/store",
       test_faults_precede_sc_decision},
      {"committed-byte overlap and masked-write intervals",
       test_external_write_intervals},
      {"targeted D-cache helper invalidates exactly one physical line",
       test_targeted_dcache_line_helper},
      {"ERET and ROM re-admission reset reservation state",
       test_eret_and_rom_reset_clear_link},
      {"rejected PI and SP writers preserve reservation",
       test_rejected_pi_and_sp_preserve_link},
      {"direct SP DMEM bus writes invalidate physical aliases",
       test_sp_dmem_bus_write},
      {"direct SP IMEM bus writes invalidate physical aliases",
       test_sp_imem_bus_write},
      {"scheduled SP read-to-DMEM invalidates linked scratchpad bytes",
       test_sp_dma_to_dmem},
      {"scheduled SP read-to-IMEM invalidates linked scratchpad bytes",
       test_sp_dma_to_imem},
      {"refused SC store preserves its uncompleted reservation",
       test_refused_sc_store_preserves_reservation},
      {"accepted nonoverlapping PI write preserves reservation",
       test_accepted_pi_nonoverlap},
      {"accepted nonoverlapping SP write preserves reservation",
       test_accepted_sp_nonoverlap},
      {"link address/width/validity are digested and machine-local",
       test_link_state_is_digest_owned_and_machine_local},
  };
  uint8_t asset[TEST_IPL3_BYTES];
  size_t index;

  (void)printf("1..%zu\n", sizeof cases / sizeof cases[0]);
  if (!read_qualified_asset(asset)) {
    (void)puts("not ok 1 - qualified IPL3 fixture");
    return 1;
  }
  for (index = 0u; index < sizeof cases / sizeof cases[0]; index++) {
    unsigned before = failure_count;

    cases[index].run(asset);
    if (failure_count == before) {
      (void)printf("ok %zu - %s\n", index + 1u, cases[index].name);
    } else {
      (void)printf("not ok %zu - %s\n", index + 1u, cases[index].name);
    }
  }
  return failure_count == 0u ? 0 : 1;
}
