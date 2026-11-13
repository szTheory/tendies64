#include "t64_internal.h"

#include <stdbool.h>
#include <stdint.h>

#define T64_CP0_ENTRYHI UINT32_C(10)
#define T64_CP0_STATUS UINT32_C(12)
#define T64_CP0_CONFIG UINT32_C(16)

#define T64_STATUS_EXL UINT32_C(0x00000002)
#define T64_STATUS_ERL UINT32_C(0x00000004)
#define T64_STATUS_KSU UINT32_C(0x00000018)
#define T64_STATUS_UX UINT32_C(0x00000020)
#define T64_STATUS_SX UINT32_C(0x00000040)
#define T64_STATUS_KX UINT32_C(0x00000080)

#define T64_ENTRYHI_ASID_MASK UINT32_C(0x000000ff)
#define T64_ENTRYLO_PFN_MASK UINT32_C(0x03ffffc0)
#define T64_ENTRYLO_CACHE_MASK UINT32_C(0x00000038)
#define T64_ENTRYLO_DIRTY UINT32_C(0x00000004)
#define T64_ENTRYLO_VALID UINT32_C(0x00000002)
#define T64_ENTRYLO_GLOBAL UINT32_C(0x00000001)
#define T64_ENTRYHI_REGION_MASK UINT64_C(0xc000000000000000)
#define T64_ENTRYHI_VPN_MASK UINT64_C(0x000000ffffffe000)
#define T64_ADDRESS32_MASK UINT64_C(0x00000000ffffffff)
#define T64_XKPHYS_PHYSICAL_HIGH_MASK UINT64_C(0x07ffffff00000000)
#define T64_PAGEMASK_FIELD UINT32_C(0x01ffe000)

typedef enum {
  T64_ADDRESS_MODE_USER = 0,
  T64_ADDRESS_MODE_SUPERVISOR = 1,
  T64_ADDRESS_MODE_KERNEL = 2
} t64_address_mode;

typedef enum {
  T64_SEGMENT_INVALID = 0,
  T64_SEGMENT_TLB = 1,
  T64_SEGMENT_DIRECT_CACHED = 2,
  T64_SEGMENT_DIRECT_UNCACHED = 3,
  T64_SEGMENT_XKPHYS = 4
} t64_segment_kind;

static bool address_mode(const t64_machine *machine,
                         t64_address_mode *mode_out,
                         bool *extended_out) {
  uint32_t status;
  uint32_t ksu;

  if (machine == NULL || mode_out == NULL || extended_out == NULL) {
    return false;
  }
  status = (uint32_t)machine->cpu.cp0[T64_CP0_STATUS];
  ksu = (status & T64_STATUS_KSU) >> 3;
  if ((status & (T64_STATUS_EXL | T64_STATUS_ERL)) != 0u || ksu == 0u) {
    *mode_out = T64_ADDRESS_MODE_KERNEL;
    *extended_out = (status & T64_STATUS_KX) != 0u;
    return true;
  }
  if (ksu == 1u) {
    *mode_out = T64_ADDRESS_MODE_SUPERVISOR;
    *extended_out = (status & T64_STATUS_SX) != 0u;
    return true;
  }
  if (ksu == 2u) {
    *mode_out = T64_ADDRESS_MODE_USER;
    *extended_out = (status & T64_STATUS_UX) != 0u;
    return true;
  }
  return false;
}

static bool is_sign_extended_32(uint64_t address) {
  uint32_t low = (uint32_t)address;
  uint64_t canonical = (uint64_t)low;

  if ((low & UINT32_C(0x80000000)) != 0u) {
    canonical |= UINT64_C(0xffffffff00000000);
  }
  return address == canonical;
}

static t64_segment_kind segment_32(uint64_t address,
                                   t64_address_mode mode) {
  uint32_t low;

  if (!is_sign_extended_32(address)) {
    return T64_SEGMENT_INVALID;
  }
  low = (uint32_t)address;
  if (mode == T64_ADDRESS_MODE_USER) {
    return low < UINT32_C(0x80000000) ? T64_SEGMENT_TLB
                                      : T64_SEGMENT_INVALID;
  }
  if (mode == T64_ADDRESS_MODE_SUPERVISOR) {
    return low < UINT32_C(0x80000000) ||
                   (low >= UINT32_C(0xc0000000) &&
                    low < UINT32_C(0xe0000000))
               ? T64_SEGMENT_TLB
               : T64_SEGMENT_INVALID;
  }
  if (low >= UINT32_C(0x80000000) && low < UINT32_C(0xa0000000)) {
    return T64_SEGMENT_DIRECT_CACHED;
  }
  if (low >= UINT32_C(0xa0000000) && low < UINT32_C(0xc0000000)) {
    return T64_SEGMENT_DIRECT_UNCACHED;
  }
  return T64_SEGMENT_TLB;
}

