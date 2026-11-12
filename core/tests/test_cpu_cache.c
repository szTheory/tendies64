#include "t64_internal.h"
#include "tendies64/t64_machine.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IPL3_ASSET_BYTES 4096u
#define CP0_CAUSE UINT32_C(13)
#define CP0_ENTRYHI UINT32_C(10)
#define CP0_STATUS UINT32_C(12)
#define CP0_CONFIG UINT32_C(16)
#define CP0_TAGLO UINT32_C(28)
#define ENTRYLO_VALID UINT32_C(0x2)
#define ENTRYLO_DIRTY UINT32_C(0x4)
#define ENTRYLO_CACHE_SHIFT 3u
#define CP0_TAGHI UINT32_C(29)
#define CACHE_INDEX_INVALIDATE_I UINT32_C(0x00)
#define CACHE_INDEX_WRITEBACK_INVALIDATE_D UINT32_C(0x01)
#define CACHE_INDEX_LOAD_TAG_I UINT32_C(0x04)
#define CACHE_INDEX_LOAD_TAG_D UINT32_C(0x05)
#define CACHE_INDEX_STORE_TAG_I UINT32_C(0x08)
#define CACHE_INDEX_STORE_TAG_D UINT32_C(0x09)
#define CACHE_CREATE_DIRTY_EXCLUSIVE_D UINT32_C(0x0d)
#define CACHE_HIT_INVALIDATE_I UINT32_C(0x10)
#define CACHE_HIT_INVALIDATE_D UINT32_C(0x11)
#define CACHE_FILL_I UINT32_C(0x14)
#define CACHE_HIT_WRITEBACK_INVALIDATE_D UINT32_C(0x15)
#define CACHE_HIT_WRITEBACK_I UINT32_C(0x18)
#define CACHE_HIT_WRITEBACK_D UINT32_C(0x19)

typedef struct {
  size_t outstanding;
} allocator_state;

typedef struct {
  allocator_state allocator;
  t64_machine *machine;
  const uint8_t *asset;
} fixture;

typedef bool (*cache_test_fn)(fixture *value);

typedef struct {
  const char *name;
  cache_test_fn run;
} cache_test_case;

