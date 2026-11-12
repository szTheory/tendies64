#include "t64_internal.h"
#include "tendies64/t64_machine.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IPL3_ASSET_BYTES 4096u
#define STATUS_USER UINT32_C(0x10)
#define STATUS_EXL UINT32_C(0x02)
#define STATUS_RE UINT32_C(0x02000000)
#define STATUS_UX UINT32_C(0x20)
#define CAUSE_EXCCODE_MASK UINT32_C(0x7c)
#define CAUSE_BD UINT32_C(0x80000000)
#define WATCH_R UINT32_C(0x2)
#define WATCH_W UINT32_C(0x1)

/* Independent expectations follow NEC, VR4300/VR4305/VR4310 User's Manual,
 * U10504EJ7V0UM00: Figure 3-2 (pp. 62-63), Table 16-2/3 (pp. 367-368),
 * endian definitions (p. 365), byte/half/double RE operations (pp. 439-441,
 * 450-451, 486, 491, 501), word-merge equations/tables (pp. 463-466,
 * 519-523), doubleword merge tables (pp. 445-446, 497, 499-500), WatchLo
 * (p. 175), Watch exception (p. 198), and reverse endianness (p. 256). The
 * width-4 scalar word lane at fixed BigEndianMem=1 is a selected coherent
 * model inference corroborated by LL p. 453, LWC p. 460, and SWC p. 516;
 * LW/SW operation pseudocode omits the explicit transform. SHA-256
 * b7c1900f1707a4b238ebd82af1669e9628e4df4875ceabfa012a9d95b4716b75;
 * public source: https://hack64.net/docs/VR43XX.pdf.
 */

typedef struct {
  size_t outstanding;
} allocator_state;

typedef struct {
  allocator_state allocator;
  t64_machine *machine;
} fixture;

typedef bool (*memory_test_fn)(fixture *value);

typedef struct {
  const char *name;
  memory_test_fn run;
} memory_test_case;

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