static t64_segment_kind segment_64(uint64_t address,
                                   t64_address_mode mode) {
  if (mode == T64_ADDRESS_MODE_USER) {
    return address < UINT64_C(0x0000010000000000) ? T64_SEGMENT_TLB
                                                  : T64_SEGMENT_INVALID;
  }
  if (mode == T64_ADDRESS_MODE_SUPERVISOR) {
    if (address < UINT64_C(0x0000010000000000) ||
        (address >= UINT64_C(0x4000000000000000) &&
         address < UINT64_C(0x4000010000000000)) ||
        (address >= UINT64_C(0xffffffffc0000000) &&
         address <= UINT64_C(0xffffffffdfffffff))) {
      return T64_SEGMENT_TLB;
    }
    return T64_SEGMENT_INVALID;
  }

  if (address < UINT64_C(0x0000010000000000) ||
      (address >= UINT64_C(0x4000000000000000) &&
       address < UINT64_C(0x4000010000000000)) ||
      (address >= UINT64_C(0xc000000000000000) &&
       address <= UINT64_C(0xc00000ff7fffffff))) {
    return T64_SEGMENT_TLB;
  }
  if ((address >> 62) == UINT64_C(2)) {
    return T64_SEGMENT_XKPHYS;
  }
  if (address >= UINT64_C(0xffffffff80000000) &&
      address <= UINT64_C(0xffffffff9fffffff)) {
    return T64_SEGMENT_DIRECT_CACHED;
  }
  if (address >= UINT64_C(0xffffffffa0000000) &&
      address <= UINT64_C(0xffffffffbfffffff)) {
    return T64_SEGMENT_DIRECT_UNCACHED;
  }
  if (address >= UINT64_C(0xffffffffc0000000)) {
    return T64_SEGMENT_TLB;
  }
  return T64_SEGMENT_INVALID;
}

bool t64_mmu_page_mask_is_defined(uint64_t page_mask) {
  switch ((uint32_t)page_mask & T64_PAGEMASK_FIELD) {
  case UINT32_C(0x00000000):
  case UINT32_C(0x00006000):
  case UINT32_C(0x0001e000):
  case UINT32_C(0x0007e000):
  case UINT32_C(0x001fe000):
  case UINT32_C(0x007fe000):
  case UINT32_C(0x01ffe000): return true;
  default: return false;
  }
}

uint64_t t64_mmu_compat_normalize_page_mask(uint64_t page_mask) {
  uint32_t field = (uint32_t)page_mask & T64_PAGEMASK_FIELD;
  uint32_t normalized = 0u;

  for (uint32_t pair = 0u; pair < 6u; pair++) {
    uint32_t lower_bit = UINT32_C(1) << (13u + pair * 2u);
    uint32_t upper_bit = UINT32_C(1) << (14u + pair * 2u);

    if ((field & upper_bit) != 0u) {
      normalized |= lower_bit | upper_bit;
    }
  }
  return (uint64_t)normalized;
}

static uint32_t page_size_shift(uint64_t page_mask) {
  switch ((uint32_t)page_mask & UINT32_C(0x01ffe000)) {
  case UINT32_C(0x00000000): return 12u;
  case UINT32_C(0x00006000): return 14u;
  case UINT32_C(0x0001e000): return 16u;
  case UINT32_C(0x0007e000): return 18u;
  case UINT32_C(0x001fe000): return 20u;
  case UINT32_C(0x007fe000): return 22u;
  case UINT32_C(0x01ffe000): return 24u;
  default: return 0u;
  }
}