static bool read_seed_asset(uint8_t bytes[IPL3_ASSET_BYTES]) {
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

static bool fixture_open(fixture *value,
                         const uint8_t asset[IPL3_ASSET_BYTES]) {
  const t64_host_allocator allocator = {
      test_allocate, test_release, &value->allocator};
  const t64_profile_descriptor *profile = NULL;

  if (value == NULL || asset == NULL ||
      t64_profile_lookup("ntsc-cart-4m-openipl3-vr4300-v1", &profile) !=
          T64_STATUS_OK ||
      profile == NULL ||
      t64_machine_create(&allocator, profile, &value->machine) !=
          T64_STATUS_OK ||
      value->machine == NULL ||
      t64_machine_admit_rom(value->machine, asset, IPL3_ASSET_BYTES) !=
          T64_STATUS_OK) {
    return false;
  }
  value->asset = asset;
  return true;
}

static bool fixture_close(fixture *value, bool okay) {
  if (value->machine != NULL) {
    t64_machine_destroy(value->machine);
    value->machine = NULL;
  }
  if (value->allocator.outstanding != 0u) {
    (void)printf("# fixture leaked %zu host allocations\n",
                 value->allocator.outstanding);
    return false;
  }
  return okay;
}

static void write_be32(uint8_t *bytes, uint32_t value) {
  bytes[0] = (uint8_t)(value >> 24);
  bytes[1] = (uint8_t)(value >> 16);
  bytes[2] = (uint8_t)(value >> 8);
  bytes[3] = (uint8_t)value;
}

static uint32_t read_be32(const uint8_t *bytes) {
  return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
         ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

static void write_ram_instruction(t64_machine *machine,
                                  uint32_t physical_address,
                                  uint32_t opcode) {
  write_be32(machine->ram + physical_address, opcode);
}

static void set_pc(t64_machine *machine, uint32_t virtual_address) {
  machine->cpu.pc = virtual_address;
  machine->cpu.pc_full = t64_cpu_canonical_address(virtual_address);
}

static uint32_t cache_index(uint32_t virtual_address) {
  return (virtual_address >> 5) & (T64_VR4300_ICACHE_LINE_COUNT - 1u);
}

static uint32_t dcache_index(uint32_t virtual_address) {
  return (virtual_address >> 4) & (T64_VR4300_DCACHE_LINE_COUNT - 1u);
}

static uint32_t entry_lo(uint64_t physical_base, uint32_t cache_algorithm) {
  return (uint32_t)((((physical_base >> 12) << 6) &
                      UINT64_C(0x03ffffc0))) |
         ((cache_algorithm & UINT32_C(7)) << ENTRYLO_CACHE_SHIFT) |
         ENTRYLO_VALID | ENTRYLO_DIRTY;
}

static void set_tlb_pair(t64_machine *machine, uint32_t index,
                         uint64_t virtual_base, uint64_t physical_base,
                         uint32_t cache_algorithm) {
  t64_vr4300_tlb_pair *entry = &machine->cpu.tlb[index];
  uint32_t value = entry_lo(physical_base, cache_algorithm);

  entry->page_mask = 0u;
  entry->vpn2 = virtual_base & UINT64_C(0x000000ffffffe000);
  entry->region = (virtual_base >> 62) & UINT64_C(3);
  entry->asid = (uint32_t)machine->cpu.cp0[CP0_ENTRYHI] & UINT32_C(0xff);
  entry->entry_lo0 = value;
  entry->entry_lo1 = value;
}

static bool line_matches(const t64_vr4300_icache_line *line,
                         uint32_t physical_tag,
                         const uint8_t expected[T64_VR4300_ICACHE_LINE_BYTES]) {
  return line->valid && line->physical_tag == physical_tag &&
         memcmp(line->data, expected, T64_VR4300_ICACHE_LINE_BYTES) == 0;
}

static bool test_index_wrap_and_same_index_replacement(
    fixture *value) {
  const uint32_t first_va = UINT32_C(0x80003ffc);
  const uint32_t next_index_va = UINT32_C(0x80004000);
  const uint32_t same_index_va = UINT32_C(0x80007ffc);
  const uint32_t index_511 = cache_index(first_va);
  const uint32_t index_0 = cache_index(next_index_va);
  t64_machine *machine = value->machine;

  if (index_511 != T64_VR4300_ICACHE_LINE_COUNT - 1u || index_0 != 0u) {
    (void)printf("# derived indices expected 511,0 observed %u,%u\n",
                 index_511, index_0);
    return false;
  }
  write_ram_instruction(machine, UINT32_C(0x3ffc), UINT32_C(0x24080011));
  write_ram_instruction(machine, UINT32_C(0x4000), UINT32_C(0x24080022));
  write_ram_instruction(machine, UINT32_C(0x7ffc), UINT32_C(0x24080033));

  set_pc(machine, first_va);
  if (t64_cpu_step(machine, NULL) != T64_STATUS_OK ||
      machine->cpu.gpr[8] != UINT64_C(0x11) ||
      !machine->cpu.icache[index_511].valid ||
      machine->cpu.icache[index_511].physical_tag != UINT32_C(0x3fe0) ||
      machine->cpu.icache[index_0].valid) {
    (void)printf("# line 511/offset 28 expected key=%08x observed valid=%u "
                 "tag=%08x gpr8=%016llx\n",
                 UINT32_C(0x3fe0),
                 machine->cpu.icache[index_511].valid ? 1u : 0u,
                 machine->cpu.icache[index_511].physical_tag,
                 (unsigned long long)machine->cpu.gpr[8]);
    return false;
  }
  set_pc(machine, next_index_va);
  if (t64_cpu_step(machine, NULL) != T64_STATUS_OK ||
      machine->cpu.gpr[8] != UINT64_C(0x22) ||
      !machine->cpu.icache[index_0].valid ||
      machine->cpu.icache[index_0].physical_tag != UINT32_C(0x4000)) {
    (void)puts("# next line did not wrap index 511 to index 0");
    return false;
  }
  set_pc(machine, same_index_va);
  if (t64_cpu_step(machine, NULL) != T64_STATUS_OK ||
      machine->cpu.gpr[8] != UINT64_C(0x33) ||
      !machine->cpu.icache[index_511].valid ||
      machine->cpu.icache[index_511].physical_tag != UINT32_C(0x7fe0)) {
    (void)puts("# different physical tag did not replace index 511");
    return false;
  }
  set_pc(machine, first_va);
  if (t64_cpu_step(machine, NULL) != T64_STATUS_OK ||
      machine->cpu.gpr[8] != UINT64_C(0x11) ||
      machine->cpu.icache[index_511].physical_tag != UINT32_C(0x3fe0)) {
    (void)puts("# evicted line did not refill its original instruction");
    return false;
  }
  return true;
}

static bool test_tlb_synonyms_and_uncached_bypass(
    fixture *value) {
  const uint32_t first_va = UINT32_C(0x00004000);
  const uint32_t alias_va = UINT32_C(0x00005000);
  const uint32_t first_index = cache_index(first_va);
  const uint32_t alias_index = cache_index(alias_va);
  const uint32_t old_instruction = UINT32_C(0x24080001);
  const uint32_t new_instruction = UINT32_C(0x24080002);
  t64_machine *machine = value->machine;

  if (first_index == alias_index) {
    (void)puts("# independent TLB synonyms must select distinct virtual indices");
    return false;
  }
  set_tlb_pair(machine, 0u, UINT64_C(0x4000), UINT64_C(0x1000), 3u);
  write_ram_instruction(machine, UINT32_C(0x1000), old_instruction);
  set_pc(machine, first_va);
  if (t64_cpu_step(machine, NULL) != T64_STATUS_OK ||
      machine->cpu.gpr[8] != UINT64_C(1) ||
      !machine->cpu.icache[first_index].valid ||
      machine->cpu.icache[first_index].physical_tag != UINT32_C(0x1000)) {
    (void)puts("# even TLB alias did not fetch the original instruction");
    return false;
  }

  write_ram_instruction(machine, UINT32_C(0x1000), new_instruction);
  set_pc(machine, alias_va);
  if (t64_cpu_step(machine, NULL) != T64_STATUS_OK ||
      machine->cpu.gpr[8] != UINT64_C(2) ||
      !machine->cpu.icache[alias_index].valid ||
      machine->cpu.icache[alias_index].physical_tag != UINT32_C(0x1000) ||
      read_be32(machine->cpu.icache[alias_index].data) != new_instruction ||
      read_be32(machine->cpu.icache[first_index].data) != old_instruction) {
    (void)puts("# same physical tag at a virtual synonym did not use its own line");
    return false;
  }
  set_pc(machine, first_va);
  if (t64_cpu_step(machine, NULL) != T64_STATUS_OK ||
      machine->cpu.gpr[8] != UINT64_C(1)) {
    (void)puts("# original synonym did not retain its stale instruction");
    return false;
  }

  machine->cpu.tlb[0].entry_lo0 = entry_lo(UINT64_C(0x1000), 2u);
  set_pc(machine, first_va);
  if (t64_cpu_step(machine, NULL) != T64_STATUS_OK ||
      machine->cpu.gpr[8] != UINT64_C(2) ||
      read_be32(machine->cpu.icache[first_index].data) != old_instruction ||
      !machine->cpu.icache[first_index].valid) {
    (void)puts("# TLB C=2 did not bypass I-cache while preserving its valid line");
    return false;
  }

  machine->cpu.cp0[CP0_CONFIG] =
      (machine->cpu.cp0[CP0_CONFIG] & ~UINT64_C(7)) | UINT64_C(2);
  write_ram_instruction(machine, UINT32_C(0x4000), UINT32_C(0x24080003));
  set_pc(machine, UINT32_C(0x80004000));
  if (t64_cpu_step(machine, NULL) != T64_STATUS_OK ||
      machine->cpu.gpr[8] != UINT64_C(3) ||
      !machine->cpu.icache[first_index].valid ||
      machine->cpu.icache[first_index].physical_tag != UINT32_C(0x1000) ||
      read_be32(machine->cpu.icache[first_index].data) != old_instruction) {
    (void)puts("# Config.K0=2 did not bypass I-cache while preserving its line");
    return false;
  }
  return true;
}

static bool test_tlb_cache_algorithm_attributes(
    fixture *value) {
  static const uint32_t algorithms[] = {0u, 1u, 3u};
  const uint32_t virtual_address = UINT32_C(0x00004000);
  const uint32_t index = cache_index(virtual_address);
  t64_machine *machine = value->machine;
  size_t i;

  for (i = 0u; i < sizeof algorithms / sizeof algorithms[0]; i++) {
    uint32_t expected = UINT32_C(0x24080010) + (uint32_t)i;

    machine->cpu.icache[index] = (t64_vr4300_icache_line){0};
    set_tlb_pair(machine, 0u, UINT64_C(0x4000), UINT64_C(0x1000),
                 algorithms[i]);
    write_ram_instruction(machine, UINT32_C(0x1000), expected);
    set_pc(machine, virtual_address);
    if (t64_cpu_step(machine, NULL) != T64_STATUS_OK ||
        machine->cpu.last_opcode != expected ||
        machine->cpu.icache[index].physical_tag != UINT32_C(0x1000) ||
        !machine->cpu.icache[index].valid) {
      (void)printf("# TLB C=%u expected cached opcode=%08x observed=%08x "
                   "valid=%u tag=%08x\n",
                   algorithms[i], expected, machine->cpu.last_opcode,
                   machine->cpu.icache[index].valid ? 1u : 0u,
                   machine->cpu.icache[index].physical_tag);
      return false;
    }
  }
  return true;
}

static bool test_all_cacheable_backing_domains(
    fixture *value) {
  static const uint32_t cache_vas[] = {
      UINT32_C(0x84000000), UINT32_C(0x84001000), UINT32_C(0x90000040)};
  static const uint32_t physical_tags[] = {
      UINT32_C(0x04000000), UINT32_C(0x04001000), UINT32_C(0x10000040)};
  t64_machine *machine = value->machine;
  size_t i;

  write_be32(machine->sp_dmem, UINT32_C(0x24080011));
  write_be32(machine->sp_imem, UINT32_C(0x24080022));
  for (i = 0u; i < sizeof cache_vas / sizeof cache_vas[0]; i++) {
    const uint32_t index = cache_index(cache_vas[i]);
    const uint8_t *expected_bytes;
    t64_status status;

    if (i == 0u) {
      expected_bytes = machine->sp_dmem;
    } else if (i == 1u) {
      expected_bytes = machine->sp_imem;
    } else {
      expected_bytes = (const uint8_t *)machine->image_storage.data + 0x40u;
    }
    set_pc(machine, cache_vas[i]);
    status = t64_cpu_step(machine, NULL);
    if (status != T64_STATUS_OK &&
        status != T64_STATUS_RESOURCE_EXHAUSTED) {
      (void)printf("# cacheable backing fetch %zu returned status=%u\n", i,
                   (unsigned)status);
      return false;
    }
    if (!machine->cpu.icache[index].valid ||
        machine->cpu.icache[index].physical_tag != physical_tags[i] ||
        memcmp(machine->cpu.icache[index].data, expected_bytes,
               T64_VR4300_ICACHE_LINE_BYTES) != 0) {
      (void)printf("# cacheable backing domain %zu did not fill expected line "
                   "valid=%u tag=%08x first=%08x\n",
                   i, machine->cpu.icache[index].valid ? 1u : 0u,
                   machine->cpu.icache[index].physical_tag,
                   read_be32(machine->cpu.icache[index].data));
      return false;
    }
  }
  return true;
}

static bool test_cartridge_range_refusal_preserves_line(fixture *value) {
  const uint32_t virtual_address = UINT32_C(0x80004000);
  const uint32_t index = cache_index(virtual_address);
  const uint32_t valid_physical_address = UINT32_C(0x10000000);
  const uint32_t out_of_image_physical_address = UINT32_C(0x10001000);
  t64_machine *machine = value->machine;
  t64_vr4300_icache_line before;
  uint32_t opcode = UINT32_C(0xdeadbeef);
  uint32_t opcode_before;
  t64_status status;

  if (machine->image_length != IPL3_ASSET_BYTES) {
    (void)printf("# fixture image length expected %u observed %zu\n",
                 IPL3_ASSET_BYTES, machine->image_length);
    return false;
  }
  status = t64_cpu_icache_fetch(machine, virtual_address,
                                valid_physical_address, &opcode);
  if (status != T64_STATUS_OK ||
      !machine->cpu.icache[index].valid ||
      machine->cpu.icache[index].physical_tag != valid_physical_address ||
      memcmp(machine->cpu.icache[index].data, machine->image_storage.data,
             T64_VR4300_ICACHE_LINE_BYTES) != 0) {
    (void)printf("# cartridge boundary setup status=%u valid=%u tag=%08x\n",
                 (unsigned)status, machine->cpu.icache[index].valid ? 1u : 0u,
                 machine->cpu.icache[index].physical_tag);
    return false;
  }
  before = machine->cpu.icache[index];
  opcode_before = opcode;
  status = t64_cpu_icache_fetch(machine, virtual_address,
                                out_of_image_physical_address, &opcode);
  if (status != T64_STATUS_BUS_RANGE || opcode != opcode_before ||
      !line_matches(&machine->cpu.icache[index], before.physical_tag,
                    before.data)) {
    (void)printf("# cartridge fill expected range refusal preserving tag=%08x "
                 "opcode=%08x; status=%u opcode=%08x valid=%u tag=%08x\n",
                 before.physical_tag, opcode_before, (unsigned)status, opcode,
                 machine->cpu.icache[index].valid ? 1u : 0u,
                 machine->cpu.icache[index].physical_tag);
    return false;
  }
  return true;
}

static uint32_t cache_instruction(uint32_t operation, uint32_t base) {
  return UINT32_C(0xbc000000) | ((base & UINT32_C(0x1f)) << 21) |
         ((operation & UINT32_C(0x1f)) << 16);
}

static t64_status run_cache_instruction(fixture *value, uint32_t operation,
                                        uint32_t target_virtual_address) {
  const uint32_t code_virtual_address = UINT32_C(0x80003000);
  const uint32_t code_physical_address = UINT32_C(0x00003000);

  write_ram_instruction(value->machine, code_physical_address,
                        cache_instruction(operation, 5u));
  value->machine->cpu.gpr[5] =
      t64_cpu_canonical_address(target_virtual_address);
  set_pc(value->machine, code_virtual_address);
  return t64_cpu_step(value->machine, NULL);
}

static void seed_dcache_line(t64_machine *machine, uint32_t virtual_address,
                             uint32_t physical_tag, bool valid, bool dirty,
                             uint8_t first_byte) {
  const uint32_t index = dcache_index(virtual_address);
  t64_vr4300_dcache_line *line = &machine->cpu.dcache[index];
  size_t offset;

  line->physical_tag = physical_tag;
  line->valid = valid;
  line->dirty = dirty;
  for (offset = 0u; offset < sizeof line->data; offset++) {
    line->data[offset] = (uint8_t)(first_byte + (uint8_t)offset);
  }
}

/* NEC VR4300 User's Manual, 7th ed., §16, printed p.404, op 6/I row:
 * when the matching I-cache block contains the address, Hit_Write_Back writes
 * the block to main memory unconditionally. Independent expectation: copy all
 * 32 selected bytes to the physical tag address; retain I-cache valid/tag/data
 * and do not change the unrelated [0x1004,0x1008) LL reservation. */
static bool test_hit_writeback_i_unconditional(fixture *value) {
  const uint32_t target_va = UINT32_C(0x80001000);
  const uint32_t target_pa = UINT32_C(0x00001000);
  const uint32_t code_va = UINT32_C(0x80003000);
  const uint32_t code_pa = UINT32_C(0x00003000);
  const uint32_t target_index = cache_index(target_va);
  uint8_t expected[T64_VR4300_ICACHE_LINE_BYTES];
  t64_vr4300_icache_line *line = &value->machine->cpu.icache[target_index];
  uint32_t ignored_opcode = 0u;
  t64_status status;
  size_t index;

  for (index = 0u; index < sizeof expected; index++) {
    value->machine->ram[target_pa + (uint32_t)index] =
        (uint8_t)(UINT8_C(0x10) + (uint8_t)index);
    expected[index] = (uint8_t)(UINT8_C(0xa0) + (uint8_t)index);
  }
  if (t64_cpu_icache_fetch(value->machine, target_va, target_pa,
                           &ignored_opcode) != T64_STATUS_OK) {
    (void)puts("# I Hit_Write_Back matching line setup failed");
    return false;
  }
  (void)memcpy(line->data, expected, sizeof expected);
  write_ram_instruction(value->machine, code_pa,
                        cache_instruction(CACHE_HIT_WRITEBACK_I, 5u));
  value->machine->cpu.gpr[5] =
      t64_cpu_canonical_address(target_va);
  value->machine->cpu.linked_valid = true;
  value->machine->cpu.linked_physical_address = UINT32_C(0x1004);
  value->machine->cpu.linked_width = UINT32_C(4);
  set_pc(value->machine, code_va);

  status = t64_cpu_step(value->machine, NULL);
  if (status != T64_STATUS_OK ||
      memcmp(value->machine->ram + target_pa, expected, sizeof expected) != 0 ||
      !line->valid || line->physical_tag != target_pa ||
      memcmp(line->data, expected, sizeof expected) != 0 ||
      !value->machine->cpu.linked_valid ||
      value->machine->cpu.linked_physical_address != UINT32_C(0x1004) ||
      value->machine->cpu.linked_width != UINT32_C(4) ||
      !value->machine->cache.operation_seen ||
      value->machine->cache.last_operation != CACHE_HIT_WRITEBACK_I) {
    (void)printf("# p.404 op6/I expected status=0 backing=%08x valid=1 "
                 "tag=%08x link=1/00001004/4; observed status=%u "
                 "backing=%08x valid=%u tag=%08x link=%u/%08x/%u op=%u\n",
                 read_be32(expected), target_pa, (unsigned)status,
                 read_be32(value->machine->ram + target_pa),
                 line->valid ? 1u : 0u, line->physical_tag,
                 value->machine->cpu.linked_valid ? 1u : 0u,
                 value->machine->cpu.linked_physical_address,
                 value->machine->cpu.linked_width,
                 value->machine->cache.last_operation);
    return false;
  }
  return true;
}

static bool test_hit_writeback_i_readonly_refusal_is_atomic(
    fixture *value) {
  const uint32_t target_va = UINT32_C(0x90000000);
  const uint32_t target_pa = UINT32_C(0x10000000);
  const uint32_t target_index = cache_index(target_va);
  t64_vr4300_icache_line *line = &value->machine->cpu.icache[target_index];
  t64_vr4300_icache_line before;
  t64_cache_state cache_before = value->machine->cache;
  uint8_t image_before[IPL3_ASSET_BYTES];
  uint32_t ignored_opcode = 0u;
  t64_status status;

  if (t64_cpu_icache_fetch(value->machine, t64_cpu_canonical_address(target_va),
                           target_pa, &ignored_opcode) != T64_STATUS_OK) {
    (void)puts("# I Hit_Write_Back readonly-line setup failed");
    return false;
  }
  memset(line->data, UINT8_C(0x6a), sizeof line->data);
  before = *line;
  memcpy(image_before, value->machine->image_storage.data,
         sizeof image_before);
  value->machine->cpu.linked_valid = true;
  value->machine->cpu.linked_physical_address = UINT32_C(0x10000004);
  value->machine->cpu.linked_width = 4u;

  status = run_cache_instruction(value, CACHE_HIT_WRITEBACK_I, target_va);
  if (status != T64_STATUS_RESOURCE_EXHAUSTED ||
      !value->machine->cpu.stopped ||
      value->machine->cpu.stop_reason != T64_STOP_REASON_CPU_UNSUPPORTED ||
      memcmp(line, &before, sizeof before) != 0 ||
      memcmp(value->machine->image_storage.data, image_before,
             sizeof image_before) != 0 ||
      memcmp(&value->machine->cache, &cache_before, sizeof cache_before) != 0 ||
      !value->machine->cpu.linked_valid ||
      value->machine->cpu.linked_physical_address != UINT32_C(0x10000004) ||
      value->machine->cpu.linked_width != 4u) {
    (void)puts("# p.404 op6/I readonly writeback refusal should stop before backing, I-line, diagnostics, or link mutation");
    return false;
  }
  return true;
}

static bool test_cache_mapped_device_writeback_remains_model_refusal(
    fixture *value) {
  const uint32_t target_va = UINT32_C(0x90000000);
  const uint32_t target_pa = UINT32_C(0x10000000);
  const uint32_t index = dcache_index(target_va);
  t64_vr4300_dcache_line *line = &value->machine->cpu.dcache[index];
  t64_vr4300_dcache_line before;
  t64_cache_state cache_before = value->machine->cache;
  uint8_t image_before[IPL3_ASSET_BYTES];
  t64_status status;

  seed_dcache_line(value->machine, target_va, target_pa, true, true,
                   UINT8_C(0xc0));
  before = *line;
  memcpy(image_before, value->machine->image_storage.data,
         sizeof image_before);
  value->machine->cpu.linked_valid = true;
  value->machine->cpu.linked_physical_address = target_pa + 4u;
  value->machine->cpu.linked_width = 4u;

  status = run_cache_instruction(value, CACHE_HIT_WRITEBACK_D, target_va);
  if (status != T64_STATUS_RESOURCE_EXHAUSTED ||
      !value->machine->cpu.stopped ||
      value->machine->cpu.stop_reason != T64_STOP_REASON_CPU_UNSUPPORTED ||
      memcmp(line, &before, sizeof before) != 0 ||
      memcmp(value->machine->image_storage.data, image_before,
             sizeof image_before) != 0 ||
      memcmp(&value->machine->cache, &cache_before, sizeof cache_before) != 0 ||
      !value->machine->cpu.linked_valid ||
      value->machine->cpu.linked_physical_address != target_pa + 4u ||
      value->machine->cpu.linked_width != 4u) {
    (void)puts("# mapped read-only D writeback should remain an atomic model refusal, not DBE");
    return false;
  }
  return true;
}

/* Primary CACHE expectations below come from NEC VR4300/VR4305/VR4310 User's
 * Manual, 7th ed., §16, printed p.404: op0/I invalidates by index; op0/D
 * writes any non-Invalid D line from its tag then invalidates; op1/I,D load
 * TagLo; op2/I,D store TagLo; op3/D creates a dirty-exclusive line and only
 * writes back a dirty old line on a miss; op4/I,D invalidate only a hit;
 * op5/I fills; op5/D writes dirty data then invalidates a hit; op6/I writes a
 * hit unconditionally, while op6/D writes a dirty hit only. Figure 5-18,
 * printed p.155, defines PTagLo as PA[31:12] and PState as I=10 valid,
 * D=10 clean/11 dirty, 00 invalid; 01 is undefined. */
static bool test_index_invalidate_i(fixture *value) {
  const uint32_t target_va = UINT32_C(0x80002120);
  const uint32_t index = cache_index(target_va);
  t64_vr4300_icache_line *line = &value->machine->cpu.icache[index];
  uint8_t data_before[T64_VR4300_ICACHE_LINE_BYTES];

  line->physical_tag = UINT32_C(0x12345220);
  line->valid = true;
  memset(line->data, UINT8_C(0x6d), sizeof line->data);
  memcpy(data_before, line->data, sizeof data_before);
  value->machine->cpu.linked_valid = true;
  value->machine->cpu.linked_physical_address = UINT32_C(0x12345224);
  value->machine->cpu.linked_width = UINT32_C(4);
  if (run_cache_instruction(value, CACHE_INDEX_INVALIDATE_I, target_va) !=
          T64_STATUS_OK ||
      line->valid || line->physical_tag != UINT32_C(0x12345220) ||
      memcmp(line->data, data_before, sizeof data_before) != 0 ||
      !value->machine->cpu.linked_valid ||
      value->machine->cpu.linked_physical_address != UINT32_C(0x12345224) ||
      value->machine->cpu.linked_width != UINT32_C(4) ||
      value->machine->cache.last_operation != CACHE_INDEX_INVALIDATE_I) {
    (void)puts("# p.404 op0/I expected indexed invalidation, unchanged data/tag, preserved D-link");
    return false;
  }
  return true;
}

static bool test_index_writeback_invalidate_d_writes_clean_line(
    fixture *value) {
  const uint32_t target_va = UINT32_C(0x80002008);
  const uint32_t writeback_tag = UINT32_C(0x00004000);
  const uint32_t index = dcache_index(target_va);
  t64_vr4300_dcache_line *line = &value->machine->cpu.dcache[index];
  uint8_t expected[T64_VR4300_DCACHE_LINE_BYTES];
  size_t offset;

  seed_dcache_line(value->machine, target_va, writeback_tag, true, false,
                   UINT8_C(0xa0));
  memcpy(expected, line->data, sizeof expected);
  memset(value->machine->ram + writeback_tag, UINT8_C(0x11), sizeof expected);
  value->machine->cpu.linked_valid = true;
  value->machine->cpu.linked_physical_address = writeback_tag + 4u;
  value->machine->cpu.linked_width = 4u;
  if (run_cache_instruction(value, CACHE_INDEX_WRITEBACK_INVALIDATE_D,
                            target_va) != T64_STATUS_OK ||
      line->valid || line->dirty ||
      memcmp(value->machine->ram + writeback_tag, expected, sizeof expected) !=
          0 ||
      value->machine->cpu.linked_valid ||
      value->machine->cache.last_operation !=
          CACHE_INDEX_WRITEBACK_INVALIDATE_D) {
    (void)puts("# p.404 op0/D expected clean non-Invalid tag writeback then invalidate and clear overlapping link");
    return false;
  }
  for (offset = 0u; offset < sizeof expected; offset++) {
    if (value->machine->ram[writeback_tag + (uint32_t)offset] !=
        (uint8_t)(UINT8_C(0xa0) + (uint8_t)offset)) {
      return false;
    }
  }
  return true;
}

static bool test_index_load_tag_i(fixture *value) {
  const uint32_t target_va = UINT32_C(0x80002ab4);
  const uint32_t index = cache_index(target_va);
  t64_vr4300_icache_line *line = &value->machine->cpu.icache[index];

  line->physical_tag = UINT32_C(0x00456aa0);
  line->valid = true;
  memset(line->data, UINT8_C(0x5c), sizeof line->data);
  value->machine->cpu.cp0[CP0_TAGHI] = UINT32_MAX;
  if (run_cache_instruction(value, CACHE_INDEX_LOAD_TAG_I, target_va) !=
          T64_STATUS_OK ||
      value->machine->cpu.cp0[CP0_TAGLO] != UINT32_C(0x00045680) ||
      value->machine->cpu.cp0[CP0_TAGHI] != 0u) {
    (void)printf("# p.404 op1/I expected TagLo=00045680 TagHi=0 observed=%08x/%08x\n",
                 (uint32_t)value->machine->cpu.cp0[CP0_TAGLO],
                 (uint32_t)value->machine->cpu.cp0[CP0_TAGHI]);
    return false;
  }
  return true;
}

static bool test_index_load_tag_d(fixture *value) {
  const uint32_t target_va = UINT32_C(0x80002ab4);
  const uint32_t index = dcache_index(target_va);
  t64_vr4300_dcache_line *line = &value->machine->cpu.dcache[index];

  seed_dcache_line(value->machine, target_va, UINT32_C(0x00456ab0), true,
                   true, UINT8_C(0x40));
  line = &value->machine->cpu.dcache[index];
  value->machine->cpu.cp0[CP0_TAGHI] = UINT32_MAX;
  if (run_cache_instruction(value, CACHE_INDEX_LOAD_TAG_D, target_va) !=
          T64_STATUS_OK ||
      value->machine->cpu.cp0[CP0_TAGLO] != UINT32_C(0x000456c0) ||
      value->machine->cpu.cp0[CP0_TAGHI] != 0u || !line->valid ||
      !line->dirty) {
    (void)printf("# p.404 op1/D expected TagLo=000456c0 TagHi=0 observed=%08x/%08x\n",
                 (uint32_t)value->machine->cpu.cp0[CP0_TAGLO],
                 (uint32_t)value->machine->cpu.cp0[CP0_TAGHI]);
    return false;
  }
  return true;
}

static bool test_index_store_tag_i(fixture *value) {
  const uint32_t target_va = UINT32_C(0x80002ab4);
  const uint32_t index = cache_index(target_va);
  t64_vr4300_icache_line *line = &value->machine->cpu.icache[index];
  uint8_t data_before[T64_VR4300_ICACHE_LINE_BYTES];

  line->physical_tag = UINT32_C(0x00001000);
  line->valid = false;
  memset(line->data, UINT8_C(0x72), sizeof line->data);
  memcpy(data_before, line->data, sizeof data_before);
  value->machine->cpu.cp0[CP0_TAGLO] = UINT32_C(0x01234580);
  value->machine->cpu.linked_valid = true;
  value->machine->cpu.linked_physical_address = UINT32_C(0x12345aa4);
  value->machine->cpu.linked_width = 4u;
  if (run_cache_instruction(value, CACHE_INDEX_STORE_TAG_I, target_va) !=
          T64_STATUS_OK ||
      !line->valid || line->physical_tag != UINT32_C(0x12345aa0) ||
      memcmp(line->data, data_before, sizeof data_before) != 0 ||
      value->machine->cpu.cp0[CP0_TAGLO] != UINT32_C(0x01234580) ||
      !value->machine->cpu.linked_valid) {
    (void)puts("# p.404 op2/I expected PTagLo plus VA[11:5] key, valid state, unchanged bytes and link");
    return false;
  }
  return true;
}

static bool test_index_store_tag_d(fixture *value) {
  const uint32_t target_va = UINT32_C(0x80002ab4);
  const uint32_t index = dcache_index(target_va);
  t64_vr4300_dcache_line *line = &value->machine->cpu.dcache[index];
  uint8_t data_before[T64_VR4300_DCACHE_LINE_BYTES];

  seed_dcache_line(value->machine, target_va, UINT32_C(0x00001000), false,
                   false, UINT8_C(0x73));
  line = &value->machine->cpu.dcache[index];
  memcpy(data_before, line->data, sizeof data_before);
  value->machine->cpu.cp0[CP0_TAGLO] = UINT32_C(0x012345c0);
  if (run_cache_instruction(value, CACHE_INDEX_STORE_TAG_D, target_va) !=
          T64_STATUS_OK ||
      !line->valid || !line->dirty ||
      line->physical_tag != UINT32_C(0x12345ab0) ||
      memcmp(line->data, data_before, sizeof data_before) != 0) {
    (void)puts("# p.404 op2/D expected PTagLo plus VA[11:4] key, dirty state and unchanged bytes");
    return false;
  }
  return true;
}

static bool test_create_dirty_exclusive_d_writes_old_dirty_line(
    fixture *value) {
  const uint32_t target_va = UINT32_C(0x80002000);
  const uint32_t index = dcache_index(target_va);
  t64_vr4300_dcache_line *line = &value->machine->cpu.dcache[index];
  uint8_t old_data[T64_VR4300_DCACHE_LINE_BYTES];

  seed_dcache_line(value->machine, target_va, UINT32_C(0x00004000), true,
                   true, UINT8_C(0xb0));
  memcpy(old_data, line->data, sizeof old_data);
  memset(value->machine->ram + UINT32_C(0x4000), UINT8_C(0x21),
         sizeof old_data);
  if (run_cache_instruction(value, CACHE_CREATE_DIRTY_EXCLUSIVE_D,
                            target_va) != T64_STATUS_OK ||
      !line->valid || !line->dirty ||
      line->physical_tag != UINT32_C(0x00002000) ||
      memcmp(line->data, old_data, sizeof old_data) != 0 ||
      memcmp(value->machine->ram + UINT32_C(0x4000), old_data,
             sizeof old_data) != 0) {
    (void)puts("# p.404 op3/D expected dirty-old writeback before tag replacement without a data fill");
    return false;
  }
  return true;
}

static bool test_hit_invalidate_d_exact_link(fixture *value) {
  const uint32_t target_va = UINT32_C(0x80002004);
  const uint32_t index = dcache_index(target_va);
  t64_vr4300_dcache_line *line = &value->machine->cpu.dcache[index];
  uint8_t old_backing[T64_VR4300_DCACHE_LINE_BYTES];

  memset(value->machine->ram + UINT32_C(0x2000), UINT8_C(0x11),
         sizeof old_backing);
  memcpy(old_backing, value->machine->ram + UINT32_C(0x2000),
         sizeof old_backing);
  seed_dcache_line(value->machine, target_va, UINT32_C(0x00002000), true,
                   true, UINT8_C(0xc0));
  value->machine->cpu.linked_valid = true;
  value->machine->cpu.linked_physical_address = UINT32_C(0x2004);
  value->machine->cpu.linked_width = 4u;
  if (run_cache_instruction(value, CACHE_HIT_INVALIDATE_D, target_va) !=
          T64_STATUS_OK ||
      line->valid || line->dirty ||
      memcmp(value->machine->ram + UINT32_C(0x2000), old_backing,
             sizeof old_backing) != 0 ||
      value->machine->cpu.linked_valid) {
    (void)puts("# p.404 op4/D expected hit invalidation without writeback and exact overlapping-link invalidation");
    return false;
  }
  return true;
}

static bool test_fill_i_refills_from_backing(fixture *value) {
  const uint32_t target_va = UINT32_C(0x80001000);
  const uint32_t target_pa = UINT32_C(0x00001000);
  const uint32_t index = cache_index(target_va);
  t64_vr4300_icache_line *line = &value->machine->cpu.icache[index];
  uint32_t ignored_opcode = 0u;

  write_ram_instruction(value->machine, target_pa, UINT32_C(0x24080001));
  if (t64_cpu_icache_fetch(value->machine, target_va, target_pa,
                           &ignored_opcode) != T64_STATUS_OK) {
    return false;
  }
  write_ram_instruction(value->machine, target_pa, UINT32_C(0x24080022));
  if (run_cache_instruction(value, CACHE_FILL_I, target_va) !=
          T64_STATUS_OK ||
      !line->valid || line->physical_tag != target_pa ||
      read_be32(line->data) != UINT32_C(0x24080022) ||
      read_be32(value->machine->ram + target_pa) != UINT32_C(0x24080022)) {
    (void)puts("# p.404 op5/I expected a staged backing refill of the selected instruction line");
    return false;
  }
  return true;
}

static bool test_hit_writeback_invalidate_d_dirty_line(fixture *value) {
  const uint32_t target_va = UINT32_C(0x80002000);
  const uint32_t index = dcache_index(target_va);
  t64_vr4300_dcache_line *line = &value->machine->cpu.dcache[index];
  uint8_t expected[T64_VR4300_DCACHE_LINE_BYTES];

  seed_dcache_line(value->machine, target_va, UINT32_C(0x00002000), true,
                   true, UINT8_C(0xd0));
  memcpy(expected, line->data, sizeof expected);
  value->machine->cpu.linked_valid = true;
  value->machine->cpu.linked_physical_address = UINT32_C(0x2000);
  value->machine->cpu.linked_width = 8u;
  if (run_cache_instruction(value, CACHE_HIT_WRITEBACK_INVALIDATE_D,
                            target_va) != T64_STATUS_OK ||
      line->valid || line->dirty ||
      memcmp(value->machine->ram + UINT32_C(0x2000), expected,
             sizeof expected) != 0 ||
      value->machine->cpu.linked_valid) {
    (void)puts("# p.404 op5/D expected dirty hit writeback, invalid state, and overlapping-link invalidation");
    return false;
  }
  return true;
}

static bool test_hit_writeback_d_clean_line_does_not_write(
    fixture *value) {
  const uint32_t target_va = UINT32_C(0x80002000);
  const uint32_t index = dcache_index(target_va);
  t64_vr4300_dcache_line *line = &value->machine->cpu.dcache[index];
  uint8_t old_backing[T64_VR4300_DCACHE_LINE_BYTES];

  memset(value->machine->ram + UINT32_C(0x2000), UINT8_C(0x12),
         sizeof old_backing);
  memcpy(old_backing, value->machine->ram + UINT32_C(0x2000),
         sizeof old_backing);
  seed_dcache_line(value->machine, target_va, UINT32_C(0x00002000), true,
                   false, UINT8_C(0xe0));
  if (run_cache_instruction(value, CACHE_HIT_WRITEBACK_D, target_va) !=
          T64_STATUS_OK ||
      !line->valid || line->dirty ||
      memcmp(value->machine->ram + UINT32_C(0x2000), old_backing,
             sizeof old_backing) != 0) {
    (void)puts("# p.404 op6/D expected clean hit to remain unchanged without backing write");
    return false;
  }
  return true;
}

static bool test_undefined_tag_state_is_atomic(fixture *value) {
  const uint32_t target_va = UINT32_C(0x80002ab4);
  const uint32_t index = dcache_index(target_va);
  t64_vr4300_dcache_line before;
  t64_cache_state cache_before;

  seed_dcache_line(value->machine, target_va, UINT32_C(0x00001000), true,
                   true, UINT8_C(0x33));
  before = value->machine->cpu.dcache[index];
  value->machine->cpu.cp0[CP0_TAGLO] = UINT32_C(0x01234540);
  value->machine->cache.last_operation = UINT32_C(0x1111);
  value->machine->cache.last_pc = UINT32_C(0x2222);
  value->machine->cache.last_address = UINT32_C(0x3333);
  value->machine->cache.operation_seen = true;
  cache_before = value->machine->cache;
  if (run_cache_instruction(value, CACHE_INDEX_STORE_TAG_D, target_va) !=
          T64_STATUS_RESOURCE_EXHAUSTED ||
      memcmp(&value->machine->cpu.dcache[index], &before, sizeof before) != 0 ||
      memcmp(&value->machine->cache, &cache_before, sizeof cache_before) != 0 ||
      value->machine->cpu.cp0[CP0_TAGLO] != UINT32_C(0x01234540)) {
    (void)puts("# p.155 PState=01 expected unsupported refusal before D-line, TagLo, or diagnostics mutation");
    return false;
  }
  return true;
}

static bool test_cache_unmapped_writeback_raises_dbe_atomically(
    fixture *value) {
  const uint32_t code_pa = UINT32_C(0x00003000);
  const uint32_t code_va = UINT32_C(0x80003000);
  const uint32_t target_va = UINT32_C(0x80002000);
  const uint32_t index = dcache_index(target_va);
  const uint32_t taglo = UINT32_C(0x004900c0); /* PA 0x04900000, dirty. */
  const uint32_t instructions[] = {
      UINT32_C(0x408ae000), /* mtc0 t2,c0_taglo */
      UINT32_C(0xbc000000) | (UINT32_C(5) << 21) |
          (CACHE_INDEX_STORE_TAG_D << 16),
      UINT32_C(0xbc000000) | (UINT32_C(5) << 21) |
          (CACHE_INDEX_WRITEBACK_INVALIDATE_D << 16),
  };
  t64_vr4300_dcache_line *line = &value->machine->cpu.dcache[index];
  t64_vr4300_dcache_line before;
  t64_cache_state cache_before;
  uint8_t backing_before[T64_VR4300_DCACHE_LINE_BYTES];
  t64_status status;
  size_t instruction;

  if (t64_bus_probe_access(value->machine, UINT32_C(0x04900000), 1u) !=
      T64_STATUS_BUS_UNMAPPED) {
    (void)puts("# expected guest TagLo target 0x04900000 to decode as BUS_UNMAPPED");
    return false;
  }
  for (instruction = 0u; instruction < sizeof instructions /
                                            sizeof instructions[0];
       instruction++) {
    write_ram_instruction(value->machine,
                          code_pa + (uint32_t)(instruction * 4u),
                          instructions[instruction]);
  }
  seed_dcache_line(value->machine, target_va, UINT32_C(0x00002000), false,
                   false, UINT8_C(0x80));
  line = &value->machine->cpu.dcache[index];
  memcpy(backing_before, value->machine->ram + UINT32_C(0x2000),
         sizeof backing_before);
  value->machine->cpu.gpr[5] = t64_cpu_canonical_address(target_va);
  value->machine->cpu.gpr[10] = taglo;
  value->machine->cpu.linked_valid = true;
  value->machine->cpu.linked_physical_address = UINT32_C(0x04900004);
  value->machine->cpu.linked_width = 4u;
  set_pc(value->machine, code_va);

  if (t64_cpu_step(value->machine, NULL) != T64_STATUS_OK ||
      t64_cpu_step(value->machine, NULL) != T64_STATUS_OK ||
      !line->valid || !line->dirty ||
      line->physical_tag != UINT32_C(0x04900000)) {
    (void)puts("# guest MTC0/Index_Store_Tag D did not create the dirty unmapped tag");
    return false;
  }
  before = *line;
  cache_before = value->machine->cache;
  status = t64_cpu_step(value->machine, NULL);
  if (status != T64_STATUS_OK ||
      ((uint32_t)value->machine->cpu.cp0[CP0_CAUSE] >> 2 & UINT32_C(0x1f)) !=
          UINT32_C(7) ||
      (value->machine->cpu.cp0[CP0_CAUSE] & UINT32_C(0x80000000)) != 0u ||
      value->machine->cpu.cp0[14] !=
          t64_cpu_canonical_address(code_va + UINT32_C(8)) ||
      value->machine->cpu.pc_full !=
          t64_cpu_canonical_address(UINT32_C(0x80000180)) ||
      value->machine->cpu.retired_instructions != UINT64_C(2) ||
      memcmp(line, &before, sizeof before) != 0 ||
      memcmp(&value->machine->cache, &cache_before, sizeof cache_before) != 0 ||
      memcmp(value->machine->ram + UINT32_C(0x2000), backing_before,
             sizeof backing_before) != 0 ||
      !value->machine->cpu.linked_valid ||
      value->machine->cpu.linked_physical_address != UINT32_C(0x04900004) ||
      value->machine->cpu.linked_width != 4u) {
    (void)printf("# p.405 CACHE Bus error expected DBE/ExcCode7 at EPC=%016llx, no retirement or line/link/backing effects; observed status=%u cause=%08x epc=%016llx pc=%016llx retired=%llu line=%u/%u/%08x link=%u/%08x/%u op=%u\n",
                 (unsigned long long)t64_cpu_canonical_address(
                     code_va + UINT32_C(8)),
                 (unsigned)status, (uint32_t)value->machine->cpu.cp0[CP0_CAUSE],
                 (unsigned long long)value->machine->cpu.cp0[14],
                 (unsigned long long)value->machine->cpu.pc_full,
                 (unsigned long long)value->machine->cpu.retired_instructions,
                 line->valid ? 1u : 0u, line->dirty ? 1u : 0u,
                 line->physical_tag,
                 value->machine->cpu.linked_valid ? 1u : 0u,
                 value->machine->cpu.linked_physical_address,
                 value->machine->cpu.linked_width,
                 value->machine->cache.last_operation);
    return false;
  }
  return true;
}

static bool test_cached_unmapped_eviction_raises_dbe_atomically(
    fixture *value, bool store) {
  const uint32_t code_pa = UINT32_C(0x00003000);
  const uint32_t code_va = UINT32_C(0x80003000);
  const uint32_t dirty_va = UINT32_C(0x80002000);
  const uint32_t access_va = UINT32_C(0x80004000);
  const uint32_t index = dcache_index(dirty_va);
  const uint32_t taglo = UINT32_C(0x004900c0); /* PA 0x04900000, dirty. */
  const uint32_t memory_opcode = store ? UINT32_C(0xacc70000)
                                       : UINT32_C(0x8cc80000);
  const uint32_t instructions[] = {
      UINT32_C(0x408ae000), /* mtc0 t2,c0_taglo */
      UINT32_C(0xbc000000) | (UINT32_C(5) << 21) |
          (CACHE_INDEX_STORE_TAG_D << 16),
      memory_opcode,
  };
  t64_vr4300_dcache_line *line = &value->machine->cpu.dcache[index];
  t64_vr4300_dcache_line before;
  t64_cache_state cache_before;
  uint8_t backing_before[T64_VR4300_DCACHE_LINE_BYTES];
  uint64_t destination_before = UINT64_C(0xfeedfacecafebeef);
  t64_status status;
  size_t instruction;

  if (t64_bus_probe_access(value->machine, UINT32_C(0x04900000), 1u) !=
      T64_STATUS_BUS_UNMAPPED) {
    (void)puts("# expected guest TagLo target 0x04900000 to decode as BUS_UNMAPPED");
    return false;
  }
  for (instruction = 0u; instruction < sizeof instructions /
                                            sizeof instructions[0];
       instruction++) {
    write_ram_instruction(value->machine,
                          code_pa + (uint32_t)(instruction * 4u),
                          instructions[instruction]);
  }
  memset(value->machine->ram + UINT32_C(0x4000), UINT8_C(0x36),
         sizeof backing_before);
  memcpy(backing_before, value->machine->ram + UINT32_C(0x4000),
         sizeof backing_before);
  seed_dcache_line(value->machine, dirty_va, UINT32_C(0x00002000), false,
                   false, UINT8_C(0x80));
  line = &value->machine->cpu.dcache[index];
  value->machine->cpu.gpr[5] = t64_cpu_canonical_address(dirty_va);
  value->machine->cpu.gpr[6] = t64_cpu_canonical_address(access_va);
  value->machine->cpu.gpr[7] = UINT64_C(0x12345678);
  value->machine->cpu.gpr[8] = destination_before;
  value->machine->cpu.gpr[10] = taglo;
  value->machine->cpu.linked_valid = true;
  value->machine->cpu.linked_physical_address = UINT32_C(0x04900004);
  value->machine->cpu.linked_width = 4u;
  set_pc(value->machine, code_va);

  if (t64_cpu_step(value->machine, NULL) != T64_STATUS_OK ||
      t64_cpu_step(value->machine, NULL) != T64_STATUS_OK ||
      !line->valid || !line->dirty ||
      line->physical_tag != UINT32_C(0x04900000)) {
    (void)puts("# guest MTC0/Index_Store_Tag D did not create the dirty unmapped tag");
    return false;
  }
  before = *line;
  cache_before = value->machine->cache;
  status = t64_cpu_step(value->machine, NULL);
  if (status != T64_STATUS_OK ||
      ((uint32_t)value->machine->cpu.cp0[CP0_CAUSE] >> 2 & UINT32_C(0x1f)) !=
          UINT32_C(7) ||
      (value->machine->cpu.cp0[CP0_CAUSE] & UINT32_C(0x80000000)) != 0u ||
      value->machine->cpu.cp0[14] !=
          t64_cpu_canonical_address(code_va + UINT32_C(8)) ||
      value->machine->cpu.pc_full !=
          t64_cpu_canonical_address(UINT32_C(0x80000180)) ||
      value->machine->cpu.retired_instructions != UINT64_C(2) ||
      memcmp(line, &before, sizeof before) != 0 ||
      memcmp(&value->machine->cache, &cache_before, sizeof cache_before) != 0 ||
      memcmp(value->machine->ram + UINT32_C(0x4000), backing_before,
             sizeof backing_before) != 0 ||
      (!store && value->machine->cpu.gpr[8] != destination_before) ||
      !value->machine->cpu.linked_valid ||
      value->machine->cpu.linked_physical_address != UINT32_C(0x04900004) ||
      value->machine->cpu.linked_width != 4u) {
    (void)printf("# p.405 Bus error expected cached %s eviction to raise precise DBE without retirement or line/link/backing effects; observed status=%u cause=%08x epc=%016llx pc=%016llx retired=%llu line=%u/%u/%08x link=%u/%08x/%u\n",
                 store ? "store" : "load", (unsigned)status,
                 (uint32_t)value->machine->cpu.cp0[CP0_CAUSE],
                 (unsigned long long)value->machine->cpu.cp0[14],
                 (unsigned long long)value->machine->cpu.pc_full,
                 (unsigned long long)value->machine->cpu.retired_instructions,
                 line->valid, line->dirty, line->physical_tag,
                 value->machine->cpu.linked_valid,
                 value->machine->cpu.linked_physical_address,
                 value->machine->cpu.linked_width);
    return false;
  }
  return true;
}

static bool test_cached_load_unmapped_eviction_raises_dbe(fixture *value) {
  return test_cached_unmapped_eviction_raises_dbe_atomically(value, false);
}

static bool test_cached_store_unmapped_eviction_raises_dbe(fixture *value) {
  return test_cached_unmapped_eviction_raises_dbe_atomically(value, true);
}

static bool test_cache_tlb_fault_precedes_index_operation(fixture *value) {
  const uint32_t target_va = UINT32_C(0x00400000);
  const uint32_t index = cache_index(target_va);
  uint64_t taglo_before = UINT64_C(0x01234580);
  uint32_t exception_code;

  set_tlb_pair(value->machine, 0u, UINT64_C(0x00400000),
               UINT64_C(0x00002000), 3u);
  value->machine->cpu.tlb[0].entry_lo0 &= ~ENTRYLO_VALID;
  value->machine->cpu.cp0[CP0_STATUS] = 0u;
  value->machine->cpu.cp0[CP0_TAGLO] = taglo_before;
  value->machine->cpu.icache[index].valid = true;
  value->machine->cpu.icache[index].physical_tag = UINT32_C(0x2000);
  write_ram_instruction(value->machine, UINT32_C(0x3000),
                        cache_instruction(CACHE_INDEX_LOAD_TAG_I, 5u));
  value->machine->cpu.gpr[5] = target_va;
  set_pc(value->machine, UINT32_C(0x80003000));
  if (t64_cpu_step(value->machine, NULL) != T64_STATUS_OK) {
    return false;
  }
  exception_code = ((uint32_t)value->machine->cpu.cp0[CP0_CAUSE] >> 2) &
                   UINT32_C(0x1f);
  if (exception_code != UINT32_C(2) ||
      value->machine->cpu.pc_full !=
          t64_cpu_canonical_address(UINT32_C(0x80000180)) ||
      value->machine->cpu.cp0[8] != UINT64_C(0x00400000) ||
      value->machine->cpu.cp0[CP0_TAGLO] != taglo_before ||
      !value->machine->cpu.icache[index].valid ||
      value->machine->cpu.icache[index].physical_tag != UINT32_C(0x2000) ||
      value->machine->cache.operation_seen) {
    (void)printf("# mapped Index_Load_Tag TLB-invalid priority expected ExcCode=2/general-vector, unchanged TagLo/line; observed cause=%08x pc=%016llx badvaddr=%016llx status=%08x vpn2=%016llx asid=%u entrylo0=%08llx TagLo=%08x\n",
                 (uint32_t)value->machine->cpu.cp0[CP0_CAUSE],
                 (unsigned long long)value->machine->cpu.pc_full,
                 (unsigned long long)value->machine->cpu.cp0[8],
                 (uint32_t)value->machine->cpu.cp0[CP0_STATUS],
                 (unsigned long long)value->machine->cpu.tlb[0].vpn2,
                 value->machine->cpu.tlb[0].asid,
                 (unsigned long long)value->machine->cpu.tlb[0].entry_lo0,
                 (uint32_t)value->machine->cpu.cp0[CP0_TAGLO]);
    return false;
  }
  return true;
}

static bool test_dcache_index_511_wrap_and_dirty_eviction(fixture *value) {
  const uint64_t va_511 = UINT64_C(0x80003ff0);
  const uint64_t va_0 = UINT64_C(0x80004000);
  const uint64_t va_511_alias = UINT64_C(0x80005ff0);
  const uint32_t pa_511 = UINT32_C(0x00003ff0);
  const uint32_t pa_0 = UINT32_C(0x00004000);
  const uint32_t pa_alias = UINT32_C(0x00005ff0);
  const uint32_t index_511 = dcache_index((uint32_t)va_511);
  const uint32_t index_0 = dcache_index((uint32_t)va_0);
  uint64_t observed = 0u;
  t64_vr4300_dcache_line *line;

  if (index_511 != T64_VR4300_DCACHE_LINE_COUNT - 1u || index_0 != 0u) {
    return false;
  }
  write_be32(value->machine->ram + pa_511, UINT32_C(0x11223344));
  write_be32(value->machine->ram + pa_0, UINT32_C(0x55667788));
  write_be32(value->machine->ram + pa_alias, UINT32_C(0xaabbccdd));
  if (t64_cpu_dcache_read(value->machine, va_511, pa_511, 4u, &observed) !=
          T64_STATUS_OK ||
      observed != UINT64_C(0x11223344)) {
    return false;
  }
  line = &value->machine->cpu.dcache[index_511];
  if (!line->valid || line->dirty || line->physical_tag != pa_511) {
    return false;
  }
  if (t64_cpu_dcache_write(value->machine, va_511, pa_511, 4u,
                           UINT8_C(0x0f), UINT32_C(0xdeadbeef)) !=
          T64_STATUS_OK ||
      !line->dirty || read_be32(value->machine->ram + pa_511) !=
                          UINT32_C(0x11223344)) {
    return false;
  }
  if (t64_cpu_dcache_read(value->machine, va_0, pa_0, 4u, &observed) !=
          T64_STATUS_OK ||
      observed != UINT64_C(0x55667788) ||
      !value->machine->cpu.dcache[index_0].valid ||
      value->machine->cpu.dcache[index_0].physical_tag != pa_0) {
    return false;
  }
  if (t64_cpu_dcache_read(value->machine, va_511_alias, pa_alias, 4u,
                          &observed) != T64_STATUS_OK ||
      observed != UINT64_C(0xaabbccdd) || !line->valid || line->dirty ||
      line->physical_tag != pa_alias ||
      read_be32(value->machine->ram + pa_511) != UINT32_C(0xdeadbeef)) {
    (void)printf("# D-cache index511 replacement expected old tag=%08x writeback=deadbeef new tag=%08x; observed old RAM=%08x tag=%08x dirty=%u\n",
                 pa_511, pa_alias,
                 read_be32(value->machine->ram + pa_511), line->physical_tag,
                 line->dirty ? 1u : 0u);
    return false;
  }
  return true;
}

static bool test_dcache_masked_store_and_exact_link_bytes(fixture *value) {
  const uint64_t virtual_address = UINT64_C(0x80002000);
  const uint32_t physical_address = UINT32_C(0x00002000);
  const uint32_t index = dcache_index((uint32_t)virtual_address);
  t64_vr4300_dcache_line *line = &value->machine->cpu.dcache[index];
  uint64_t observed = 0u;

  write_be32(value->machine->ram + physical_address, UINT32_C(0x10111213));
  if (t64_cpu_dcache_read(value->machine, virtual_address, physical_address,
                          4u, &observed) != T64_STATUS_OK ||
      observed != UINT64_C(0x10111213)) {
    return false;
  }
  value->machine->cpu.linked_valid = true;
  value->machine->cpu.linked_physical_address = physical_address + 1u;
  value->machine->cpu.linked_width = 1u;
  if (t64_cpu_dcache_write(value->machine, virtual_address, physical_address,
                           4u, UINT8_C(0x05), UINT32_C(0xaabbccdd)) !=
          T64_STATUS_OK ||
      !line->dirty || line->data[0] != UINT8_C(0xaa) ||
      line->data[1] != UINT8_C(0x11) || line->data[2] != UINT8_C(0xcc) ||
      line->data[3] != UINT8_C(0x13) ||
      read_be32(value->machine->ram + physical_address) !=
          UINT32_C(0x10111213) ||
      !value->machine->cpu.linked_valid) {
    (void)puts("# masked cached store expected bytes 0/2 only, stale backing and preserved link on byte1");
    return false;
  }
  value->machine->cpu.linked_physical_address = physical_address + 2u;
  value->machine->cpu.linked_width = 1u;
  if (t64_cpu_dcache_write(value->machine, virtual_address, physical_address,
                           4u, UINT8_C(0x04), UINT32_C(0x11223344)) !=
          T64_STATUS_OK ||
      value->machine->cpu.linked_valid || line->data[2] != UINT8_C(0x33) ||
      read_be32(value->machine->ram + physical_address) !=
          UINT32_C(0x10111213)) {
    (void)puts("# masked cached store expected selected byte2 to clear only its LL interval and remain write-back");
    return false;
  }
  return true;
}

static bool test_cached_reverse_endian_store_masks_and_widths(
    fixture *value) {
  const uint32_t virtual_code = UINT32_C(0x00000000);
  const uint32_t virtual_data = UINT32_C(0x00001000);
  const uint32_t physical_code = UINT32_C(0x00001000);
  const uint32_t physical_data = UINT32_C(0x00002000);
  const uint32_t index = dcache_index(virtual_data);
  const uint32_t status_user_re_ux = UINT32_C(0x02000030);
  const uint8_t initial[T64_VR4300_DCACHE_LINE_BYTES] = {
      0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
      0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xf0, 0x0f};
  const struct {
    uint32_t operation;
    uint64_t source;
    const uint8_t expected_first_eight[8];
    const char *name;
  } stores[] = {
      {0x28u, UINT64_C(0x18),
       {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x18}, "SB"},
      {0x29u, UINT64_C(0x0718),
       {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x07, 0x18}, "SH"},
      {0x2bu, UINT64_C(0xe5f60718),
       {0x11, 0x22, 0x33, 0x44, 0xe5, 0xf6, 0x07, 0x18}, "SW"},
      {0x3fu, UINT64_C(0xa1b2c3d4e5f60718),
       {0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0x07, 0x18}, "SD"},
  };
  t64_machine *machine = value->machine;
  t64_vr4300_dcache_line *line = &machine->cpu.dcache[index];
  size_t store_index;

  machine->cpu.cp0[CP0_ENTRYHI] = UINT64_C(1);
  set_tlb_pair(machine, 0u, UINT64_C(0), UINT64_C(0x1000), UINT32_C(2));
  /* User code fetch is uncached; VA 0x1000 data maps cacheably to PA 0x2000. */
  machine->cpu.tlb[0].entry_lo1 = entry_lo(UINT64_C(0x2000), UINT32_C(3));
  machine->cpu.cp0[CP0_STATUS] = status_user_re_ux;
  (void)memcpy(machine->ram + physical_data, initial, sizeof initial);

  for (store_index = 0u;
       store_index < sizeof stores / sizeof stores[0]; store_index++) {
    uint32_t opcode = (stores[store_index].operation << 26) |
                      (UINT32_C(4) << 21) | (UINT32_C(8) << 16);
    t64_status result;

    write_ram_instruction(machine, physical_code, opcode);
    machine->cpu.gpr[4] = virtual_data;
    machine->cpu.gpr[8] = stores[store_index].source;
    machine->cpu.pc = virtual_code;
    machine->cpu.pc_full = virtual_code;
    result = t64_cpu_step(machine, NULL);
    if (result != T64_STATUS_OK || !line->valid || !line->dirty ||
        line->physical_tag != physical_data ||
        memcmp(line->data, stores[store_index].expected_first_eight, 8u) != 0 ||
        memcmp(machine->ram + physical_data, initial, sizeof initial) != 0) {
      (void)printf("# manual RE cached %s expected selected physical lanes, dirty PA=%08x and unchanged backing; observed status=%u valid=%u dirty=%u tag=%08x bytes=%02x%02x%02x%02x%02x%02x%02x%02x\n",
                   stores[store_index].name, physical_data, (unsigned)result,
                   line->valid, line->dirty, line->physical_tag,
                   line->data[0], line->data[1], line->data[2], line->data[3],
                   line->data[4], line->data[5], line->data[6], line->data[7]);
      return false;
    }
  }
  return true;
}

static bool test_dcache_refused_fill_and_writeback_are_atomic(
    fixture *value) {
  const uint64_t virtual_address = UINT64_C(0x80002000);
  const uint32_t index = dcache_index((uint32_t)virtual_address);
  t64_vr4300_dcache_line *line = &value->machine->cpu.dcache[index];
  t64_vr4300_dcache_line before;
  uint64_t observed = UINT64_C(0xfeedface);
  t64_status status;

  seed_dcache_line(value->machine, (uint32_t)virtual_address,
                   UINT32_C(0x10000000), true, true, UINT8_C(0x91));
  before = *line;
  memset(value->machine->ram + UINT32_C(0x4000), UINT8_C(0x37),
         T64_VR4300_DCACHE_LINE_BYTES);
  status = t64_cpu_dcache_read(value->machine, virtual_address,
                               UINT32_C(0x00004000), 4u, &observed);
  if (status != T64_STATUS_BUS_DEVICE_REFUSED ||
      memcmp(line, &before, sizeof before) != 0 ||
      read_be32(value->machine->ram + UINT32_C(0x4000)) !=
          UINT32_C(0x37373737) ||
      observed != UINT64_C(0xfeedface)) {
    (void)puts("# conflicting read expected read-only old dirty tag refusal before line replacement or backing change");
    return false;
  }
  status = t64_cpu_dcache_read(value->machine, virtual_address,
                               UINT32_C(0x10001000), 4u, &observed);
  if (status != T64_STATUS_BUS_RANGE ||
      memcmp(line, &before, sizeof before) != 0) {
    (void)puts("# out-of-image fill expected range refusal before D-line replacement");
    return false;
  }
  status = t64_cpu_dcache_write(value->machine, virtual_address,
                                UINT32_C(0x10000000), 4u, UINT8_C(0x0f),
                                UINT32_C(0x11223344));
  if (status != T64_STATUS_BUS_DEVICE_REFUSED ||
      memcmp(line, &before, sizeof before) != 0) {
    (void)puts("# cached ROM store expected refusal before dirty/tag/data mutation");
    return false;
  }
  return true;
}

static bool test_dcache_writeback_all_accepted_mutable_domains(
    fixture *value) {
  static const uint32_t physical_addresses[] = {
      UINT32_C(0x00002000), UINT32_C(0x04000ff0), UINT32_C(0x04001ff0)};
  static const uint32_t words[] = {
      UINT32_C(0xa1b2c3d4), UINT32_C(0x11223344), UINT32_C(0x55667788)};
  static const char *const domain_names[] = {"RDRAM", "SP DMEM", "SP IMEM"};
  size_t entry;

  for (entry = 0u; entry < sizeof physical_addresses /
                                 sizeof physical_addresses[0];
       entry++) {
    const uint32_t physical_address = physical_addresses[entry];
    const uint64_t virtual_address =
        t64_cpu_canonical_address(UINT32_C(0x80000000) |
                                  physical_address);
    uint64_t observed = 0u;
    uint8_t *backing;

    if (physical_address < UINT32_C(0x04000000)) {
      backing = value->machine->ram + physical_address;
    } else if (physical_address < UINT32_C(0x04001000)) {
      backing = value->machine->sp_dmem +
                (physical_address - UINT32_C(0x04000000));
    } else {
      backing = value->machine->sp_imem +
                (physical_address - UINT32_C(0x04001000));
    }
    write_be32(backing, UINT32_C(0x01020304));
    if (t64_cpu_dcache_read(value->machine, virtual_address,
                            physical_address, 4u, &observed) !=
            T64_STATUS_OK ||
        observed != UINT64_C(0x01020304) ||
        t64_cpu_dcache_write(value->machine, virtual_address,
                             physical_address, 4u, UINT8_C(0x0f),
                             words[entry]) != T64_STATUS_OK ||
        read_be32(backing) != UINT32_C(0x01020304) ||
        t64_cpu_dcache_hit_writeback(value->machine, virtual_address,
                                     physical_address) != T64_STATUS_OK ||
        read_be32(backing) != words[entry]) {
      (void)printf("# %s full-line writeback failed at %08x\n",
                   domain_names[entry], physical_address);
      return false;
    }
  }
  return true;
}

static bool test_hit_invalidate_does_not_clear_link(
    fixture *value) {
  const uint32_t target_index = cache_index(UINT32_C(0x80001000));
  t64_machine *machine = value->machine;
  uint32_t ignored_opcode = 0u;

  write_ram_instruction(machine, UINT32_C(0x1000), UINT32_C(0x24080001));
  write_ram_instruction(machine, UINT32_C(0x2000),
                        cache_instruction(CACHE_HIT_INVALIDATE_I, 5u));
  write_ram_instruction(machine, UINT32_C(0x2004),
                        cache_instruction(CACHE_HIT_INVALIDATE_I, 5u));
  set_pc(machine, UINT32_C(0x80001000));
  if (t64_cpu_step(machine, NULL) != T64_STATUS_OK ||
      !machine->cpu.icache[target_index].valid) {
    (void)puts("# cache target line was not populated before invalidation");
    return false;
  }
  machine->cpu.linked_valid = true;
  machine->cpu.linked_physical_address = UINT32_C(0x1000);
  machine->cpu.linked_width = UINT32_C(4);
  machine->cpu.gpr[5] = t64_cpu_canonical_address(UINT32_C(0x80001000));
  set_pc(machine, UINT32_C(0x80002000));
  if (t64_cpu_step(machine, NULL) != T64_STATUS_OK ||
      machine->cpu.icache[target_index].valid ||
      !machine->cpu.linked_valid ||
      machine->cpu.linked_physical_address != UINT32_C(0x1000) ||
      machine->cpu.linked_width != UINT32_C(4) ||
      machine->cache.last_operation != CACHE_HIT_INVALIDATE_I) {
    (void)puts("# I-cache invalidation changed the D-link or missed its target");
    return false;
  }

  if (t64_cpu_icache_fetch(machine, UINT32_C(0x80001000), UINT32_C(0x1000),
                           &ignored_opcode) != T64_STATUS_OK) {
    (void)puts("# failed to restore target line for mismatched-tag invalidation");
    return false;
  }
  machine->cpu.gpr[5] = t64_cpu_canonical_address(UINT32_C(0x80005000));
  set_pc(machine, UINT32_C(0x80002004));
  if (cache_index(UINT32_C(0x80005000)) != target_index ||
      t64_cpu_step(machine, NULL) != T64_STATUS_OK ||
      !machine->cpu.icache[target_index].valid ||
      machine->cpu.icache[target_index].physical_tag != UINT32_C(0x1000) ||
      !machine->cpu.linked_valid) {
    (void)puts("# mismatched physical tag at the same index invalidated a line");
    return false;
  }
  return true;
}

static bool test_uncached_and_undefined_cache_refusals(
    fixture *value) {
  const uint32_t target_index = cache_index(UINT32_C(0x80001000));
  const uint32_t cache_pc = UINT32_C(0x80002000);
  const uint32_t cache_opcode =
      cache_instruction(CACHE_HIT_INVALIDATE_I, 5u);
  t64_machine *machine = value->machine;
  uint64_t taglo_before;
  t64_cache_state cache_before;
  t64_vr4300_icache_line target_before;

  write_ram_instruction(machine, UINT32_C(0x1000), UINT32_C(0x24080001));
  write_ram_instruction(machine, UINT32_C(0x2000), cache_opcode);
  set_pc(machine, UINT32_C(0x80001000));
  if (t64_cpu_step(machine, NULL) != T64_STATUS_OK) {
    return false;
  }
  target_before = machine->cpu.icache[target_index];
  machine->cache.last_operation = UINT32_C(0x1234);
  machine->cache.last_pc = UINT32_C(0x5678);
  machine->cache.last_address = UINT32_C(0x9abc);
  machine->cache.operation_seen = false;
  cache_before = machine->cache;
  taglo_before = machine->cpu.cp0[CP0_TAGLO];
  machine->cpu.gpr[5] =
      t64_cpu_canonical_address(UINT32_C(0xa0001000));
  set_pc(machine, cache_pc);
  if (t64_cpu_step(machine, NULL) != T64_STATUS_RESOURCE_EXHAUSTED ||
      !machine->cpu.stopped || machine->cpu.stop_opcode != cache_opcode ||
      machine->cpu.stop_pc != cache_pc ||
      memcmp(&machine->cache, &cache_before, sizeof cache_before) != 0 ||
      machine->cpu.cp0[CP0_TAGLO] != taglo_before ||
      !machine->cpu.icache[target_index].valid ||
      machine->cpu.icache[target_index].physical_tag !=
          target_before.physical_tag ||
      memcmp(machine->cpu.icache[target_index].data, target_before.data,
             sizeof target_before.data) != 0) {
    (void)puts("# uncacheable Hit_Invalidate I was not refused without effects");
    return false;
  }

  machine->cpu.stopped = false;
  machine->cpu.stop_reason = 0u;
  machine->cpu.stop_pc = 0u;
  machine->cpu.stop_opcode = 0u;
  machine->cpu.attempt_outcome = T64_CPU_ATTEMPT_NONE;
  write_ram_instruction(machine, UINT32_C(0x3000),
                        cache_instruction(UINT32_C(0x1c), 5u));
  machine->cpu.gpr[5] = t64_cpu_canonical_address(UINT32_C(0x80001000));
  set_pc(machine, UINT32_C(0x80003000));
  if (t64_cpu_step(machine, NULL) != T64_STATUS_RESOURCE_EXHAUSTED ||
      machine->cpu.stop_opcode != cache_instruction(UINT32_C(0x1c), 5u) ||
      memcmp(&machine->cache, &cache_before, sizeof cache_before) != 0 ||
      machine->cpu.cp0[CP0_TAGLO] != taglo_before ||
      !line_matches(&machine->cpu.icache[target_index],
                    target_before.physical_tag, target_before.data)) {
    (void)puts("# manual-undefined CACHE operation changed cache or CP0 state");
    return false;
  }
  return true;
}

static bool test_cache_privilege_check_precedes_operation(
    fixture *value) {
  const uint32_t virtual_pc = UINT32_C(0x00004000);
  const uint32_t line_index = cache_index(virtual_pc);
  const uint32_t opcode = cache_instruction(CACHE_HIT_INVALIDATE_I, 5u);
  t64_machine *machine = value->machine;
  uint32_t exception_code;

  set_tlb_pair(machine, 0u, UINT64_C(0x4000), UINT64_C(0x2000), 3u);
  write_ram_instruction(machine, UINT32_C(0x2000), opcode);
  machine->cpu.cp0[CP0_STATUS] = UINT32_C(0x10); /* User, CU0 clear. */
  machine->cpu.gpr[5] = (uint64_t)virtual_pc;
  machine->cache.last_operation = UINT32_C(0x1234);
  machine->cache.last_pc = UINT32_C(0x5678);
  machine->cache.last_address = UINT32_C(0x9abc);
  machine->cache.operation_seen = false;
  set_pc(machine, virtual_pc);

  if (t64_cpu_step(machine, NULL) != T64_STATUS_OK) {
    (void)puts("# user CACHE did not enter its coprocessor-unusable exception");
    return false;
  }
  exception_code = ((uint32_t)machine->cpu.cp0[CP0_CAUSE] >> 2) &
                   UINT32_C(0x1f);
  if (exception_code != UINT32_C(11) ||
      machine->cpu.last_opcode != opcode ||
      !machine->cpu.icache[line_index].valid ||
      machine->cpu.icache[line_index].physical_tag != UINT32_C(0x2000) ||
      machine->cache.last_operation != UINT32_C(0x1234) ||
      machine->cache.last_pc != UINT32_C(0x5678) ||
      machine->cache.last_address != UINT32_C(0x9abc) ||
      machine->cache.operation_seen) {
    (void)printf("# user CACHE exception=%u opcode=%08x line_valid=%u "
                 "line_tag=%08x cache_op=%08x\n",
                 exception_code, machine->cpu.last_opcode,
                 machine->cpu.icache[line_index].valid ? 1u : 0u,
                 machine->cpu.icache[line_index].physical_tag,
                 machine->cache.last_operation);
    return false;
  }
  return true;
}

static bool test_dma_does_not_snoop_instruction_cache(
    fixture *value, bool sp_dma) {
  const uint32_t physical_code = UINT32_C(0x2000);
  const uint32_t virtual_code = UINT32_C(0x80002000);
  const uint32_t original_instruction = UINT32_C(0x24080001);
  const uint32_t line_index = cache_index(virtual_code);
  const uint32_t dma_base = sp_dma ? UINT32_C(0x04040000)
                                   : UINT32_C(0x04600000);
  const uint32_t instruction_count = UINT32_C(32);
  t64_machine *machine = value->machine;
  t64_vr4300_icache_line line_before;
  t64_stop_request request = {T64_STOP_INSTRUCTION_COUNT,
                              (uint64_t)instruction_count};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  t64_status status;
  size_t index;

  for (index = 0u; index < instruction_count; index++) {
    write_ram_instruction(machine, physical_code + (uint32_t)(index * 4u),
                          index == 0u ? original_instruction : 0u);
  }
  if (sp_dma) {
    write_be32(machine->sp_dmem, UINT32_C(0x24080002));
    write_be32(machine->sp_dmem + 4u, UINT32_C(0));
  }
  set_pc(machine, virtual_code);
  if (t64_cpu_step(machine, NULL) != T64_STATUS_OK ||
      machine->cpu.gpr[8] != UINT64_C(1) ||
      !machine->cpu.icache[line_index].valid) {
    (void)puts("# DMA tracer did not cache the initial instruction");
    return false;
  }
  line_before = machine->cpu.icache[line_index];

  if (sp_dma) {
    status = t64_machine_bus_write(machine, dma_base, 4u, 0u);
    if (status == T64_STATUS_OK) {
      status = t64_machine_bus_write(machine, dma_base + 4u, 4u,
                                     physical_code);
    }
    if (status == T64_STATUS_OK) {
      status = t64_machine_bus_write(machine, dma_base + 12u, 4u,
                                     UINT32_C(7));
    }
  } else {
    status = t64_machine_bus_write(machine, dma_base, 4u, physical_code);
    if (status == T64_STATUS_OK) {
      status = t64_machine_bus_write(machine, dma_base + 4u, 4u,
                                     UINT32_C(0x10000000));
    }
    if (status == T64_STATUS_OK) {
      status = t64_machine_bus_write(machine, dma_base + 12u, 4u,
                                     UINT32_C(7));
    }
  }
  if (status != T64_STATUS_OK) {
    (void)printf("# %s DMA request returned status=%u\n",
                 sp_dma ? "SP" : "PI", (unsigned)status);
    return false;
  }
  status = t64_machine_advance(machine, &request, &result);
  if (status != T64_STATUS_OK ||
      result.reason != T64_STOP_REASON_INSTRUCTION_COUNT ||
      machine->instruction_count != (uint64_t)instruction_count) {
    (void)printf("# %s DMA progress status=%u reason=%u instructions=%llu "
                 "tick=%llu\n",
                 sp_dma ? "SP" : "PI", (unsigned)status, result.reason,
                 (unsigned long long)machine->instruction_count,
                 (unsigned long long)result.tick);
    return false;
  }
  if (sp_dma) {
    if (read_be32(machine->ram + physical_code) != UINT32_C(0x24080002)) {
      (void)puts("# SP DMA did not change the RDRAM instruction bytes");
      return false;
    }
  } else if (memcmp(machine->ram + physical_code, value->asset, 8u) != 0 ||
             read_be32(value->asset) == original_instruction) {
    (void)puts("# PI DMA did not install distinct cartridge backing bytes");
    return false;
  }
  if (!line_matches(&machine->cpu.icache[line_index],
                    line_before.physical_tag, line_before.data)) {
    (void)puts("# PI/SP DMA snooped or replaced a valid I-cache line");
    return false;
  }
  machine->cpu.gpr[8] = 0u;
  set_pc(machine, virtual_code);
  if (t64_cpu_step(machine, NULL) != T64_STATUS_OK ||
      machine->cpu.last_opcode != original_instruction ||
      machine->cpu.gpr[8] != UINT64_C(1) ||
      machine->cpu.attempt_outcome != T64_CPU_ATTEMPT_RETIRED) {
    (void)printf("# %s DMA stale execution opcode=%08x gpr8=%016llx "
                 "outcome=%u\n",
                 sp_dma ? "SP" : "PI", machine->cpu.last_opcode,
                 (unsigned long long)machine->cpu.gpr[8],
                 machine->cpu.attempt_outcome);
    return false;
  }
  return true;
}

static bool test_pi_dma_does_not_snoop_instruction_cache(
    fixture *value) {
  return test_dma_does_not_snoop_instruction_cache(value, false);
}

static bool test_sp_dma_does_not_snoop_instruction_cache(
    fixture *value) {
  return test_dma_does_not_snoop_instruction_cache(value, true);
}

static bool test_cache_digest_sensitivity_and_machine_isolation(
    fixture *value) {
  static const uint8_t zero_line[T64_VR4300_ICACHE_LINE_BYTES] = {0};
  static const uint8_t zero_dline[T64_VR4300_DCACHE_LINE_BYTES] = {0};
  fixture other = {0};
  uint8_t baseline[T64_DIGEST_SIZE];
  uint8_t changed[T64_DIGEST_SIZE];
  uint8_t other_baseline[T64_DIGEST_SIZE];
  t64_vr4300_icache_line saved_line;
  t64_vr4300_dcache_line saved_dline;
  t64_machine *machine = value->machine;
  bool okay = true;
  size_t index;

  if (!fixture_open(&other, value->asset)) {
    (void)puts("# digest/isolation fixture setup failed");
    return fixture_close(&other, false);
  }
  for (index = 0u; index < T64_VR4300_ICACHE_LINE_COUNT; index++) {
    const t64_vr4300_icache_line *line = &machine->cpu.icache[index];
    const t64_vr4300_icache_line *other_line =
        &other.machine->cpu.icache[index];

    if (line->valid || line->physical_tag != 0u ||
        memcmp(line->data, zero_line, sizeof zero_line) != 0 ||
        other_line->valid || other_line->physical_tag != 0u ||
        memcmp(other_line->data, zero_line, sizeof zero_line) != 0) {
      (void)printf("# prepared I-cache line %zu was not invalid and zero\n",
                   index);
      return fixture_close(&other, false);
    }
  }
  for (index = 0u; index < T64_VR4300_DCACHE_LINE_COUNT; index++) {
    const t64_vr4300_dcache_line *line = &machine->cpu.dcache[index];
    const t64_vr4300_dcache_line *other_line =
        &other.machine->cpu.dcache[index];

    if (line->valid || line->dirty || line->physical_tag != 0u ||
        memcmp(line->data, zero_dline, sizeof zero_dline) != 0 ||
        other_line->valid || other_line->dirty ||
        other_line->physical_tag != 0u ||
        memcmp(other_line->data, zero_dline, sizeof zero_dline) != 0) {
      (void)printf("# prepared D-cache line %zu was not invalid and zero\n",
                   index);
      return fixture_close(&other, false);
    }
  }
  if (t64_machine_digest(machine, baseline) != T64_STATUS_OK ||
      t64_machine_digest(other.machine, other_baseline) != T64_STATUS_OK) {
    (void)puts("# digest/isolation fixture digest failed");
    return fixture_close(&other, false);
  }
  saved_line = machine->cpu.icache[17];
  machine->cpu.icache[17].physical_tag ^= UINT32_C(0x20);
  okay = t64_machine_digest(machine, changed) == T64_STATUS_OK &&
         memcmp(baseline, changed, sizeof baseline) != 0;
  machine->cpu.icache[17] = saved_line;

  if (okay) {
    machine->cpu.icache[17].valid = true;
    okay = t64_machine_digest(machine, changed) == T64_STATUS_OK &&
           memcmp(baseline, changed, sizeof baseline) != 0;
    machine->cpu.icache[17] = saved_line;
  }
  if (okay) {
    machine->cpu.icache[17].data[9] ^= UINT8_C(1);
    okay = t64_machine_digest(machine, changed) == T64_STATUS_OK &&
           memcmp(baseline, changed, sizeof baseline) != 0;
    machine->cpu.icache[17] = saved_line;
  }
  if (okay) {
    machine->cache.last_operation ^= UINT32_C(1);
    okay = t64_machine_digest(machine, changed) == T64_STATUS_OK &&
           memcmp(baseline, changed, sizeof baseline) != 0;
    machine->cache.last_operation ^= UINT32_C(1);
  }
  saved_dline = machine->cpu.dcache[17];
  if (okay) {
    machine->cpu.dcache[17].physical_tag ^= UINT32_C(0x10);
    okay = t64_machine_digest(machine, changed) == T64_STATUS_OK &&
           memcmp(baseline, changed, sizeof baseline) != 0;
    machine->cpu.dcache[17] = saved_dline;
  }
  if (okay) {
    machine->cpu.dcache[17].valid = true;
    okay = t64_machine_digest(machine, changed) == T64_STATUS_OK &&
           memcmp(baseline, changed, sizeof baseline) != 0;
    machine->cpu.dcache[17] = saved_dline;
  }
  if (okay) {
    machine->cpu.dcache[17].dirty = true;
    okay = t64_machine_digest(machine, changed) == T64_STATUS_OK &&
           memcmp(baseline, changed, sizeof baseline) != 0;
    machine->cpu.dcache[17] = saved_dline;
  }
  if (okay) {
    machine->cpu.dcache[17].data[9] ^= UINT8_C(1);
    okay = t64_machine_digest(machine, changed) == T64_STATUS_OK &&
           memcmp(baseline, changed, sizeof baseline) != 0;
    machine->cpu.dcache[17] = saved_dline;
  }
  if (okay) {
    machine->cpu.icache[17].valid = true;
    machine->cpu.icache[17].physical_tag = UINT32_C(0x12340);
    machine->cpu.icache[17].data[0] = UINT8_C(0xa5);
    okay = !other.machine->cpu.icache[17].valid &&
           other.machine->cpu.icache[17].physical_tag == 0u &&
           other.machine->cpu.icache[17].data[0] == 0u &&
           t64_machine_digest(other.machine, changed) == T64_STATUS_OK &&
           memcmp(other_baseline, changed, sizeof other_baseline) == 0;
    machine->cpu.icache[17] = saved_line;
  }
  if (okay) {
    machine->cpu.dcache[17].valid = true;
    machine->cpu.dcache[17].dirty = true;
    machine->cpu.dcache[17].physical_tag = UINT32_C(0x12340);
    machine->cpu.dcache[17].data[0] = UINT8_C(0xa5);
    okay = !other.machine->cpu.dcache[17].valid &&
           !other.machine->cpu.dcache[17].dirty &&
           other.machine->cpu.dcache[17].physical_tag == 0u &&
           other.machine->cpu.dcache[17].data[0] == 0u &&
           t64_machine_digest(other.machine, changed) == T64_STATUS_OK &&
           memcmp(other_baseline, changed, sizeof other_baseline) == 0;
    machine->cpu.dcache[17] = saved_dline;
  }
  if (!okay) {
    (void)puts("# v9 line/diagnostic sensitivity or machine isolation failed");
  }
  return fixture_close(&other, okay);
}

int main(int argc, char **argv) {
  static const cache_test_case icache_cases[] = {
      {"index 511/0 boundary, final word and same-index physical tag replacement",
       test_index_wrap_and_same_index_replacement},
      {"TLB synonyms, C=2, and Config.K0=2 preserve cacheable lines",
       test_tlb_synonyms_and_uncached_bypass},
      {"TLB cache algorithms 0, 1, and 3 use the modeled I-cache",
       test_tlb_cache_algorithm_attributes},
      {"RDRAM, SP DMEM, SP IMEM, and cartridge lines fill from their backing",
       test_all_cacheable_backing_domains},
      {"out-of-image cartridge refill refuses without replacing a valid line",
       test_cartridge_range_refusal_preserves_line},
      {"Hit_Invalidate I preserves the D-link and checks the physical tag",
       test_hit_invalidate_does_not_clear_link},
      {"uncached and manual-undefined CACHE operations refuse without effects",
       test_uncached_and_undefined_cache_refusals},
      {"CACHE privilege check precedes translation and cache effects",
       test_cache_privilege_check_precedes_operation},
      {"PI DMA changes backing without snooping a valid I-cache line",
       test_pi_dma_does_not_snoop_instruction_cache},
      {"SP DMA changes backing without snooping a valid I-cache line",
       test_sp_dma_does_not_snoop_instruction_cache},
      {"prepared I-cache is invalid; v9 hashes line fields and isolates machines",
       test_cache_digest_sensitivity_and_machine_isolation},
  };
  static const cache_test_case cache_ops_cases[] = {
      {"CACHE p.404 op6/I writes a matching I-line unconditionally",
       test_hit_writeback_i_unconditional},
      {"CACHE p.404 op6/I readonly writeback refusal is atomic",
       test_hit_writeback_i_readonly_refusal_is_atomic},
      {"mapped cartridge D writeback refusal remains distinct from DBE",
       test_cache_mapped_device_writeback_remains_model_refusal},
      {"CACHE p.404 op0/I invalidates by virtual index without D-link effects",
       test_index_invalidate_i},
      {"CACHE p.404 op0/D writes a clean non-Invalid tagged line before invalidating",
       test_index_writeback_invalidate_d_writes_clean_line},
      {"CACHE p.404 op1/I maps internal aligned key to hardware TagLo",
       test_index_load_tag_i},
      {"CACHE p.404 op1/D reports PTag and dirty PState through TagLo",
       test_index_load_tag_d},
      {"CACHE p.404 op2/I reconstructs PA low bits from the I index",
       test_index_store_tag_i},
      {"CACHE p.404 op2/D reconstructs PA low bits from the D index",
       test_index_store_tag_d},
      {"CACHE p.404 op3/D writes back dirty replacement before create-exclusive",
       test_create_dirty_exclusive_d_writes_old_dirty_line},
      {"CACHE p.404 op4/D invalidates only its hit and selected LL range",
       test_hit_invalidate_d_exact_link},
      {"CACHE p.404 op5/I refills the selected line from backing",
       test_fill_i_refills_from_backing},
      {"CACHE p.404 op5/D writes a dirty hit before invalidation",
       test_hit_writeback_invalidate_d_dirty_line},
      {"CACHE p.404 op6/D clean line does not write backing",
       test_hit_writeback_d_clean_line_does_not_write},
      {"undefined TagLo PState 01 leaves D line and diagnostics unchanged",
       test_undefined_tag_state_is_atomic},
      {"mapped Index_Load_Tag reports TLB invalid before cache effects",
       test_cache_tlb_fault_precedes_index_operation},
      {"CACHE Bus error routes unmapped dirty writeback through precise DBE",
       test_cache_unmapped_writeback_raises_dbe_atomically},
      {"cached LW eviction of unmapped dirty line raises precise DBE",
       test_cached_load_unmapped_eviction_raises_dbe},
      {"cached SW eviction of unmapped dirty line raises precise DBE",
       test_cached_store_unmapped_eviction_raises_dbe},
  };
  static const cache_test_case dcache_cases[] = {
      {"512-line D-cache wraps index 511 to 0 and writes back dirty replacement",
       test_dcache_index_511_wrap_and_dirty_eviction},
      {"D-cache byte masks update only selected bytes and exact LL intervals",
       test_dcache_masked_store_and_exact_link_bytes},
      {"cached user RE stores map byte/half/word/doubleword lanes exactly",
       test_cached_reverse_endian_store_masks_and_widths},
      {"D-cache refused dirty writeback and fill preserve line/backing state",
       test_dcache_refused_fill_and_writeback_are_atomic},
      {"D-cache writeback covers RDRAM, SP DMEM, and SP IMEM",
       test_dcache_writeback_all_accepted_mutable_domains},
  };
  const cache_test_case *cases = icache_cases;
  size_t case_count = sizeof icache_cases / sizeof icache_cases[0];
  uint8_t asset[IPL3_ASSET_BYTES];
  size_t failure_count = 0u;
  size_t index;

  if (argc == 2 && strcmp(argv[1], "--cache-ops") == 0) {
    cases = cache_ops_cases;
    case_count = sizeof cache_ops_cases / sizeof cache_ops_cases[0];
  } else if (argc == 2 && strcmp(argv[1], "--dcache") == 0) {
    cases = dcache_cases;
    case_count = sizeof dcache_cases / sizeof dcache_cases[0];
  } else if (argc != 1) {
    (void)fprintf(stderr, "usage: %s [--cache-ops|--dcache]\n", argv[0]);
    return 2;
  }

  if (!read_seed_asset(asset)) {
    (void)puts("1..1");
    (void)puts("not ok 1 - qualified IPL3 fixture setup");
    return 1;
  }
  (void)printf("1..%zu\n", case_count);
  for (index = 0u; index < case_count; index++) {
    fixture value = {0};
    bool okay = fixture_open(&value, asset);

    if (okay) {
      okay = cases[index].run(&value);
    } else {
      (void)puts("# VR4300 cache fixture setup failed");
    }
    okay = fixture_close(&value, okay);
    (void)printf("%s %zu - %s\n", okay ? "ok" : "not ok", index + 1u,
                 cases[index].name);
    if (!okay) {
      failure_count++;
    }
  }
  return failure_count == 0u ? 0 : 1;
}
