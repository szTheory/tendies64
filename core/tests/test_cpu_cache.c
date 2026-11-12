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
#define CACHE_HIT_INVALIDATE_I UINT32_C(0x10)

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
  fixture other = {0};
  uint8_t baseline[T64_DIGEST_SIZE];
  uint8_t changed[T64_DIGEST_SIZE];
  uint8_t other_baseline[T64_DIGEST_SIZE];
  t64_vr4300_icache_line saved_line;
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
  if (!okay) {
    (void)puts("# v9 line/diagnostic sensitivity or machine isolation failed");
  }
  return fixture_close(&other, okay);
}

int main(void) {
  static const cache_test_case cases[] = {
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
  uint8_t asset[IPL3_ASSET_BYTES];
  size_t failure_count = 0u;
  size_t index;

  if (!read_seed_asset(asset)) {
    (void)puts("1..1");
    (void)puts("not ok 1 - qualified IPL3 fixture setup");
    return 1;
  }
  (void)printf("1..%zu\n", sizeof cases / sizeof cases[0]);
  for (index = 0u; index < sizeof cases / sizeof cases[0]; index++) {
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