static bool entry_matches(const t64_machine *machine,
                          const t64_vr4300_tlb_pair *entry,
                          uint64_t address, bool extended,
                          uint32_t asid) {
  uint32_t page_shift = page_size_shift(entry->page_mask);
  uint64_t page_pair_size;
  uint64_t address_key;
  uint64_t entry_key;
  uint64_t compare_mask;
  bool global;

  (void)machine;
  if (page_shift == 0u) {
    return false;
  }
  page_pair_size = UINT64_C(1) << (page_shift + 1u);
  if (extended) {
    address_key = (address & T64_ENTRYHI_REGION_MASK) |
                  (address & T64_ENTRYHI_VPN_MASK);
    entry_key = ((entry->region << 62) & T64_ENTRYHI_REGION_MASK) |
                 (entry->vpn2 & T64_ENTRYHI_VPN_MASK);
    compare_mask = T64_ENTRYHI_REGION_MASK |
                   (T64_ENTRYHI_VPN_MASK & ~(page_pair_size - 1u));
  } else {
    address_key = (uint32_t)address;
    entry_key = (uint32_t)entry->vpn2;
    compare_mask = T64_ADDRESS32_MASK & ~(page_pair_size - 1u);
  }
  global = ((entry->entry_lo0 & T64_ENTRYLO_GLOBAL) != 0u) &&
           ((entry->entry_lo1 & T64_ENTRYLO_GLOBAL) != 0u);
  return (address_key & compare_mask) == (entry_key & compare_mask) &&
         (global || entry->asid == asid);
}

bool t64_mmu_probe(const t64_machine *machine, uint64_t entry_hi,
                   uint32_t *entry_index_out, bool *duplicate_out) {
  t64_address_mode mode;
  bool extended;
  uint32_t asid = (uint32_t)entry_hi & T64_ENTRYHI_ASID_MASK;
  uint32_t found = UINT32_MAX;
  uint32_t match_count = 0u;
  uint32_t index;

  if (machine == NULL || entry_index_out == NULL || duplicate_out == NULL ||
      !address_mode(machine, &mode, &extended)) {
    return false;
  }
  (void)mode;
  *entry_index_out = UINT32_MAX;
  *duplicate_out = false;
  for (index = 0u; index < T64_VR4300_TLB_ENTRY_COUNT; index++) {
    const t64_vr4300_tlb_pair *entry = &machine->cpu.tlb[index];

    if (!t64_mmu_page_mask_is_defined(entry->page_mask)) {
      return false;
    }
    if (entry_matches(machine, entry, entry_hi, extended, asid)) {
      found = index;
      match_count++;
    }
  }
  if (match_count == 1u) {
    *entry_index_out = found;
  }
  *duplicate_out = match_count > 1u;
  return true;
}

static void set_address_fault(const t64_mmu_request *request,
                              t64_mmu_result *result) {
  result->fault = T64_MMU_FAULT_ADDRESS;
  result->translated = false;
  result->virtual_address = request->virtual_address;
}

