#include "t64_internal.h"
#include "tendies64/t64_machine.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IPL3_ASSET_BYTES 4096u
#define CP0_INDEX UINT32_C(0)
#define CP0_RANDOM UINT32_C(1)
#define CP0_ENTRYLO0 UINT32_C(2)
#define CP0_ENTRYLO1 UINT32_C(3)
#define CP0_CONTEXT UINT32_C(4)
#define CP0_PAGEMASK UINT32_C(5)
#define CP0_WIRED UINT32_C(6)
#define CP0_BADVADDR UINT32_C(8)
#define CP0_ENTRYHI UINT32_C(10)
#define CP0_STATUS UINT32_C(12)
#define CP0_CAUSE UINT32_C(13)
#define CP0_EPC UINT32_C(14)
#define CP0_XCONTEXT UINT32_C(20)
#define STATUS_BEV UINT32_C(0x00400000)
#define STATUS_EXL UINT32_C(0x00000002)
#define STATUS_TS UINT32_C(0x00200000)
#define ENTRYLO_PFN_MASK UINT32_C(0x03ffffc0)
#define ENTRYLO_CACHE_SHIFT 3u
#define ENTRYLO_DIRTY UINT32_C(0x4)
#define ENTRYLO_VALID UINT32_C(0x2)
#define ENTRYLO_GLOBAL UINT32_C(0x1)
#define CAUSE_EXCCODE_MASK UINT32_C(0x7c)
#define INDEX_PROBE UINT32_C(0x80000000)

typedef struct {
  size_t outstanding;
} allocator_state;

typedef struct {
  allocator_state allocator;
  t64_machine *machine;
} fixture;

typedef struct {
  const char *name;
  bool (*run)(void);
} mmu_test_case;

static bool read_seed_asset(uint8_t bytes[IPL3_ASSET_BYTES]);

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