static bool read_asset(uint8_t bytes[IPL3_ASSET_BYTES]) {
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

static uint32_t entry_lo(uint32_t physical_base, bool valid, bool dirty) {
  uint32_t result = ((physical_base >> 12) << 6) | UINT32_C(0x18);

  if (valid) {
    result |= UINT32_C(0x2);
  }
  if (dirty) {
    result |= UINT32_C(0x4);
  }
  return result;
}

static void set_entry(t64_machine *machine, uint32_t index,
                      uint64_t virtual_base, uint32_t asid,
                      uint32_t even_physical, bool even_valid,
                      bool even_dirty, uint32_t odd_physical,
                      bool odd_valid, bool odd_dirty) {
  t64_vr4300_tlb_pair *entry = &machine->cpu.tlb[index];

  *entry = (t64_vr4300_tlb_pair){0};
  entry->vpn2 = virtual_base & UINT64_C(0x000000ffffffe000);
  entry->asid = asid;
  entry->entry_lo0 = entry_lo(even_physical, even_valid, even_dirty);
  entry->entry_lo1 = entry_lo(odd_physical, odd_valid, odd_dirty);
}

static bool fixture_open(fixture *value) {
  const t64_host_allocator allocator = {
      test_allocate, test_release, &value->allocator};
  const t64_profile_descriptor *profile = NULL;
  uint8_t asset[IPL3_ASSET_BYTES];
  size_t index;

  if (value == NULL || !read_asset(asset) ||
      t64_profile_lookup("ntsc-cart-4m-openipl3-vr4300-v1", &profile) !=
          T64_STATUS_OK ||
      profile == NULL ||
      t64_machine_create(&allocator, profile, &value->machine) !=
          T64_STATUS_OK ||
      value->machine == NULL ||
      t64_machine_admit_rom(value->machine, asset, sizeof asset) !=
          T64_STATUS_OK) {
    return false;
  }
  value->machine->cpu.cp0[10] = UINT64_C(1);
  for (index = 0u; index < T64_VR4300_TLB_ENTRY_COUNT; index++) {
    value->machine->cpu.tlb[index] = (t64_vr4300_tlb_pair){0};
    value->machine->cpu.tlb[index].asid = UINT32_C(0xff);
  }
  /* The code is VA 0000 -> PA 1000; its data window is VA 1000 -> PA 2000. */
  set_entry(value->machine, 0u, 0u, 1u, UINT32_C(0x1000), true, true,
            UINT32_C(0x2000), true, true);
  set_entry(value->machine, 1u, UINT64_C(0x2000), 1u, UINT32_C(0x3000),
            true, true, UINT32_C(0x4000), true, true);
  return true;
}

static bool fixture_close(fixture *value, bool okay) {
  if (value->machine != NULL) {
    t64_machine_destroy(value->machine);
    value->machine = NULL;
  }
  if (value->allocator.outstanding != 0u) {
    (void)printf("# memory fixture leaked %zu host allocations\n",
                 value->allocator.outstanding);
    return false;
  }
  return okay;
}

static uint32_t encode_i(uint32_t operation, uint32_t rt, uint16_t immediate) {
  return (operation << 26) | (UINT32_C(4) << 21) | (rt << 16) | immediate;
}

static void write_word(t64_machine *machine, uint32_t physical,
                       uint32_t value) {
  machine->ram[physical] = (uint8_t)(value >> 24);
  machine->ram[physical + 1u] = (uint8_t)(value >> 16);
  machine->ram[physical + 2u] = (uint8_t)(value >> 8);
  machine->ram[physical + 3u] = (uint8_t)value;
}

static void write_instruction(t64_machine *machine, uint32_t opcode) {
  write_word(machine, UINT32_C(0x1000), opcode);
  /* Each run_one call installs a fresh synthetic instruction directly in RAM. */
  t64_cpu_icache_hit_invalidate(machine, UINT64_C(0), UINT32_C(0x1000));
}

static t64_status run_one(t64_machine *machine, uint32_t opcode,
                          uint32_t status, uint64_t base,
                          uint64_t value) {
  write_instruction(machine, opcode);
  machine->cpu.pc = 0u;
  machine->cpu.pc_full = 0u;
  machine->cpu.delay_slot_pending = false;
  machine->cpu.pending_exception_valid = false;
  machine->cpu.stopped = false;
  machine->cpu.attempt_outcome = T64_CPU_ATTEMPT_NONE;
  machine->cpu.cp0[12] = status;
  machine->cpu.cp0[13] = 0u;
  machine->cpu.cp0[8] = 0u;
  machine->cpu.cp0[14] = 0u;
  machine->cpu.gpr[4] = base;
  machine->cpu.gpr[8] = value;
  machine->cpu.gpr[0] = 0u;
  return t64_cpu_step(machine, NULL);
}

static uint32_t cause_code(const t64_machine *machine) {
  return ((uint32_t)machine->cpu.cp0[13] & CAUSE_EXCCODE_MASK) >> 2;
}

static bool test_reverse_endian_user_lanes(fixture *value) {
  const uint32_t status = STATUS_USER | STATUS_RE | STATUS_UX;
  uint8_t *bytes = value->machine->ram + UINT32_C(0x2000);
  t64_status result;

  (void)memcpy(bytes, (const uint8_t[]){0x11, 0x22, 0x33, 0x44,
                                        0x55, 0x66, 0x77, 0x88}, 8u);
  result = run_one(value->machine, encode_i(0x24u, 8u, 0u), status,
                   UINT64_C(0x1000), UINT64_C(0));
  if (result != T64_STATUS_OK || value->machine->cpu.gpr[8] != 0x88u) {
    (void)printf("# user RE LBU PA+7 expected=88 observed=%016llx status=%u\n",
                 (unsigned long long)value->machine->cpu.gpr[8],
                 (unsigned)result);
    return false;
  }
  result = run_one(value->machine, encode_i(0x25u, 8u, 0u), status,
                   UINT64_C(0x1000), UINT64_C(0));
  if (result != T64_STATUS_OK || value->machine->cpu.gpr[8] != 0x7788u) {
    (void)printf("# user RE LHU PA+6..7 expected=7788 observed=%016llx status=%u\n",
                 (unsigned long long)value->machine->cpu.gpr[8],
                 (unsigned)result);
    return false;
  }
  result = run_one(value->machine, encode_i(0x27u, 8u, 0u), status,
                   UINT64_C(0x1000), UINT64_C(0));
  if (result != T64_STATUS_OK ||
      value->machine->cpu.gpr[8] != UINT64_C(0x55667788)) {
    (void)printf("# user RE LW PA+4..7 expected=0000000055667788 observed=%016llx "
                 "status=%u\n",
                 (unsigned long long)value->machine->cpu.gpr[8],
                 (unsigned)result);
    return false;
  }
  result = run_one(value->machine, encode_i(0x37u, 8u, 0u), status,
                   UINT64_C(0x1000), UINT64_C(0));
  if (result != T64_STATUS_OK ||
      value->machine->cpu.gpr[8] != UINT64_C(0x1122334455667788)) {
    (void)printf("# user RE LD PA+0..7 expected=1122334455667788 observed=%016llx "
                 "status=%u\n",
                 (unsigned long long)value->machine->cpu.gpr[8],
                 (unsigned)result);
    return false;
  }

  (void)memset(bytes, 0u, 8u);
  result = run_one(value->machine, encode_i(0x28u, 8u, 0u), status,
                   UINT64_C(0x1000), UINT64_C(0x18));
  if (result != T64_STATUS_OK || bytes[7] != UINT8_C(0x18)) {
    (void)puts("# user RE SB did not write physical lane +7");
    return false;
  }
  (void)memset(bytes, 0u, 8u);
  result = run_one(value->machine, encode_i(0x29u, 8u, 0u), status,
                   UINT64_C(0x1000), UINT64_C(0x0718));
  if (result != T64_STATUS_OK || bytes[6] != UINT8_C(0x07) ||
      bytes[7] != UINT8_C(0x18)) {
    (void)puts("# user RE SH did not write physical lanes +6..7");
    return false;
  }
  (void)memset(bytes, 0u, 8u);
  result = run_one(value->machine, encode_i(0x2bu, 8u, 0u), status,
                   UINT64_C(0x1000), UINT64_C(0xa1b2c3d4e5f60718));
  if (result != T64_STATUS_OK ||
      memcmp(bytes + 4u,
             (const uint8_t[]){0xe5, 0xf6, 0x07, 0x18}, 4u) != 0) {
    (void)printf("# user RE SW lanes=%02x%02x%02x%02x status=%u\n",
                 bytes[4], bytes[5], bytes[6], bytes[7], (unsigned)result);
    return false;
  }
  (void)memset(bytes, 0u, 8u);
  result = run_one(value->machine, encode_i(0x3fu, 8u, 0u), status,
                   UINT64_C(0x1000), UINT64_C(0x0123456789abcdef));
  if (result != T64_STATUS_OK ||
      memcmp(bytes,
             (const uint8_t[]){0x01, 0x23, 0x45, 0x67,
                               0x89, 0xab, 0xcd, 0xef}, 8u) != 0) {
    (void)puts("# user RE SD did not store at physical lanes +0..7");
    return false;
  }

  (void)memcpy(bytes, (const uint8_t[]){0x11, 0x22, 0x33, 0x44,
                                        0x55, 0x66, 0x77, 0x88}, 8u);
  result = run_one(value->machine, encode_i(0x27u, 8u, 0u), STATUS_RE,
                   UINT64_C(0x1000), UINT64_C(0));
  return result == T64_STATUS_OK &&
         value->machine->cpu.gpr[8] == UINT64_C(0x11223344);
}

static bool test_aligned_integer_widths(fixture *value) {
  static const struct {
    uint32_t operation;
    uint16_t offset;
    uint64_t expected;
  } loads[] = {
      {0x20u, 0u, UINT64_C(0xffffffffffffff80)},
      {0x24u, 0u, UINT64_C(0x80)},
      {0x21u, 0u, UINT64_C(0xffffffffffff8012)},
      {0x25u, 2u, UINT64_C(0x3456)},
      {0x23u, 0u, UINT64_C(0xffffffff80123456)},
      {0x27u, 0u, UINT64_C(0x80123456)},
      {0x37u, 0u, UINT64_C(0x80123456789abcde)},
  };
  static const uint32_t source[] = {0xa1u, 0xb2c3u, 0xd4e5f607u};
  static const uint32_t offsets[] = {8u, 10u, 12u};
  static const uint32_t operations[] = {0x28u, 0x29u, 0x2bu};
  uint8_t *bytes = value->machine->ram + UINT32_C(0x2000);
  size_t index;

  (void)memcpy(bytes, (const uint8_t[]){0x80, 0x12, 0x34, 0x56,
                                        0x78, 0x9a, 0xbc, 0xde}, 8u);
  for (index = 0u; index < sizeof loads / sizeof loads[0]; index++) {
    t64_status status = run_one(
        value->machine,
        encode_i(loads[index].operation, 8u, loads[index].offset), 0u,
        UINT64_C(0x1000), UINT64_C(0));

    if (status != T64_STATUS_OK ||
        value->machine->cpu.gpr[8] != loads[index].expected) {
      (void)printf("# aligned load index=%zu observed=%016llx expected=%016llx\n",
                   index, (unsigned long long)value->machine->cpu.gpr[8],
                   (unsigned long long)loads[index].expected);
      return false;
    }
  }
  (void)memset(bytes + 8u, 0u, 24u);
  for (index = 0u; index < sizeof operations / sizeof operations[0]; index++) {
    if (run_one(value->machine,
                encode_i(operations[index], 8u, (uint16_t)offsets[index]),
                0u, UINT64_C(0x1000), source[index]) != T64_STATUS_OK) {
      return false;
    }
  }
  if (bytes[8] != UINT8_C(0xa1) || bytes[10] != UINT8_C(0xb2) ||
      bytes[11] != UINT8_C(0xc3) || bytes[12] != UINT8_C(0xd4) ||
      bytes[13] != UINT8_C(0xe5) || bytes[14] != UINT8_C(0xf6) ||
      bytes[15] != UINT8_C(0x07)) {
    (void)printf("# aligned stores bytes=%02x%02x%02x%02x%02x%02x%02x%02x\n",
                 bytes[8], bytes[9], bytes[10], bytes[11], bytes[12],
                 bytes[13], bytes[14], bytes[15]);
    return false;
  }
  (void)memset(bytes + 16u, 0u, 8u);
  if (run_one(value->machine, encode_i(0x3fu, 8u, 16u), 0u,
              UINT64_C(0x1000), UINT64_C(0x1827364554637281)) !=
          T64_STATUS_OK ||
      memcmp(bytes + 16u,
             (const uint8_t[]){0x18, 0x27, 0x36, 0x45,
                               0x54, 0x63, 0x72, 0x81}, 8u) != 0) {
    (void)puts("# aligned doubleword store did not preserve big-endian bytes");
    return false;
  }
  return true;
}

static bool test_word_merge_offsets(fixture *value) {
  static const uint32_t left_load[] = {
      UINT32_C(0x10111213), UINT32_C(0x11121344),
      UINT32_C(0x12133344), UINT32_C(0x13223344)};
  static const uint32_t right_load[] = {
      UINT32_C(0x11223310), UINT32_C(0x11221011),
      UINT32_C(0x11101112), UINT32_C(0x10111213)};
  static const uint8_t left_store[4][4] = {
      {0xa1, 0xb2, 0xc3, 0xd4}, {0x10, 0xa1, 0xb2, 0xc3},
      {0x10, 0x11, 0xa1, 0xb2}, {0x10, 0x11, 0x12, 0xa1}};
  static const uint8_t right_store[4][4] = {
      {0xd4, 0x11, 0x12, 0x13}, {0xc3, 0xd4, 0x12, 0x13},
      {0xb2, 0xc3, 0xd4, 0x13}, {0xa1, 0xb2, 0xc3, 0xd4}};
  uint8_t *bytes = value->machine->ram + UINT32_C(0x2000);
  uint32_t offset;

  for (offset = 0u; offset < 4u; offset++) {
    t64_status status;

    (void)memcpy(bytes, (const uint8_t[]){0x10, 0x11, 0x12, 0x13}, 4u);
    status = run_one(value->machine, encode_i(0x22u, 8u, (uint16_t)offset),
                     0u, UINT64_C(0x1000), UINT64_C(0x11223344));
    if (status != T64_STATUS_OK ||
        (uint32_t)value->machine->cpu.gpr[8] != left_load[offset]) {
      (void)printf("# LWL offset=%u expected=%08x observed=%08x\n", offset,
                   left_load[offset], (uint32_t)value->machine->cpu.gpr[8]);
      return false;
    }
    (void)memcpy(bytes, (const uint8_t[]){0x10, 0x11, 0x12, 0x13}, 4u);
    status = run_one(value->machine, encode_i(0x26u, 8u, (uint16_t)offset),
                     0u, UINT64_C(0x1000), UINT64_C(0x11223344));
    if (status != T64_STATUS_OK ||
        (uint32_t)value->machine->cpu.gpr[8] != right_load[offset]) {
      (void)printf("# LWR offset=%u expected=%08x observed=%08x\n", offset,
                   right_load[offset], (uint32_t)value->machine->cpu.gpr[8]);
      return false;
    }
    (void)memcpy(bytes, (const uint8_t[]){0x10, 0x11, 0x12, 0x13}, 4u);
    status = run_one(value->machine, encode_i(0x2au, 8u, (uint16_t)offset),
                     0u, UINT64_C(0x1000), UINT64_C(0xa1b2c3d4));
    if (status != T64_STATUS_OK ||
        memcmp(bytes, left_store[offset], 4u) != 0) {
      (void)printf("# SWL offset=%u observed=%02x%02x%02x%02x\n", offset,
                   bytes[0], bytes[1], bytes[2], bytes[3]);
      return false;
    }
    (void)memcpy(bytes, (const uint8_t[]){0x10, 0x11, 0x12, 0x13}, 4u);
    status = run_one(value->machine, encode_i(0x2eu, 8u, (uint16_t)offset),
                     0u, UINT64_C(0x1000), UINT64_C(0xa1b2c3d4));
    if (status != T64_STATUS_OK ||
        memcmp(bytes, right_store[offset], 4u) != 0) {
      (void)printf("# SWR offset=%u observed=%02x%02x%02x%02x\n", offset,
                   bytes[0], bytes[1], bytes[2], bytes[3]);
      return false;
    }
  }
  return true;
}

static bool test_reverse_endian_word_merge_offsets(fixture *value) {
  static const uint32_t left_load[] = {
      UINT32_C(0x17667788), UINT32_C(0x16177788),
      UINT32_C(0x15161788), UINT32_C(0x14151617),
      UINT32_C(0x13667788), UINT32_C(0x12137788),
      UINT32_C(0x11121388), UINT32_C(0x10111213)};
  static const uint32_t right_load[] = {
      UINT32_C(0x14151617), UINT32_C(0x55141516),
      UINT32_C(0x55661415), UINT32_C(0x55667714),
      UINT32_C(0x10111213), UINT32_C(0x55101112),
      UINT32_C(0x55661011), UINT32_C(0x55667710)};
  static const uint8_t left_store[8][8] = {
      {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x55},
      {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x55, 0x66},
      {0x10, 0x11, 0x12, 0x13, 0x14, 0x55, 0x66, 0x77},
      {0x10, 0x11, 0x12, 0x13, 0x55, 0x66, 0x77, 0x88},
      {0x10, 0x11, 0x12, 0x55, 0x14, 0x15, 0x16, 0x17},
      {0x10, 0x11, 0x55, 0x66, 0x14, 0x15, 0x16, 0x17},
      {0x10, 0x55, 0x66, 0x77, 0x14, 0x15, 0x16, 0x17},
      {0x55, 0x66, 0x77, 0x88, 0x14, 0x15, 0x16, 0x17}};
  static const uint8_t right_store[8][8] = {
      {0x10, 0x11, 0x12, 0x13, 0x55, 0x66, 0x77, 0x88},
      {0x10, 0x11, 0x12, 0x13, 0x66, 0x77, 0x88, 0x17},
      {0x10, 0x11, 0x12, 0x13, 0x77, 0x88, 0x16, 0x17},
      {0x10, 0x11, 0x12, 0x13, 0x88, 0x15, 0x16, 0x17},
      {0x55, 0x66, 0x77, 0x88, 0x14, 0x15, 0x16, 0x17},
      {0x66, 0x77, 0x88, 0x13, 0x14, 0x15, 0x16, 0x17},
      {0x77, 0x88, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17},
      {0x88, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17}};
  const uint32_t status = STATUS_USER | STATUS_RE | STATUS_UX;
  uint8_t *bytes = value->machine->ram + UINT32_C(0x2000);
  uint32_t offset;

  for (offset = 0u; offset < 8u; offset++) {
    t64_status result;

    (void)memcpy(bytes, (const uint8_t[]){0x10, 0x11, 0x12, 0x13,
                                          0x14, 0x15, 0x16, 0x17}, 8u);
    result = run_one(value->machine,
                     encode_i(0x22u, 8u, (uint16_t)offset), status,
                     UINT64_C(0x1000), UINT64_C(0x1122334455667788));
    if (result != T64_STATUS_OK ||
        (uint32_t)value->machine->cpu.gpr[8] != left_load[offset]) {
      (void)printf("# user RE LWL offset=%u expected=%08x observed=%08x\n",
                   offset, left_load[offset],
                   (uint32_t)value->machine->cpu.gpr[8]);
      return false;
    }
    (void)memcpy(bytes, (const uint8_t[]){0x10, 0x11, 0x12, 0x13,
                                          0x14, 0x15, 0x16, 0x17}, 8u);
    result = run_one(value->machine,
                     encode_i(0x26u, 8u, (uint16_t)offset), status,
                     UINT64_C(0x1000), UINT64_C(0x1122334455667788));
    if (result != T64_STATUS_OK ||
        (uint32_t)value->machine->cpu.gpr[8] != right_load[offset]) {
      (void)printf("# user RE LWR offset=%u expected=%08x observed=%08x\n",
                   offset, right_load[offset],
                   (uint32_t)value->machine->cpu.gpr[8]);
      return false;
    }
    (void)memcpy(bytes, (const uint8_t[]){0x10, 0x11, 0x12, 0x13,
                                          0x14, 0x15, 0x16, 0x17}, 8u);
    result = run_one(value->machine,
                     encode_i(0x2au, 8u, (uint16_t)offset), status,
                     UINT64_C(0x1000), UINT64_C(0x1122334455667788));
    if (result != T64_STATUS_OK ||
        memcmp(bytes, left_store[offset], 8u) != 0) {
      (void)printf("# user RE SWL offset=%u produced wrong bytes\n", offset);
      return false;
    }
    (void)memcpy(bytes, (const uint8_t[]){0x10, 0x11, 0x12, 0x13,
                                          0x14, 0x15, 0x16, 0x17}, 8u);
    result = run_one(value->machine,
                     encode_i(0x2eu, 8u, (uint16_t)offset), status,
                     UINT64_C(0x1000), UINT64_C(0x1122334455667788));
    if (result != T64_STATUS_OK ||
        memcmp(bytes, right_store[offset], 8u) != 0) {
      (void)printf("# user RE SWR offset=%u produced wrong bytes\n", offset);
      return false;
    }
  }
  return true;
}

static bool test_doubleword_merge_offsets(fixture *value) {
  static const uint64_t left_load[8] = {
      UINT64_C(0x1011121314151617), UINT64_C(0x1112131415161788),
      UINT64_C(0x1213141516177788), UINT64_C(0x1314151617667788),
      UINT64_C(0x1415161755667788), UINT64_C(0x1516174455667788),
      UINT64_C(0x1617334455667788), UINT64_C(0x1722334455667788)};
  static const uint64_t right_load[8] = {
      UINT64_C(0x1122334455667710), UINT64_C(0x1122334455661011),
      UINT64_C(0x1122334455101112), UINT64_C(0x1122334410111213),
      UINT64_C(0x1122331011121314), UINT64_C(0x1122101112131415),
      UINT64_C(0x1110111213141516), UINT64_C(0x1011121314151617)};
  static const uint8_t left_store[8][8] = {
      {0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0x07, 0x18},
      {0x10, 0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0x07},
      {0x10, 0x11, 0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6},
      {0x10, 0x11, 0x12, 0xa1, 0xb2, 0xc3, 0xd4, 0xe5},
      {0x10, 0x11, 0x12, 0x13, 0xa1, 0xb2, 0xc3, 0xd4},
      {0x10, 0x11, 0x12, 0x13, 0x14, 0xa1, 0xb2, 0xc3},
      {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0xa1, 0xb2},
      {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0xa1}};
  static const uint8_t right_store[8][8] = {
      {0x18, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17},
      {0x07, 0x18, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17},
      {0xf6, 0x07, 0x18, 0x13, 0x14, 0x15, 0x16, 0x17},
      {0xe5, 0xf6, 0x07, 0x18, 0x14, 0x15, 0x16, 0x17},
      {0xd4, 0xe5, 0xf6, 0x07, 0x18, 0x15, 0x16, 0x17},
      {0xc3, 0xd4, 0xe5, 0xf6, 0x07, 0x18, 0x16, 0x17},
      {0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0x07, 0x18, 0x17},
      {0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0x07, 0x18}};
  const uint8_t initial[8] = {0x10, 0x11, 0x12, 0x13,
                              0x14, 0x15, 0x16, 0x17};
  uint8_t *bytes = value->machine->ram + UINT32_C(0x2000);
  uint32_t offset;

  for (offset = 0u; offset < 8u; offset++) {
    t64_status status;

    (void)memcpy(bytes, initial, sizeof initial);
    status = run_one(value->machine, encode_i(0x1au, 8u, (uint16_t)offset),
                     0u, UINT64_C(0x1000),
                     UINT64_C(0x1122334455667788));
    if (status != T64_STATUS_OK || value->machine->cpu.gpr[8] !=
                                       left_load[offset]) {
      (void)printf("# LDL offset=%u expected=%016llx observed=%016llx\n",
                   offset, (unsigned long long)left_load[offset],
                   (unsigned long long)value->machine->cpu.gpr[8]);
      return false;
    }
    (void)memcpy(bytes, initial, sizeof initial);
    status = run_one(value->machine, encode_i(0x1bu, 8u, (uint16_t)offset),
                     0u, UINT64_C(0x1000),
                     UINT64_C(0x1122334455667788));
    if (status != T64_STATUS_OK || value->machine->cpu.gpr[8] !=
                                       right_load[offset]) {
      (void)printf("# LDR offset=%u expected=%016llx observed=%016llx\n",
                   offset, (unsigned long long)right_load[offset],
                   (unsigned long long)value->machine->cpu.gpr[8]);
      return false;
    }
    (void)memcpy(bytes, initial, sizeof initial);
    status = run_one(value->machine, encode_i(0x2cu, 8u, (uint16_t)offset),
                     0u, UINT64_C(0x1000),
                     UINT64_C(0xa1b2c3d4e5f60718));
    if (status != T64_STATUS_OK || memcmp(bytes, left_store[offset], 8u) != 0) {
      (void)printf("# SDL offset=%u first=%02x expected=%02x\n", offset,
                   bytes[0], left_store[offset][0]);
      return false;
    }
    (void)memcpy(bytes, initial, sizeof initial);
    status = run_one(value->machine, encode_i(0x2du, 8u, (uint16_t)offset),
                     0u, UINT64_C(0x1000),
                     UINT64_C(0xa1b2c3d4e5f60718));
    if (status != T64_STATUS_OK ||
        memcmp(bytes, right_store[offset], 8u) != 0) {
      (void)printf("# SDR offset=%u first=%02x expected=%02x\n", offset,
                   bytes[0], right_store[offset][0]);
      return false;
    }
  }
  return true;
}

static bool test_reverse_endian_doubleword_merge_offsets(fixture *value) {
  static const uint64_t left_load[8] = {
      UINT64_C(0x1722334455667788), UINT64_C(0x1617334455667788),
      UINT64_C(0x1516174455667788), UINT64_C(0x1415161755667788),
      UINT64_C(0x1314151617667788), UINT64_C(0x1213141516177788),
      UINT64_C(0x1112131415161788), UINT64_C(0x1011121314151617)};
  static const uint64_t right_load[8] = {
      UINT64_C(0x1011121314151617), UINT64_C(0x1110111213141516),
      UINT64_C(0x1122101112131415), UINT64_C(0x1122331011121314),
      UINT64_C(0x1122334410111213), UINT64_C(0x1122334455101112),
      UINT64_C(0x1122334455661011), UINT64_C(0x1122334455667710)};
  static const uint8_t left_store[8][8] = {
      {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x11},
      {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x11, 0x22},
      {0x10, 0x11, 0x12, 0x13, 0x14, 0x11, 0x22, 0x33},
      {0x10, 0x11, 0x12, 0x13, 0x11, 0x22, 0x33, 0x44},
      {0x10, 0x11, 0x12, 0x11, 0x22, 0x33, 0x44, 0x55},
      {0x10, 0x11, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66},
      {0x10, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77},
      {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88}};
  static const uint8_t right_store[8][8] = {
      {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88},
      {0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x17},
      {0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x16, 0x17},
      {0x44, 0x55, 0x66, 0x77, 0x88, 0x15, 0x16, 0x17},
      {0x55, 0x66, 0x77, 0x88, 0x14, 0x15, 0x16, 0x17},
      {0x66, 0x77, 0x88, 0x13, 0x14, 0x15, 0x16, 0x17},
      {0x77, 0x88, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17},
      {0x88, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17}};
  const uint32_t status = STATUS_USER | STATUS_RE | STATUS_UX;
  const uint8_t initial[8] = {0x10, 0x11, 0x12, 0x13,
                              0x14, 0x15, 0x16, 0x17};
  uint8_t *bytes = value->machine->ram + UINT32_C(0x2000);
  uint32_t offset;

  for (offset = 0u; offset < 8u; offset++) {
    t64_status result;

    (void)memcpy(bytes, initial, sizeof initial);
    result = run_one(value->machine,
                     encode_i(0x1au, 8u, (uint16_t)offset), status,
                     UINT64_C(0x1000), UINT64_C(0x1122334455667788));
    if (result != T64_STATUS_OK ||
        value->machine->cpu.gpr[8] != left_load[offset]) {
      (void)printf("# user RE LDL offset=%u expected=%016llx observed=%016llx\n",
                   offset, (unsigned long long)left_load[offset],
                   (unsigned long long)value->machine->cpu.gpr[8]);
      return false;
    }
    (void)memcpy(bytes, initial, sizeof initial);
    result = run_one(value->machine,
                     encode_i(0x1bu, 8u, (uint16_t)offset), status,
                     UINT64_C(0x1000), UINT64_C(0x1122334455667788));
    if (result != T64_STATUS_OK ||
        value->machine->cpu.gpr[8] != right_load[offset]) {
      (void)printf("# user RE LDR offset=%u expected=%016llx observed=%016llx\n",
                   offset, (unsigned long long)right_load[offset],
                   (unsigned long long)value->machine->cpu.gpr[8]);
      return false;
    }
    (void)memcpy(bytes, initial, sizeof initial);
    result = run_one(value->machine,
                     encode_i(0x2cu, 8u, (uint16_t)offset), status,
                     UINT64_C(0x1000), UINT64_C(0x1122334455667788));
    if (result != T64_STATUS_OK ||
        memcmp(bytes, left_store[offset], sizeof initial) != 0) {
      (void)printf("# user RE SDL offset=%u produced wrong byte lanes\n",
                   offset);
      return false;
    }
    (void)memcpy(bytes, initial, sizeof initial);
    result = run_one(value->machine,
                     encode_i(0x2du, 8u, (uint16_t)offset), status,
                     UINT64_C(0x1000), UINT64_C(0x1122334455667788));
    if (result != T64_STATUS_OK ||
        memcmp(bytes, right_store[offset], sizeof initial) != 0) {
      (void)printf("# user RE SDR offset=%u produced wrong byte lanes\n",
                   offset);
      return false;
    }
  }
  return true;
}

static bool test_zero_register_and_address_wrap(fixture *value) {
  t64_status status;
  uint32_t opcode;

  write_word(value->machine, UINT32_C(0x1000), UINT32_C(0x8c880001));
  t64_cpu_icache_hit_invalidate(value->machine, UINT64_C(0),
                                UINT32_C(0x1000));
  value->machine->cpu.pc_full = 0u;
  value->machine->cpu.pc = 0u;
  value->machine->cpu.cp0[12] = 0u;
  value->machine->cpu.gpr[4] = UINT64_MAX;
  status = t64_cpu_step(value->machine, NULL);
  if (status != T64_STATUS_OK ||
      value->machine->cpu.gpr[8] != UINT64_C(0xffffffff8c880001)) {
    (void)printf("# effective address wrap load=%016llx\n",
                 (unsigned long long)value->machine->cpu.gpr[8]);
    return false;
  }
  opcode = encode_i(0x23u, 0u, 0u);
  status = run_one(value->machine, opcode, 0u, UINT64_C(0x1000),
                   UINT64_C(0x12345678));
  return status == T64_STATUS_OK && value->machine->cpu.gpr[0] == 0u;
}

static bool test_watch_permissions_and_deferred_context(fixture *value) {
  t64_machine *machine = value->machine;
  uint8_t *bytes = machine->ram + UINT32_C(0x2000);
  t64_status status;

  (void)memcpy(bytes, (const uint8_t[]){0x11, 0x22, 0x33, 0x44}, 4u);
  machine->cpu.cp0[18] = UINT32_C(0x2000) | WATCH_R;
  status = run_one(machine, encode_i(0x24u, 8u, 0u), 0u,
                   UINT64_C(0x1000), UINT64_C(0xfeedface));
  if (status != T64_STATUS_OK || cause_code(machine) != 23u ||
      machine->cpu.gpr[8] != UINT64_C(0xfeedface)) {
    (void)printf("# Watch R cause=%u destination=%016llx\n",
                 cause_code(machine),
                 (unsigned long long)machine->cpu.gpr[8]);
    return false;
  }
  machine->cpu.cp0[18] = UINT32_C(0x2000) | WATCH_W;
  status = run_one(machine, encode_i(0x28u, 8u, 0u), 0u,
                   UINT64_C(0x1000), UINT64_C(0xaa));
  if (status != T64_STATUS_OK || cause_code(machine) != 23u ||
      bytes[0] != UINT8_C(0x11)) {
    (void)printf("# Watch W cause=%u memory=%02x\n", cause_code(machine),
                 bytes[0]);
    return false;
  }

  machine->cpu.cp0[18] = UINT32_C(0x2000) | WATCH_W;
  status = run_one(machine, encode_i(0x28u, 8u, 0u), STATUS_EXL,
                   UINT64_C(0x1000), UINT64_C(0xaa));
  if (status != T64_STATUS_OK || cause_code(machine) != 0u ||
      bytes[0] != UINT8_C(0xaa)) {
    (void)puts("# EXL did not defer Watch while the handler masked it");
    return false;
  }

  write_word(machine, UINT32_C(0x1000), UINT32_C(0x10000001));
  write_word(machine, UINT32_C(0x1004), encode_i(0x28u, 8u, 0u));
  t64_cpu_icache_hit_invalidate(machine, UINT64_C(0), UINT32_C(0x1000));
  machine->cpu.pc = 0u;
  machine->cpu.pc_full = 0u;
  machine->cpu.cp0[12] = 0u;
  machine->cpu.cp0[18] = UINT32_C(0x2000) | WATCH_W;
  machine->cpu.gpr[4] = UINT64_C(0x1000);
  machine->cpu.gpr[8] = UINT64_C(0xbb);
  if (t64_cpu_step(machine, NULL) != T64_STATUS_OK ||
      t64_cpu_step(machine, NULL) != T64_STATUS_OK ||
      cause_code(machine) != 23u ||
      ((uint32_t)machine->cpu.cp0[13] & CAUSE_BD) == 0u ||
      machine->cpu.cp0[14] != 0u || bytes[0] != UINT8_C(0xaa)) {
    (void)printf("# delay-slot Watch cause=%08x epc=%016llx memory=%02x\n",
                 (uint32_t)machine->cpu.cp0[13],
                 (unsigned long long)machine->cpu.cp0[14], bytes[0]);
    return false;
  }
  return true;
}

static bool test_fault_priority_and_store_preflight(fixture *value) {
  t64_machine *machine = value->machine;
  uint8_t *bytes = machine->ram + UINT32_C(0x2000);
  t64_status status;

  (void)memcpy(bytes, (const uint8_t[]){0x11, 0x22, 0x33, 0x44}, 4u);
  status = run_one(machine, encode_i(0x23u, 8u, 1u), 0u,
                   UINT64_C(0x1000), UINT64_C(0xfeedface));
  if (status != T64_STATUS_OK || cause_code(machine) != 4u ||
      machine->cpu.cp0[8] != UINT64_C(0x1001) ||
      machine->cpu.gpr[8] != UINT64_C(0xfeedface)) {
    (void)printf("# alignment priority cause=%u bad=%016llx\n",
                 cause_code(machine),
                 (unsigned long long)machine->cpu.cp0[8]);
    return false;
  }

  machine->cpu.tlb[0].entry_lo1 = entry_lo(UINT32_C(0x2000), false, false);
  status = run_one(machine, encode_i(0x2bu, 8u, 0u), 0u,
                   UINT64_C(0x1000), UINT64_C(0xaabbccdd));
  if (status != T64_STATUS_OK || cause_code(machine) != 3u ||
      memcmp(bytes, (const uint8_t[]){0x11, 0x22, 0x33, 0x44}, 4u) != 0) {
    (void)printf("# invalid store priority cause=%u memory=%02x%02x%02x%02x\n",
                 cause_code(machine), bytes[0], bytes[1], bytes[2], bytes[3]);
    return false;
  }
  machine->cpu.tlb[0].entry_lo1 = entry_lo(UINT32_C(0x2000), true, false);
  status = run_one(machine, encode_i(0x2bu, 8u, 0u), 0u,
                   UINT64_C(0x1000), UINT64_C(0xaabbccdd));
  if (status != T64_STATUS_OK || cause_code(machine) != 1u ||
      memcmp(bytes, (const uint8_t[]){0x11, 0x22, 0x33, 0x44}, 4u) != 0) {
    (void)printf("# modified store priority cause=%u\n", cause_code(machine));
    return false;
  }

  set_entry(machine, 0u, 0u, 1u, UINT32_C(0x1000), true, true,
            UINT32_C(0x90000000), true, true);
  machine->cpu.cp0[18] = UINT32_C(0x90000000) | WATCH_R;
  status = run_one(machine, encode_i(0x24u, 8u, 0u), 0u,
                   UINT64_C(0x1000), UINT64_C(0xfeedface));
  if (status != T64_STATUS_OK || cause_code(machine) != 23u ||
      machine->cpu.gpr[8] != UINT64_C(0xfeedface)) {
    (void)printf("# Watch must precede unmapped data bus error: cause=%u\n",
                 cause_code(machine));
    return false;
  }

  machine->cpu.cp0[18] = 0u;
  status = run_one(machine, encode_i(0x2bu, 8u, 0u), 0u,
                   UINT64_C(0x1000), UINT64_C(0xaabbccdd));
  if (status != T64_STATUS_OK || cause_code(machine) != 7u) {
    (void)printf("# unmapped data store expected DBE cause=7 observed=%u\n",
                 cause_code(machine));
    return false;
  }

  set_entry(machine, 0u, 0u, 1u, UINT32_C(0x1000), true, true,
            UINT32_C(0x2000), true, true);
  (void)memcpy(bytes, (const uint8_t[]){0x11, 0x22, 0x33, 0x44}, 4u);
  machine->cpu.cp0[18] = UINT32_C(0x2000) | WATCH_W;
  write_word(machine, UINT32_C(0x1000), UINT32_C(0x10000001));
  write_word(machine, UINT32_C(0x1004), encode_i(0x2bu, 8u, 0u));
  t64_cpu_icache_hit_invalidate(machine, UINT64_C(0), UINT32_C(0x1000));
  machine->cpu.pc = 0u;
  machine->cpu.pc_full = 0u;
  machine->cpu.cp0[12] = 0u;
  machine->cpu.gpr[4] = UINT64_C(0x1000);
  machine->cpu.gpr[8] = UINT64_C(0xaabbccdd);
  if (t64_cpu_step(machine, NULL) != T64_STATUS_OK ||
      t64_cpu_step(machine, NULL) != T64_STATUS_OK ||
      cause_code(machine) != 23u ||
      ((uint32_t)machine->cpu.cp0[13] & CAUSE_BD) == 0u ||
      machine->cpu.cp0[14] != 0u ||
      memcmp(bytes, (const uint8_t[]){0x11, 0x22, 0x33, 0x44}, 4u) != 0) {
    (void)puts("# store fault boundary published partial effects");
    return false;
  }
  return true;
}

static bool test_user_double_merge_mode_refusal(fixture *value) {
  static const uint32_t operations[] = {
      0x1au, 0x1bu, 0x2cu, 0x2du, 0x37u, 0x3fu};
  uint8_t *bytes = value->machine->ram + UINT32_C(0x2000);
  size_t index;

  for (index = 0u; index < sizeof operations / sizeof operations[0]; index++) {
    t64_status status;

    (void)memcpy(bytes, (const uint8_t[]){0x11, 0x22, 0x33, 0x44,
                                          0x55, 0x66, 0x77, 0x88}, 8u);
    status = run_one(value->machine,
                     encode_i(operations[index], 8u, 0u), STATUS_USER,
                     UINT64_C(0x1000), UINT64_C(0xaabbccddeeff0011));
    if (status != T64_STATUS_OK || cause_code(value->machine) != 10u ||
        value->machine->cpu.gpr[8] != UINT64_C(0xaabbccddeeff0011) ||
        memcmp(bytes,
               (const uint8_t[]){0x11, 0x22, 0x33, 0x44,
                                 0x55, 0x66, 0x77, 0x88}, 8u) != 0) {
      (void)printf("# user 32-bit 64-bit memory op=%02x cause=%u\n",
                   operations[index], cause_code(value->machine));
      return false;
    }
  }
  return true;
}

static bool test_reverse_endian_scalar_cartridge_bounds(fixture *value) {
  static const uint32_t operations[] = {0x24u, 0x25u, 0x23u};
  static const uint16_t offsets[] = {UINT16_C(0x0ffb), UINT16_C(0x0ffa),
                                     UINT16_C(0x0ff8)};
  static const uint32_t widths[] = {1u, 2u, 4u};
  t64_machine *machine = value->machine;
  const uint32_t status_bits = STATUS_USER | STATUS_RE | STATUS_UX;
  size_t saved_image_length = machine->image_length;
  uint32_t index;

  /* A 4-byte-aligned, 4 mod 8 cartridge end makes the selected RE byte,
   * halfword, and word lane fall outside the backed image while the
   * untranslated operand range remains inside it. */
  machine->image_length = 4092u;
  set_entry(machine, 0u, 0u, 1u, UINT32_C(0x1000), true, true,
            UINT32_C(0x10000000), true, true);
  for (index = 0u; index < 3u; index++) {
    uint64_t address = UINT64_C(0x1000) + offsets[index];
    uint64_t retired_before = machine->cpu.retired_instructions;
    t64_status result = run_one(
        machine, encode_i(operations[index], 8u, offsets[index]),
        status_bits, UINT64_C(0x1000), UINT64_C(0xfeedface));

    if (result != T64_STATUS_OK || cause_code(machine) != 7u ||
        machine->cpu.cp0[8] != 0u ||
        machine->cpu.gpr[8] != UINT64_C(0xfeedface) ||
        machine->cpu.retired_instructions != retired_before) {
      (void)printf(
          "# RE scalar lane bound width=%u va=%016llx status=%u cause=%u badvaddr=%016llx destination=%016llx retired=%llu\n",
          widths[index], (unsigned long long)address, (unsigned)result,
          cause_code(machine),
          (unsigned long long)machine->cpu.cp0[8],
          (unsigned long long)machine->cpu.gpr[8],
          (unsigned long long)machine->cpu.retired_instructions);
      machine->image_length = saved_image_length;
      set_entry(machine, 0u, 0u, 1u, UINT32_C(0x1000), true, true,
                UINT32_C(0x2000), true, true);
      return false;
    }
  }
  machine->image_length = saved_image_length;
  set_entry(machine, 0u, 0u, 1u, UINT32_C(0x1000), true, true,
            UINT32_C(0x2000), true, true);
  return true;
}

int main(void) {
  static const memory_test_case tests[] = {
      {"user reverse endian selects physical lanes while kernel and fetch stay BE",
       test_reverse_endian_user_lanes},
      {"aligned integer widths sign extend and store exact bytes",
       test_aligned_integer_widths},
      {"LWL/LWR/SWL/SWR offsets match independent big-endian tables",
       test_word_merge_offsets},
      {"user RE word merges match independent manual tables",
       test_reverse_endian_word_merge_offsets},
      {"LDL/LDR/SDL/SDR offsets match independent big-endian tables",
       test_doubleword_merge_offsets},
      {"user RE doubleword merges match independent manual tables",
       test_reverse_endian_doubleword_merge_offsets},
      {"zero register and wrapped effective address remain defined",
       test_zero_register_and_address_wrap},
      {"Watch read/write masks and delayed context are precise",
       test_watch_permissions_and_deferred_context},
      {"alignment, TLB, Watch and bus faults preserve priority and effects",
       test_fault_priority_and_store_preflight},
      {"LDL in 32-bit user mode raises Reserved Instruction",
       test_user_double_merge_mode_refusal},
      {"user RE scalar widths preflight the selected cartridge lane",
       test_reverse_endian_scalar_cartridge_bounds},
  };
  fixture value = {0};
  size_t index;
  size_t failures = 0u;

  if (!fixture_open(&value)) {
    (void)puts("1..1\nnot ok 1 - memory fixture setup");
    return 1;
  }
  (void)printf("1..%zu\n", sizeof tests / sizeof tests[0]);
  for (index = 0u; index < sizeof tests / sizeof tests[0]; index++) {
    bool okay = tests[index].run(&value);

    (void)printf("%s %zu - %s\n", okay ? "ok" : "not ok", index + 1u,
                 tests[index].name);
    if (!okay) {
      failures++;
    }
  }
  if (!fixture_close(&value, failures == 0u)) {
    failures++;
  }
  return failures == 0u ? 0 : 1;
}