bool t64_mmu_translate(const t64_machine *machine,
                       const t64_mmu_request *request,
                       t64_mmu_result *result_out) {
  t64_mmu_result result = {0};
  t64_address_mode mode;
  t64_segment_kind segment;
  bool extended;
  uint64_t address;
  uint32_t asid;
  uint32_t index;
  uint32_t match_count = 0u;
  const t64_vr4300_tlb_pair *match = NULL;

  if (machine == NULL || request == NULL || result_out == NULL ||
      (request->kind != T64_MMU_ACCESS_FETCH &&
       request->kind != T64_MMU_ACCESS_READ &&
       request->kind != T64_MMU_ACCESS_WRITE)) {
    return false;
  }
  result.width = request->width;
  result.physical_domain = T64_MMU_DOMAIN_BUS;
  result.matched_entry = UINT32_MAX;
  result.virtual_address = request->virtual_address;
  if (request->width != 1u && request->width != 2u &&
      request->width != 4u && request->width != 8u) {
    set_address_fault(request, &result);
    *result_out = result;
    return true;
  }
  if ((request->kind == T64_MMU_ACCESS_FETCH &&
       (request->virtual_address & UINT64_C(3)) != 0u) ||
      (request->kind != T64_MMU_ACCESS_FETCH && request->width > 1u &&
       (request->virtual_address & ((uint64_t)request->width - 1u)) != 0u)) {
    set_address_fault(request, &result);
    *result_out = result;
    return true;
  }
  if (!address_mode(machine, &mode, &extended)) {
    set_address_fault(request, &result);
    *result_out = result;
    return true;
  }
  result.extended_address_mode = extended;
  address = request->virtual_address;
  segment = extended ? segment_64(address, mode) : segment_32(address, mode);
  if (segment == T64_SEGMENT_INVALID) {
    set_address_fault(request, &result);
    *result_out = result;
    return true;
  }
  if (segment == T64_SEGMENT_DIRECT_CACHED ||
      segment == T64_SEGMENT_DIRECT_UNCACHED) {
    uint32_t low = (uint32_t)address;
    uint32_t cache_algorithm = segment == T64_SEGMENT_DIRECT_UNCACHED
                                   ? UINT32_C(2)
                                   : (uint32_t)machine->cpu.cp0[
                                         T64_CP0_CONFIG] & UINT32_C(7);

    result.physical_address = (uint64_t)(low & UINT32_C(0x1fffffff));
    result.cache_algorithm = (uint8_t)cache_algorithm;
    result.cacheable = cache_algorithm != UINT32_C(2);
    result.translated = true;
  } else if (segment == T64_SEGMENT_XKPHYS) {
    uint32_t cache_algorithm =
        (uint32_t)((address >> 59) & UINT64_C(7));

    result.physical_domain = T64_MMU_DOMAIN_XKPHYS;
    if ((address & T64_XKPHYS_PHYSICAL_HIGH_MASK) != 0u) {
      set_address_fault(request, &result);
      *result_out = result;
      return true;
    }
    result.physical_address = address & T64_ADDRESS32_MASK;
    result.cache_algorithm = (uint8_t)cache_algorithm;
    result.cacheable = cache_algorithm != UINT32_C(2);
    result.translated = true;
  } else {
    asid = (uint32_t)machine->cpu.cp0[T64_CP0_ENTRYHI] &
           T64_ENTRYHI_ASID_MASK;
    for (index = 0u; index < T64_VR4300_TLB_ENTRY_COUNT; index++) {
      const t64_vr4300_tlb_pair *entry = &machine->cpu.tlb[index];

      if (!t64_mmu_page_mask_is_defined(entry->page_mask)) {
        set_address_fault(request, &result);
        *result_out = result;
        return true;
      }
      if (entry_matches(machine, entry, address, extended, asid)) {
        match = entry;
        result.matched_entry = index;
        match_count++;
      }
    }
    if (match_count > 1u) {
      result.fault = T64_MMU_FAULT_DUPLICATE_MATCH;
      result.matched_entry = UINT32_MAX;
      *result_out = result;
      return true;
    }
    if (match == NULL) {
      result.fault = T64_MMU_FAULT_TLB_REFILL;
      result.tlb_context_valid = true;
      result.tlb_refill_vector =
          ((uint32_t)machine->cpu.cp0[T64_CP0_STATUS] & T64_STATUS_EXL) == 0u;
      *result_out = result;
      return true;
    }
    {
      uint32_t page_shift = page_size_shift(match->page_mask);
      uint64_t page_bytes = UINT64_C(1) << page_shift;
      uint32_t odd = (address & page_bytes) != 0u ? 1u : 0u;
      uint32_t entry_lo = (uint32_t)(odd != 0u ? match->entry_lo1
                                               : match->entry_lo0);
      uint32_t physical_frame;

      result.entry_lo = entry_lo;
      if ((entry_lo & T64_ENTRYLO_VALID) == 0u) {
        result.fault = T64_MMU_FAULT_TLB_INVALID;
        result.tlb_context_valid = true;
        *result_out = result;
        return true;
      }
      if (request->kind == T64_MMU_ACCESS_WRITE &&
          (entry_lo & T64_ENTRYLO_DIRTY) == 0u) {
        result.fault = T64_MMU_FAULT_TLB_MODIFIED;
        result.tlb_context_valid = true;
        *result_out = result;
        return true;
      }
      physical_frame =
          (entry_lo & T64_ENTRYLO_PFN_MASK) >> 6;
      physical_frame &= ~((uint32_t)(page_bytes >> 12) - UINT32_C(1));
      result.physical_address = ((uint64_t)physical_frame << 12) |
                               (address & (page_bytes - UINT64_C(1)));
      result.cache_algorithm =
          (uint8_t)((entry_lo & T64_ENTRYLO_CACHE_MASK) >> 3);
      result.cacheable = result.cache_algorithm != UINT8_C(2);
      result.translated = true;
    }
  }
  if (result.translated &&
      (result.physical_address > UINT32_MAX ||
       result.physical_address + (uint64_t)request->width >
           UINT64_C(0x100000000))) {
    set_address_fault(request, &result);
  }
  *result_out = result;
  return true;
}