static bool fixture_open(fixture *value, uint32_t status, uint32_t asid) {
  const t64_host_allocator allocator = {
      test_allocate, test_release, &value->allocator};
  const t64_profile_descriptor *profile = NULL;
  uint8_t asset[IPL3_ASSET_BYTES];

  if (value == NULL ||
      t64_profile_lookup("ntsc-cart-4m-openipl3-vr4300-v1", &profile) !=
          T64_STATUS_OK ||
      profile == NULL ||
      t64_machine_create(&allocator, profile, &value->machine) !=
          T64_STATUS_OK ||
      value->machine == NULL || !read_seed_asset(asset) ||
      t64_machine_admit_rom(value->machine, asset, sizeof asset) !=
          T64_STATUS_OK) {
    return false;
  }
  value->machine->cpu.cp0[CP0_STATUS] = status;
  value->machine->cpu.cp0[CP0_ENTRYHI] = (uint64_t)(asid & UINT32_C(0xff));
  for (size_t index = 0u; index < T64_VR4300_TLB_ENTRY_COUNT; index++) {
    value->machine->cpu.tlb[index] = (t64_vr4300_tlb_pair){0};
    value->machine->cpu.tlb[index].asid = UINT32_C(0xff);
  }
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

static void set_entry(t64_machine *machine, uint32_t index,
                      uint64_t page_mask, uint64_t virtual_base,
                      uint32_t asid, uint32_t entry_lo0,
                      uint32_t entry_lo1) {
  t64_vr4300_tlb_pair *entry = &machine->cpu.tlb[index];

  entry->page_mask = page_mask;
  entry->vpn2 = virtual_base & UINT64_C(0x000000ffffffe000);
  entry->region = (virtual_base >> 62) & UINT64_C(3);
  entry->asid = asid & UINT32_C(0xff);
  entry->entry_lo0 = entry_lo0;
  entry->entry_lo1 = entry_lo1;
}

static uint32_t entry_lo(uint64_t physical_base, uint32_t cache_algorithm,
                         bool valid, bool dirty, bool global) {
  uint32_t result =
      (uint32_t)(((physical_base >> 12) << 6) & ENTRYLO_PFN_MASK) |
      ((cache_algorithm & UINT32_C(7)) << ENTRYLO_CACHE_SHIFT);

  if (valid) {
    result |= ENTRYLO_VALID;
  }
  if (dirty) {
    result |= ENTRYLO_DIRTY;
  }
  if (global) {
    result |= ENTRYLO_GLOBAL;
  }
  return result;
}

static bool translate_matches(t64_machine *machine, uint64_t address,
                              uint32_t width, t64_mmu_access_kind kind,
                              t64_mmu_fault fault, uint64_t physical,
                              uint32_t entry_index) {
  t64_mmu_request request = {address, width, kind};
  t64_mmu_result result = {0};

  if (!t64_mmu_translate(machine, &request, &result) ||
      result.fault != fault ||
      (fault == T64_MMU_FAULT_NONE &&
       (!result.translated || result.physical_address != physical ||
        result.matched_entry != entry_index)) ||
      (fault != T64_MMU_FAULT_NONE && result.translated)) {
    (void)printf("# translate va=%016llx width=%u kind=%u expected fault=%u "
                 "pa=%016llx entry=%u observed fault=%u translated=%u "
                 "pa=%016llx entry=%u context=%u refill=%u\n",
                 (unsigned long long)address, width, (unsigned)kind,
                 (unsigned)fault, (unsigned long long)physical, entry_index,
                 (unsigned)result.fault, result.translated ? 1u : 0u,
                 (unsigned long long)result.physical_address,
                 result.matched_entry, result.tlb_context_valid ? 1u : 0u,
                 result.tlb_refill_vector ? 1u : 0u);
    return false;
  }
  return true;
}

static bool test_all_page_masks_and_pair_boundaries(void) {
  static const struct {
    uint64_t mask;
    uint32_t shift;
  } page_sizes[] = {
      {UINT64_C(0x00000000), 12u}, {UINT64_C(0x00006000), 14u},
      {UINT64_C(0x0001e000), 16u}, {UINT64_C(0x0007e000), 18u},
      {UINT64_C(0x001fe000), 20u}, {UINT64_C(0x007fe000), 22u},
      {UINT64_C(0x01ffe000), 24u},
  };
  fixture value = {0};
  const uint64_t virtual_base = UINT64_C(0x10000000);
  const uint64_t physical_even = UINT64_C(0x04000000);
  const uint64_t physical_odd = UINT64_C(0x08000000);
  bool okay = false;

  if (!fixture_open(&value, 0u, 1u)) {
    return fixture_close(&value, false);
  }
  for (size_t index = 0u; index < sizeof page_sizes / sizeof page_sizes[0];
       index++) {
    uint64_t page_bytes = UINT64_C(1) << page_sizes[index].shift;
    uint64_t pair_bytes = page_bytes * UINT64_C(2);
    uint64_t even_last = virtual_base + page_bytes - UINT64_C(1);
    uint64_t odd_first = virtual_base + page_bytes;
    uint64_t odd_last = virtual_base + pair_bytes - UINT64_C(1);
    uint32_t lo0 = entry_lo(physical_even, 3u, true, true, false);
    uint32_t lo1 = entry_lo(physical_odd, 3u, true, true, false);

    if (!t64_mmu_page_mask_is_defined(page_sizes[index].mask)) {
      (void)printf("# manual PageMask %016llx rejected\n",
                   (unsigned long long)page_sizes[index].mask);
      return fixture_close(&value, false);
    }
    for (size_t slot = 0u; slot < T64_VR4300_TLB_ENTRY_COUNT; slot++) {
      value.machine->cpu.tlb[slot] = (t64_vr4300_tlb_pair){0};
      value.machine->cpu.tlb[slot].asid = UINT32_C(0xff);
    }
    set_entry(value.machine, 31u, page_sizes[index].mask, virtual_base, 1u,
              lo0, lo1);
    if (!translate_matches(value.machine, virtual_base, 1u,
                           T64_MMU_ACCESS_READ, T64_MMU_FAULT_NONE,
                           physical_even, 31u) ||
        !translate_matches(value.machine, even_last, 1u,
                           T64_MMU_ACCESS_READ, T64_MMU_FAULT_NONE,
                           physical_even + page_bytes - UINT64_C(1), 31u) ||
        !translate_matches(value.machine, odd_first, 1u,
                           T64_MMU_ACCESS_READ, T64_MMU_FAULT_NONE,
                           physical_odd, 31u) ||
        !translate_matches(value.machine, odd_last, 1u,
                           T64_MMU_ACCESS_READ, T64_MMU_FAULT_NONE,
                           physical_odd + page_bytes - UINT64_C(1), 31u) ||
        !translate_matches(value.machine, virtual_base - UINT64_C(1), 1u,
                           T64_MMU_ACCESS_READ,
                           T64_MMU_FAULT_TLB_REFILL, 0u, UINT32_MAX) ||
        !translate_matches(value.machine, virtual_base + pair_bytes, 1u,
                           T64_MMU_ACCESS_READ,
                           T64_MMU_FAULT_TLB_REFILL, 0u, UINT32_MAX)) {
      return fixture_close(&value, false);
    }
  }
  okay = true;
  return fixture_close(&value, okay);
}

static bool test_asid_global_and_access_priority(void) {
  fixture value = {0};
  const uint64_t address = UINT64_C(0x2000);
  uint32_t mapped_lo = entry_lo(UINT64_C(0x1000), 3u, true, false, false);
  bool okay = false;

  if (!fixture_open(&value, 0u, 1u)) {
    return fixture_close(&value, false);
  }
  set_entry(value.machine, 30u, 0u, address, 2u, mapped_lo, mapped_lo);
  if (!translate_matches(value.machine, address, 4u, T64_MMU_ACCESS_READ,
                         T64_MMU_FAULT_TLB_REFILL, 0u, UINT32_MAX)) {
    return fixture_close(&value, false);
  }
  set_entry(value.machine, 30u, 0u, address, 1u,
            entry_lo(UINT64_C(0x1000), 3u, false, false, false),
            entry_lo(UINT64_C(0x2000), 3u, false, false, false));
  if (!translate_matches(value.machine, address, 4u, T64_MMU_ACCESS_READ,
                         T64_MMU_FAULT_TLB_INVALID, 0u, UINT32_MAX) ||
      !translate_matches(value.machine, address, 4u, T64_MMU_ACCESS_WRITE,
                         T64_MMU_FAULT_TLB_INVALID, 0u, UINT32_MAX)) {
    return fixture_close(&value, false);
  }
  set_entry(value.machine, 30u, 0u, address, 1u, mapped_lo, mapped_lo);
  if (!translate_matches(value.machine, address, 4u, T64_MMU_ACCESS_READ,
                         T64_MMU_FAULT_NONE, UINT64_C(0x1000), 30u) ||
      !translate_matches(value.machine, address, 4u, T64_MMU_ACCESS_FETCH,
                         T64_MMU_FAULT_NONE, UINT64_C(0x1000), 30u) ||
      !translate_matches(value.machine, address, 4u, T64_MMU_ACCESS_WRITE,
                         T64_MMU_FAULT_TLB_MODIFIED, 0u, UINT32_MAX)) {
    return fixture_close(&value, false);
  }
  set_entry(value.machine, 30u, 0u, address, 2u,
            entry_lo(UINT64_C(0x1000), 3u, true, true, true),
            entry_lo(UINT64_C(0x2000), 3u, true, true, false));
  if (!translate_matches(value.machine, address, 4u, T64_MMU_ACCESS_READ,
                         T64_MMU_FAULT_TLB_REFILL, 0u, UINT32_MAX)) {
    return fixture_close(&value, false);
  }
  set_entry(value.machine, 30u, 0u, address, 2u,
            entry_lo(UINT64_C(0x1000), 3u, true, true, true),
            entry_lo(UINT64_C(0x2000), 3u, true, true, true));
  if (!translate_matches(value.machine, address, 4u, T64_MMU_ACCESS_READ,
                         T64_MMU_FAULT_NONE, UINT64_C(0x1000), 30u)) {
    return fixture_close(&value, false);
  }
  okay = true;
  return fixture_close(&value, okay);
}

static bool expect_fault(t64_machine *machine, uint64_t address,
                         t64_mmu_fault fault, bool extended) {
  t64_mmu_request request = {address, 1u, T64_MMU_ACCESS_READ};
  t64_mmu_result result = {0};

  if (!t64_mmu_translate(machine, &request, &result) ||
      result.fault != fault || result.extended_address_mode != extended) {
    (void)printf("# segment va=%016llx status=%08x expected fault=%u "
                 "extended=%u observed fault=%u extended=%u\n",
                 (unsigned long long)address,
                 (uint32_t)machine->cpu.cp0[CP0_STATUS], (unsigned)fault,
                 extended ? 1u : 0u, (unsigned)result.fault,
                 result.extended_address_mode ? 1u : 0u);
    return false;
  }
  return true;
}

static bool test_segment_boundaries_and_privilege_modes(void) {
  fixture value = {0};
  t64_mmu_request request = {0};
  t64_mmu_result result = {0};
  bool okay = false;

  if (!fixture_open(&value, 0u, 0u)) {
    return fixture_close(&value, false);
  }
  value.machine->cpu.cp0[16] = UINT64_C(3);
  if (!t64_mmu_translate(value.machine,
                         &(t64_mmu_request){UINT64_C(0xffffffff80000000),
                                            1u, T64_MMU_ACCESS_READ},
                         &result) ||
      result.fault != T64_MMU_FAULT_NONE || result.physical_address != 0u ||
      !result.cacheable || result.cache_algorithm != UINT8_C(3) ||
      !translate_matches(value.machine, UINT64_C(0xffffffff9fffffff), 1u,
                         T64_MMU_ACCESS_READ, T64_MMU_FAULT_NONE,
                         UINT64_C(0x1fffffff), UINT32_MAX) ||
      !t64_mmu_translate(value.machine,
                         &(t64_mmu_request){UINT64_C(0xffffffffa0000000),
                                            1u, T64_MMU_ACCESS_READ},
                         &result) ||
      result.fault != T64_MMU_FAULT_NONE || result.physical_address != 0u ||
      result.cacheable || result.cache_algorithm != UINT8_C(2)) {
    return fixture_close(&value, false);
  }

  value.machine->cpu.cp0[CP0_STATUS] = UINT32_C(0x10);
  if (!expect_fault(value.machine, UINT64_C(0x7fffffff),
                    T64_MMU_FAULT_TLB_REFILL, false) ||
      !expect_fault(value.machine, UINT64_C(0x80000000),
                    T64_MMU_FAULT_ADDRESS, false)) {
    return fixture_close(&value, false);
  }
  value.machine->cpu.cp0[CP0_STATUS] = UINT32_C(0x8);
  if (!expect_fault(value.machine, UINT64_C(0xffffffffc0000000),
                    T64_MMU_FAULT_TLB_REFILL, false) ||
      !expect_fault(value.machine, UINT64_C(0xffffffffe0000000),
                    T64_MMU_FAULT_ADDRESS, false)) {
    return fixture_close(&value, false);
  }
  value.machine->cpu.cp0[CP0_STATUS] = 0u;
  if (!expect_fault(value.machine, UINT64_C(0xffffffffe0000000),
                    T64_MMU_FAULT_TLB_REFILL, false) ||
      !expect_fault(value.machine, UINT64_C(0x00000000c0000000),
                    T64_MMU_FAULT_ADDRESS, false)) {
    return fixture_close(&value, false);
  }

  value.machine->cpu.cp0[CP0_STATUS] = UINT32_C(0x30);
  if (!expect_fault(value.machine, UINT64_C(0x000000ffffffffff),
                    T64_MMU_FAULT_TLB_REFILL, true) ||
      !expect_fault(value.machine, UINT64_C(0x0000010000000000),
                    T64_MMU_FAULT_ADDRESS, true)) {
    return fixture_close(&value, false);
  }
  value.machine->cpu.cp0[CP0_STATUS] = UINT32_C(0x48);
  if (!expect_fault(value.machine, UINT64_C(0x000000ffffffffff),
                    T64_MMU_FAULT_TLB_REFILL, true) ||
      !expect_fault(value.machine, UINT64_C(0x400000ffffffffff),
                    T64_MMU_FAULT_TLB_REFILL, true) ||
      !expect_fault(value.machine, UINT64_C(0x4000010000000000),
                    T64_MMU_FAULT_ADDRESS, true) ||
      !expect_fault(value.machine, UINT64_C(0xffffffffdfffffff),
                    T64_MMU_FAULT_TLB_REFILL, true) ||
      !expect_fault(value.machine, UINT64_C(0xffffffffe0000000),
                    T64_MMU_FAULT_ADDRESS, true)) {
    return fixture_close(&value, false);
  }
  value.machine->cpu.cp0[CP0_STATUS] = UINT32_C(0x80);
  if (!expect_fault(value.machine, UINT64_C(0x000000ffffffffff),
                    T64_MMU_FAULT_TLB_REFILL, true) ||
      !expect_fault(value.machine, UINT64_C(0x400000ffffffffff),
                    T64_MMU_FAULT_TLB_REFILL, true) ||
      !expect_fault(value.machine, UINT64_C(0xc00000ff7fffffff),
                    T64_MMU_FAULT_TLB_REFILL, true) ||
      !expect_fault(value.machine, UINT64_C(0xc00000ff80000000),
                    T64_MMU_FAULT_ADDRESS, true) ||
      !expect_fault(value.machine, UINT64_C(0xffffffff80000000),
                    T64_MMU_FAULT_NONE, true)) {
    return fixture_close(&value, false);
  }
  if (!t64_mmu_translate(value.machine,
                         &(t64_mmu_request){UINT64_C(0xffffffffa0000000),
                                            1u, T64_MMU_ACCESS_READ},
                         &result) ||
      result.fault != T64_MMU_FAULT_NONE || result.physical_address != 0u ||
      result.cacheable || result.cache_algorithm != UINT8_C(2)) {
    return fixture_close(&value, false);
  }
  request.virtual_address = UINT64_C(0x9000000000001234);
  request.width = 1u;
  request.kind = T64_MMU_ACCESS_READ;
  if (!t64_mmu_translate(value.machine, &request, &result) ||
      result.fault != T64_MMU_FAULT_NONE || !result.translated ||
      result.physical_domain != T64_MMU_DOMAIN_XKPHYS ||
      result.physical_address != UINT64_C(0x1234) || result.cacheable ||
      result.cache_algorithm != UINT8_C(2)) {
    (void)printf("# xkphys valid status=%u fault=%u domain=%u pa=%016llx "
                 "cache=%u algorithm=%u\n",
                 0u, (unsigned)result.fault,
                 (unsigned)result.physical_domain,
                 (unsigned long long)result.physical_address,
                 result.cacheable ? 1u : 0u, result.cache_algorithm);
    return fixture_close(&value, false);
  }
  request.virtual_address = UINT64_C(0x8000000100000000);
  if (!t64_mmu_translate(value.machine, &request, &result) ||
      result.fault != T64_MMU_FAULT_ADDRESS || result.translated ||
      result.physical_domain != T64_MMU_DOMAIN_XKPHYS) {
    (void)printf("# xkphys width refusal expected address fault/domain 1; "
                 "observed fault=%u translated=%u domain=%u\n",
                 (unsigned)result.fault, result.translated ? 1u : 0u,
                 (unsigned)result.physical_domain);
    return fixture_close(&value, false);
  }
  request.virtual_address = UINT64_C(0x1001);
  request.width = 4u;
  if (!t64_mmu_translate(value.machine, &request, &result) ||
      result.fault != T64_MMU_FAULT_ADDRESS) {
    return fixture_close(&value, false);
  }
  okay = true;
  return fixture_close(&value, okay);
}

static bool test_fault_context_and_priority(void) {
  fixture value = {0};
  t64_mmu_request request = {UINT64_C(0x2000), 4u, T64_MMU_ACCESS_READ};
  t64_mmu_result result = {0};
  bool okay = false;

  if (!fixture_open(&value, 0u, 0u)) {
    return fixture_close(&value, false);
  }
  if (!t64_mmu_translate(value.machine, &request, &result) ||
      result.fault != T64_MMU_FAULT_TLB_REFILL ||
      !result.tlb_context_valid || !result.tlb_refill_vector ||
      result.extended_address_mode) {
    return fixture_close(&value, false);
  }
  value.machine->cpu.cp0[CP0_STATUS] = STATUS_EXL;
  if (!t64_mmu_translate(value.machine, &request, &result) ||
      result.fault != T64_MMU_FAULT_TLB_REFILL ||
      !result.tlb_context_valid || result.tlb_refill_vector) {
    return fixture_close(&value, false);
  }
  value.machine->cpu.cp0[CP0_STATUS] = 0u;
  set_entry(value.machine, 31u, 0u, UINT64_C(0x2000), 0u,
            entry_lo(UINT64_C(0x1000), 3u, false, false, false),
            entry_lo(UINT64_C(0x2000), 3u, false, false, false));
  if (!t64_mmu_translate(value.machine, &request, &result) ||
      result.fault != T64_MMU_FAULT_TLB_INVALID ||
      !result.tlb_context_valid || result.tlb_refill_vector) {
    return fixture_close(&value, false);
  }
  request.kind = T64_MMU_ACCESS_WRITE;
  if (!t64_mmu_translate(value.machine, &request, &result) ||
      result.fault != T64_MMU_FAULT_TLB_INVALID) {
    return fixture_close(&value, false);
  }
  set_entry(value.machine, 31u, 0u, UINT64_C(0x2000), 0u,
            entry_lo(UINT64_C(0x1000), 3u, true, false, false),
            entry_lo(UINT64_C(0x2000), 3u, true, false, false));
  if (!t64_mmu_translate(value.machine, &request, &result) ||
      result.fault != T64_MMU_FAULT_TLB_MODIFIED ||
      !result.tlb_context_valid) {
    return fixture_close(&value, false);
  }
  okay = true;
  return fixture_close(&value, okay);
}

static bool test_extended_regions_and_cache_attributes(void) {
  static const struct {
    uint64_t address;
    uint64_t physical;
    uint64_t region;
  } extended_regions[] = {
      {UINT64_C(0x0000000000002000), UINT64_C(0x1000), UINT64_C(0)},
      {UINT64_C(0x4000000000002000), UINT64_C(0x2000), UINT64_C(1)},
      {UINT64_C(0xc000000000002000), UINT64_C(0x3000), UINT64_C(3)},
  };
  fixture value = {0};
  bool okay = false;

  if (!fixture_open(&value, UINT32_C(0x80), 7u)) {
    return fixture_close(&value, false);
  }
  for (size_t index = 0u;
       index < sizeof extended_regions / sizeof extended_regions[0]; index++) {
    t64_mmu_request request = {extended_regions[index].address, 1u,
                               T64_MMU_ACCESS_READ};
    t64_mmu_result result = {0};
    uint32_t lo = entry_lo(extended_regions[index].physical, 3u, true, true,
                           false);

    set_entry(value.machine, 5u, 0u, extended_regions[index].address, 7u,
              lo, lo);
    if (!t64_mmu_translate(value.machine, &request, &result) ||
        result.fault != T64_MMU_FAULT_NONE || !result.translated ||
        result.physical_address != extended_regions[index].physical ||
        result.matched_entry != UINT32_C(5) ||
        value.machine->cpu.tlb[5].region != extended_regions[index].region) {
      (void)printf("# extended region=%llu va=%016llx fault=%u pa=%016llx "
                   "matched=%u entry-region=%llu\n",
                   (unsigned long long)extended_regions[index].region,
                   (unsigned long long)extended_regions[index].address,
                   (unsigned)result.fault,
                   (unsigned long long)result.physical_address,
                   result.matched_entry,
                   (unsigned long long)value.machine->cpu.tlb[5].region);
      return fixture_close(&value, false);
    }
  }
  value.machine->cpu.tlb[5].region = UINT64_C(0);
  if (!translate_matches(value.machine,
                         UINT64_C(0x4000000000002000), 1u,
                         T64_MMU_ACCESS_READ, T64_MMU_FAULT_TLB_REFILL, 0u,
                         UINT32_MAX)) {
    return fixture_close(&value, false);
  }

  value.machine->cpu.cp0[CP0_STATUS] = 0u;
  value.machine->cpu.cp0[CP0_ENTRYHI] = 0u;
  for (uint32_t cache_algorithm = 0u; cache_algorithm < 8u;
       cache_algorithm++) {
    uint32_t lo = entry_lo(UINT64_C(0x1000), cache_algorithm, true, true,
                           false);
    t64_mmu_request request = {UINT64_C(0x2000), 1u,
                               T64_MMU_ACCESS_READ};
    t64_mmu_result result = {0};

    set_entry(value.machine, 0u, 0u, UINT64_C(0x2000), 0u, lo, lo);
    if (!t64_mmu_translate(value.machine, &request, &result) ||
        result.fault != T64_MMU_FAULT_NONE ||
        result.cache_algorithm != cache_algorithm ||
        result.cacheable != (cache_algorithm != UINT32_C(2))) {
      (void)printf("# TLB C=%u fault=%u observed C=%u cacheable=%u\n",
                   cache_algorithm, (unsigned)result.fault,
                   result.cache_algorithm,
                   result.cacheable ? 1u : 0u);
      return fixture_close(&value, false);
    }
  }
  okay = true;
  return fixture_close(&value, okay);
}

static bool test_tlb_management_and_invalid_mask_refusal(void) {
  fixture value = {0};
  t64_cp0_effect effect = {0};
  t64_vr4300_tlb_pair saved = {0};
  bool okay = false;

  if (!fixture_open(&value, 0u, 3u)) {
    return fixture_close(&value, false);
  }
  value.machine->cpu.cp0[CP0_INDEX] = UINT64_C(0x80000025);
  value.machine->cpu.cp0[CP0_PAGEMASK] = UINT64_C(0x6000);
  value.machine->cpu.cp0[CP0_ENTRYHI] = UINT64_C(0x4003);
  value.machine->cpu.cp0[CP0_ENTRYLO0] =
      entry_lo(UINT64_C(0x04000000), 3u, true, true, false);
  value.machine->cpu.cp0[CP0_ENTRYLO1] =
      entry_lo(UINT64_C(0x04004000), 3u, true, true, false);
  if (t64_cp0_prepare_tlb_instruction(value.machine, UINT32_C(0x42000002),
                                      &effect) != T64_CP0_TRANSFER_OK ||
      effect.tlb_operation != T64_CP0_TLB_WRITE_INDEXED ||
      effect.tlb_index != UINT32_C(5) ||
      value.machine->cpu.tlb[5].page_mask != 0u) {
    return fixture_close(&value, false);
  }
  t64_cp0_commit_transfer(value.machine, &effect);
  if (value.machine->cpu.tlb[5].page_mask != UINT64_C(0x6000) ||
      value.machine->cpu.tlb[5].vpn2 != UINT64_C(0x4000) ||
      value.machine->cpu.tlb[5].region != UINT64_C(0) ||
      value.machine->cpu.tlb[5].asid != UINT32_C(3) ||
      value.machine->cpu.tlb[5].entry_lo0 !=
          value.machine->cpu.cp0[CP0_ENTRYLO0] ||
      value.machine->cpu.tlb[5].entry_lo1 !=
          value.machine->cpu.cp0[CP0_ENTRYLO1]) {
    return fixture_close(&value, false);
  }

  value.machine->cpu.cp0[CP0_INDEX] = UINT64_C(5);
  effect = (t64_cp0_effect){0};
  if (t64_cp0_prepare_tlb_instruction(value.machine, UINT32_C(0x42000001),
                                      &effect) != T64_CP0_TRANSFER_OK ||
      effect.tlb_operation != T64_CP0_TLB_READ) {
    return fixture_close(&value, false);
  }
  value.machine->cpu.cp0[CP0_PAGEMASK] = 0u;
  value.machine->cpu.cp0[CP0_ENTRYHI] = 0u;
  value.machine->cpu.cp0[CP0_ENTRYLO0] = 0u;
  value.machine->cpu.cp0[CP0_ENTRYLO1] = 0u;
  t64_cp0_commit_transfer(value.machine, &effect);
  if (value.machine->cpu.cp0[CP0_PAGEMASK] != UINT64_C(0x6000) ||
      value.machine->cpu.cp0[CP0_ENTRYHI] != UINT64_C(0x4003) ||
      value.machine->cpu.cp0[CP0_ENTRYLO0] == 0u ||
      value.machine->cpu.cp0[CP0_ENTRYLO1] == 0u) {
    return fixture_close(&value, false);
  }

  value.machine->cpu.cp0[CP0_ENTRYHI] = UINT64_C(0x4003);
  value.machine->cpu.cp0[CP0_INDEX] = 0u;
  effect = (t64_cp0_effect){0};
  if (t64_cp0_prepare_tlb_instruction(value.machine, UINT32_C(0x42000008),
                                      &effect) != T64_CP0_TRANSFER_OK ||
      effect.tlb_operation != T64_CP0_TLB_PROBE || effect.tlb_duplicate ||
      effect.tlb_index != UINT32_C(5)) {
    return fixture_close(&value, false);
  }
  t64_cp0_commit_transfer(value.machine, &effect);
  if ((value.machine->cpu.cp0[CP0_INDEX] & UINT64_C(0x8000001f)) != 5u) {
    return fixture_close(&value, false);
  }
  value.machine->cpu.cp0[CP0_ENTRYHI] = UINT64_C(0x8003);
  effect = (t64_cp0_effect){0};
  if (t64_cp0_prepare_tlb_instruction(value.machine, UINT32_C(0x42000008),
                                      &effect) != T64_CP0_TRANSFER_OK ||
      !effect.write_cp0 || effect.tlb_index != UINT32_MAX) {
    return fixture_close(&value, false);
  }
  t64_cp0_commit_transfer(value.machine, &effect);
  if ((value.machine->cpu.cp0[CP0_INDEX] & INDEX_PROBE) == 0u) {
    return fixture_close(&value, false);
  }

  value.machine->cpu.cp0[CP0_RANDOM] = UINT64_C(7);
  value.machine->cpu.cp0[CP0_WIRED] = UINT64_C(3);
  value.machine->cpu.cp0[CP0_ENTRYHI] = UINT64_C(0x10003);
  value.machine->cpu.cp0[CP0_PAGEMASK] = 0u;
  value.machine->cpu.cp0[CP0_ENTRYLO0] =
      entry_lo(UINT64_C(0x08000000), 3u, true, true, false);
  value.machine->cpu.cp0[CP0_ENTRYLO1] =
      entry_lo(UINT64_C(0x08001000), 3u, true, true, false);
  effect = (t64_cp0_effect){0};
  if (t64_cp0_prepare_tlb_instruction(value.machine, UINT32_C(0x42000006),
                                      &effect) != T64_CP0_TRANSFER_OK ||
      effect.tlb_operation != T64_CP0_TLB_WRITE_RANDOM ||
      effect.tlb_index != UINT32_C(7)) {
    return fixture_close(&value, false);
  }
  saved = value.machine->cpu.tlb[7];
  t64_cp0_commit_transfer(value.machine, &effect);
  if (value.machine->cpu.tlb[7].vpn2 != UINT64_C(0x10000) ||
      value.machine->cpu.tlb[7].entry_lo0 == saved.entry_lo0 ||
      value.machine->cpu.tlb[6].vpn2 != 0u) {
    return fixture_close(&value, false);
  }
  t64_cp0_instruction_retired(value.machine);
  if (value.machine->cpu.cp0[CP0_RANDOM] != UINT64_C(6)) {
    return fixture_close(&value, false);
  }
  value.machine->cpu.cp0[CP0_RANDOM] = UINT64_C(4);
  t64_cp0_instruction_retired(value.machine);
  if (value.machine->cpu.cp0[CP0_RANDOM] != UINT64_C(3)) {
    return fixture_close(&value, false);
  }
  t64_cp0_instruction_retired(value.machine);
  if (value.machine->cpu.cp0[CP0_RANDOM] != UINT64_C(31)) {
    return fixture_close(&value, false);
  }

  saved = value.machine->cpu.tlb[6];
  value.machine->cpu.cp0[CP0_INDEX] = UINT64_C(6);
  value.machine->cpu.cp0[CP0_PAGEMASK] = UINT64_C(0x2000);
  effect = (t64_cp0_effect){0};
  if (t64_cp0_prepare_tlb_instruction(value.machine, UINT32_C(0x42000002),
                                      &effect) !=
          T64_CP0_TRANSFER_UNSUPPORTED ||
      memcmp(&saved, &value.machine->cpu.tlb[6], sizeof saved) != 0) {
    return fixture_close(&value, false);
  }
  okay = true;
  return fixture_close(&value, okay);
}

static bool step_load_fault(fixture *value, uint64_t address,
                            uint32_t instruction, bool expect_fetch_fault) {
  t64_machine *machine = value->machine;
  t64_status status;

  machine->cpu.pc = UINT32_C(0x80001000);
  machine->cpu.pc_full = UINT64_C(0xffffffff80001000);
  machine->ram[0x1000u] = (uint8_t)(instruction >> 24);
  machine->ram[0x1001u] = (uint8_t)(instruction >> 16);
  machine->ram[0x1002u] = (uint8_t)(instruction >> 8);
  machine->ram[0x1003u] = (uint8_t)instruction;
  machine->cpu.gpr[9] = address;
  machine->cpu.gpr[8] = UINT64_C(0xfeedface);
  if (expect_fetch_fault) {
    machine->cpu.pc = (uint32_t)address;
    machine->cpu.pc_full = address;
  }
  status = t64_cpu_step(machine, NULL);
  return status == T64_STATUS_OK && machine->cpu.pending_exception_valid;
}

static void report_exception_state(const char *label,
                                   const t64_machine *machine) {
  (void)printf("# %s status=%08x pc=%016llx cause=%08x bad=%016llx "
               "entryhi=%016llx context=%016llx xcontext=%016llx "
               "epc=%016llx pending=%u\n",
               label, (uint32_t)machine->cpu.cp0[CP0_STATUS],
               (unsigned long long)machine->cpu.pc_full,
               (uint32_t)machine->cpu.cp0[CP0_CAUSE],
               (unsigned long long)machine->cpu.cp0[CP0_BADVADDR],
               (unsigned long long)machine->cpu.cp0[CP0_ENTRYHI],
               (unsigned long long)machine->cpu.cp0[CP0_CONTEXT],
               (unsigned long long)machine->cpu.cp0[CP0_XCONTEXT],
               (unsigned long long)machine->cpu.cp0[CP0_EPC],
               machine->cpu.pending_exception_valid ? 1u : 0u);
}

static bool test_exception_vector_and_context_policy(void) {
  fixture value = {0};
  bool okay = false;

  if (!fixture_open(&value, 0u, 0u)) {
    return fixture_close(&value, false);
  }
  if (!step_load_fault(&value, UINT64_C(0x2000), UINT32_C(0x8d280000),
                       false) ||
      value.machine->cpu.pc_full != UINT64_C(0xffffffff80000000) ||
      ((uint32_t)value.machine->cpu.cp0[CP0_CAUSE] & CAUSE_EXCCODE_MASK) !=
          UINT32_C(8) ||
      value.machine->cpu.cp0[CP0_BADVADDR] != UINT64_C(0x2000) ||
      value.machine->cpu.cp0[CP0_ENTRYHI] != UINT64_C(0x2000) ||
      value.machine->cpu.cp0[CP0_CONTEXT] != UINT64_C(0x10) ||
      value.machine->cpu.cp0[CP0_XCONTEXT] != UINT64_C(0x10)) {
    report_exception_state("normal refill", value.machine);
    return fixture_close(&value, false);
  }
  fixture_close(&value, true);

  value = (fixture){0};
  if (!fixture_open(&value, STATUS_BEV, 0u) ||
      !step_load_fault(&value, UINT64_C(0x2000), UINT32_C(0x8d280000),
                       false) ||
      value.machine->cpu.pc_full != UINT64_C(0xffffffffbfc00200)) {
    if (value.machine != NULL) {
      report_exception_state("BEV refill", value.machine);
    }
    return fixture_close(&value, false);
  }
  fixture_close(&value, true);

  value = (fixture){0};
  if (!fixture_open(&value, UINT32_C(0x80), 0u) ||
      !step_load_fault(&value, UINT64_C(0x4000), UINT32_C(0x8d280000),
                       false) ||
      value.machine->cpu.pc_full != UINT64_C(0xffffffff80000080) ||
      value.machine->cpu.cp0[CP0_XCONTEXT] != UINT64_C(0x20)) {
    if (value.machine != NULL) {
      report_exception_state("XTLB refill", value.machine);
    }
    return fixture_close(&value, false);
  }
  fixture_close(&value, true);

  value = (fixture){0};
  if (!fixture_open(&value, STATUS_BEV | UINT32_C(0x80), 0u) ||
      !step_load_fault(&value, UINT64_C(0x4000), UINT32_C(0x8d280000),
                       false) ||
      value.machine->cpu.pc_full != UINT64_C(0xffffffffbfc00280)) {
    if (value.machine != NULL) {
      report_exception_state("BEV XTLB refill", value.machine);
    }
    return fixture_close(&value, false);
  }
  if (!fixture_close(&value, true)) {
    return false;
  }

  value = (fixture){0};
  if (!fixture_open(&value, UINT32_C(0x80), 0u) ||
      !step_load_fault(&value, UINT64_C(0x4000000000004000),
                       UINT32_C(0x8d280000), false) ||
      value.machine->cpu.pc_full != UINT64_C(0xffffffff80000080) ||
      value.machine->cpu.cp0[CP0_XCONTEXT] != UINT64_C(0x80000020)) {
    if (value.machine != NULL) {
      report_exception_state("supervisor-region XTLB refill", value.machine);
    }
    return fixture_close(&value, false);
  }
  if (!fixture_close(&value, true)) {
    return false;
  }

  value = (fixture){0};
  if (!fixture_open(&value, STATUS_EXL, 0u)) {
    return fixture_close(&value, false);
  }
  value.machine->cpu.cp0[CP0_EPC] = UINT64_C(0x12345678);
  value.machine->cpu.cp0[CP0_CAUSE] = UINT32_C(0x80000000);
  if (!step_load_fault(&value, UINT64_C(0x2000), UINT32_C(0x8d280000),
                       false) ||
      value.machine->cpu.pc_full != UINT64_C(0xffffffff80000180) ||
      value.machine->cpu.cp0[CP0_EPC] != UINT64_C(0x12345678) ||
      ((uint32_t)value.machine->cpu.cp0[CP0_CAUSE] & UINT32_C(0x8000007c)) !=
          UINT32_C(0x80000008) ||
      value.machine->cpu.cp0[CP0_ENTRYHI] != UINT64_C(0x2000)) {
    report_exception_state("nested EXL refill", value.machine);
    return fixture_close(&value, false);
  }
  fixture_close(&value, true);

  value = (fixture){0};
  {
    const uint64_t kuseg_alias = UINT64_C(0xffffffffe0002000);
    uint64_t expected_xcontext =
        ((kuseg_alias >> 9) & UINT64_C(0x000000007ffffff0)) |
        ((kuseg_alias >> 31) & UINT64_C(0x0000000180000000));

    if (!fixture_open(&value, 0u, 0u) ||
        !step_load_fault(&value, kuseg_alias, UINT32_C(0x8d280000), false) ||
        value.machine->cpu.pc_full != UINT64_C(0xffffffff80000000) ||
        value.machine->cpu.cp0[CP0_BADVADDR] != kuseg_alias ||
        value.machine->cpu.cp0[CP0_ENTRYHI] != UINT64_C(0xe0002000) ||
        value.machine->cpu.cp0[CP0_XCONTEXT] != expected_xcontext) {
      if (value.machine != NULL) {
        report_exception_state("32-bit kernel XContext region", value.machine);
      }
      return fixture_close(&value, false);
    }
  }
  if (!fixture_close(&value, true)) {
    return false;
  }

  value = (fixture){0};
  if (!fixture_open(&value, 0u, 0u)) {
    return fixture_close(&value, false);
  }
  set_entry(value.machine, 0u, 0u, UINT64_C(0x2000), 0u, 0u, 0u);
  if (!step_load_fault(&value, UINT64_C(0x2000), UINT32_C(0x8d280000),
                       false) ||
      value.machine->cpu.pc_full != UINT64_C(0xffffffff80000180) ||
      ((uint32_t)value.machine->cpu.cp0[CP0_CAUSE] & CAUSE_EXCCODE_MASK) !=
          UINT32_C(8) ||
      value.machine->cpu.gpr[8] != UINT64_C(0xfeedface)) {
    report_exception_state("invalid translation", value.machine);
    return fixture_close(&value, false);
  }
  fixture_close(&value, true);

  value = (fixture){0};
  if (!fixture_open(&value, 0u, 0u)) {
    return fixture_close(&value, false);
  }
  set_entry(value.machine, 0u, 0u, UINT64_C(0x2000), 0u,
            entry_lo(UINT64_C(0x1000), 3u, true, false, false),
            entry_lo(UINT64_C(0x2000), 3u, true, false, false));
  if (!step_load_fault(&value, UINT64_C(0x2000), UINT32_C(0xad280000),
                       false) ||
      value.machine->cpu.pc_full != UINT64_C(0xffffffff80000180) ||
      ((uint32_t)value.machine->cpu.cp0[CP0_CAUSE] & CAUSE_EXCCODE_MASK) !=
          UINT32_C(4) ||
      value.machine->ram[0x2000u] != 0u) {
    report_exception_state("modified store", value.machine);
    return fixture_close(&value, false);
  }
  fixture_close(&value, true);

  value = (fixture){0};
  if (!fixture_open(&value, 0u, 0u) ||
      !step_load_fault(&value, UINT64_C(0x80000000),
                       UINT32_C(0x8d280000), false) ||
      value.machine->cpu.pc_full != UINT64_C(0xffffffff80000180) ||
      ((uint32_t)value.machine->cpu.cp0[CP0_CAUSE] & CAUSE_EXCCODE_MASK) !=
          UINT32_C(16) ||
      value.machine->cpu.cp0[CP0_BADVADDR] != UINT64_C(0x80000000)) {
    if (value.machine != NULL) {
      report_exception_state("address fault", value.machine);
    }
    return fixture_close(&value, false);
  }
  if (!fixture_close(&value, true)) {
    return false;
  }

  value = (fixture){0};
  if (!fixture_open(&value, 0u, 0u)) {
    return fixture_close(&value, false);
  }
  value.machine->cpu.pc = UINT32_C(0x2000);
  value.machine->cpu.pc_full = UINT64_C(0x2000);
  if (!step_load_fault(&value, UINT64_C(0x2000), UINT32_C(0), true) ||
      value.machine->cpu.pc_full != UINT64_C(0xffffffff80000000) ||
      value.machine->cpu.cp0[CP0_BADVADDR] != UINT64_C(0x2000) ||
      ((uint32_t)value.machine->cpu.cp0[CP0_CAUSE] & CAUSE_EXCCODE_MASK) !=
          UINT32_C(8)) {
    report_exception_state("fetch refill", value.machine);
    return fixture_close(&value, false);
  }
  okay = true;
  return fixture_close(&value, okay);
}

static bool test_supported_cache_operand_uses_tlb_translation(void) {
  fixture value = {0};
  bool okay = false;
  const uint32_t cache_instruction = UINT32_C(0xbd150000);
  t64_status status;

  if (!fixture_open(&value, 0u, 1u)) {
    return fixture_close(&value, false);
  }
  /* CACHE Hit_Writeback_Invalidate_D translates its virtual operand before
   * executing; the manual allows TLB miss/invalid exceptions for CACHE. */
  value.machine->cpu.pc = UINT32_C(0x80001000);
  value.machine->cpu.pc_full = UINT64_C(0xffffffff80001000);
  value.machine->ram[0x1000u] = (uint8_t)(cache_instruction >> 24);
  value.machine->ram[0x1001u] = (uint8_t)(cache_instruction >> 16);
  value.machine->ram[0x1002u] = (uint8_t)(cache_instruction >> 8);
  value.machine->ram[0x1003u] = (uint8_t)cache_instruction;
  value.machine->cpu.gpr[8] = UINT64_C(0x100);
  value.machine->cpu.linked_valid = true;
  value.machine->cpu.linked_width = UINT32_C(4);
  value.machine->cpu.linked_physical_address = UINT32_C(0x80);
  status = t64_cpu_step(value.machine, NULL);
  if (status != T64_STATUS_OK ||
      !value.machine->cpu.pending_exception_valid ||
      value.machine->cpu.pc_full != UINT64_C(0xffffffff80000000) ||
      ((uint32_t)value.machine->cpu.cp0[CP0_CAUSE] & CAUSE_EXCCODE_MASK) !=
          UINT32_C(8) ||
      value.machine->cpu.cp0[CP0_BADVADDR] != UINT64_C(0x100) ||
      value.machine->cpu.cp0[CP0_ENTRYHI] != UINT64_C(1) ||
      value.machine->cpu.retired_instructions != 0u ||
      value.machine->cache.operation_seen ||
      !value.machine->cpu.linked_valid ||
      value.machine->cpu.linked_width != UINT32_C(4) ||
      value.machine->cpu.linked_physical_address != UINT32_C(0x80)) {
    report_exception_state("CACHE TLB refill", value.machine);
    return fixture_close(&value, false);
  }
  okay = true;
  return fixture_close(&value, okay);
}

static bool read_seed_asset(uint8_t bytes[IPL3_ASSET_BYTES]) {
  const char *path = getenv("T64_TEST_IPL3_PATH");
  FILE *file;
  size_t length;
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
  length = fread(bytes, 1u, IPL3_ASSET_BYTES, file);
  extra = fgetc(file);
  read_error = ferror(file);
  close_error = fclose(file);
  return read_error == 0 && close_error == 0 &&
         length == IPL3_ASSET_BYTES && extra == EOF;
}

static bool test_invalid_seed_tags_shutdown_until_reset(void) {
  fixture value = {0};
  t64_mmu_request request = {UINT64_C(0x1000), 4u, T64_MMU_ACCESS_READ};
  t64_mmu_result result = {0};
  uint8_t asset[IPL3_ASSET_BYTES];
  uint64_t destination_before;
  bool okay = false;
  uint32_t entry_index = UINT32_MAX;
  bool duplicate = false;

  if (!fixture_open(&value, 0u, 0u)) {
    return fixture_close(&value, false);
  }
  /* The prepared seed has 32 zero tags. V=0 is checked after tag and ASID
   * matching, so 0x1000 matches all 32 pairs and must never select one. */
  for (size_t index = 0u; index < T64_VR4300_TLB_ENTRY_COUNT; index++) {
    value.machine->cpu.tlb[index] = (t64_vr4300_tlb_pair){0};
  }
  if (!t64_mmu_probe(value.machine, UINT64_C(0x1000), &entry_index,
                     &duplicate) ||
      !duplicate || entry_index != UINT32_MAX ||
      !t64_mmu_translate(value.machine, &request, &result) ||
      result.fault != T64_MMU_FAULT_DUPLICATE_MATCH ||
      result.matched_entry != UINT32_MAX) {
    return fixture_close(&value, false);
  }
  value.machine->cpu.pc = UINT32_C(0x80001000);
  value.machine->cpu.pc_full = UINT64_C(0xffffffff80001000);
  value.machine->ram[0x1000u] = UINT8_C(0x8d);
  value.machine->ram[0x1001u] = UINT8_C(0x28);
  value.machine->ram[0x1002u] = UINT8_C(0x00);
  value.machine->ram[0x1003u] = UINT8_C(0x00);
  value.machine->cpu.gpr[9] = UINT64_C(0x1000);
  value.machine->cpu.gpr[8] = UINT64_C(0xfeedface);
  destination_before = value.machine->cpu.gpr[8];
  if (t64_cpu_step(value.machine, NULL) != T64_STATUS_RESOURCE_EXHAUSTED ||
      !value.machine->cpu.tlb_shutdown || !value.machine->cpu.stopped ||
      ((uint32_t)value.machine->cpu.cp0[CP0_STATUS] & STATUS_TS) == 0u ||
      value.machine->cpu.gpr[8] != destination_before ||
      t64_cpu_step(value.machine, NULL) != T64_STATUS_RESOURCE_EXHAUSTED ||
      !value.machine->cpu.tlb_shutdown ||
      value.machine->cpu.gpr[8] != destination_before) {
    return fixture_close(&value, false);
  }
  if (!read_seed_asset(asset) ||
      t64_machine_admit_rom(value.machine, asset, sizeof asset) !=
          T64_STATUS_OK ||
      value.machine->cpu.tlb_shutdown ||
      ((uint32_t)value.machine->cpu.cp0[CP0_STATUS] & STATUS_TS) != 0u) {
    return fixture_close(&value, false);
  }
  for (size_t index = 0u; index < T64_VR4300_TLB_ENTRY_COUNT; index++) {
    if (value.machine->cpu.tlb[index].page_mask != 0u ||
        value.machine->cpu.tlb[index].vpn2 != 0u ||
        value.machine->cpu.tlb[index].entry_lo0 != 0u ||
        value.machine->cpu.tlb[index].entry_lo1 != 0u) {
      return fixture_close(&value, false);
    }
  }
  {
    t64_cp0_effect effect = {0};

    value.machine->cpu.cp0[CP0_ENTRYHI] = UINT64_C(0x1000);
    if (t64_cp0_prepare_tlb_instruction(value.machine,
                                        UINT32_C(0x42000008), &effect) !=
            T64_CP0_TRANSFER_OK ||
        !effect.tlb_duplicate || effect.tlb_index != UINT32_MAX ||
        (effect.cp0_value & INDEX_PROBE) == 0u) {
      return fixture_close(&value, false);
    }
    t64_cp0_commit_transfer(value.machine, &effect);
    if (!value.machine->cpu.tlb_shutdown ||
        ((uint32_t)value.machine->cpu.cp0[CP0_STATUS] & STATUS_TS) == 0u ||
        (value.machine->cpu.cp0[CP0_INDEX] & INDEX_PROBE) == 0u) {
      return fixture_close(&value, false);
    }
    if (t64_machine_admit_rom(value.machine, asset, sizeof asset) !=
            T64_STATUS_OK ||
        value.machine->cpu.tlb_shutdown ||
        ((uint32_t)value.machine->cpu.cp0[CP0_STATUS] & STATUS_TS) != 0u) {
      return fixture_close(&value, false);
    }
  }
  okay = true;
  return fixture_close(&value, okay);
}

int main(void) {
  static const mmu_test_case cases[] = {
      {"all seven paired page sizes translate both halves and boundaries",
       test_all_page_masks_and_pair_boundaries},
      {"ASID, both-half global, V-before-D and fetch permissions",
       test_asid_global_and_access_priority},
      {"32-bit and extended segment privilege boundaries and physical domains",
       test_segment_boundaries_and_privilege_modes},
      {"refill, EXL, invalid and modified fault context is distinguished",
       test_fault_context_and_priority},
      {"extended region tags and all TLB cache algorithms are preserved",
       test_extended_regions_and_cache_attributes},
      {"TLBP, TLBR, TLBWI, TLBWR and invalid masks preserve CP0 policy",
       test_tlb_management_and_invalid_mask_refusal},
      {"refill and XTLB vectors commit context and preserve nested EPC/BD",
       test_exception_vector_and_context_policy},
      {"supported CACHE operand uses the new-profile TLB translation",
       test_supported_cache_operand_uses_tlb_translation},
      {"zero invalid seed tags cause duplicate shutdown until reset",
       test_invalid_seed_tags_shutdown_until_reset},
  };
  size_t failures = 0u;

  (void)printf("1..%zu\n", sizeof cases / sizeof cases[0]);
  for (size_t index = 0u; index < sizeof cases / sizeof cases[0]; index++) {
    bool okay = cases[index].run();

    (void)printf("%s %zu - %s\n", okay ? "ok" : "not ok", index + 1u,
                 cases[index].name);
    if (!okay) {
      failures++;
    }
  }
  return failures == 0u ? 0 : 1;
}
