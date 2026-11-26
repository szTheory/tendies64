/*
 * Phase 9 SI, PIF and input-latch tests (plan 09-03).
 *
 * Every figure below is a named software-profile policy value written out as
 * a literal in this file; nothing is taken from a production helper, and no
 * hardware accuracy or hardware timing is claimed.
 *
 * capacity-rule: the shared scheduler capacity rule against a hand-derived
 * table.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_internal.h"
#include "t64_test.h"

#define SI_STEM_ID "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1"
#define IPL3_ASSET_BYTES 4096u

static bool read_openipl3_asset(uint8_t bytes[IPL3_ASSET_BYTES]) {
  const char *path = getenv("T64_TEST_IPL3_PATH");
  FILE *file;
  size_t length;
  int extra;
  int read_error;
  int close_error;

  if (path == NULL || path[0] == '\0' || bytes == NULL) return false;
  file = fopen(path, "rb");
  if (file == NULL) return false;
  length = fread(bytes, 1u, IPL3_ASSET_BYTES, file);
  extra = fgetc(file);
  read_error = ferror(file);
  close_error = fclose(file);
  return read_error == 0 && close_error == 0 &&
         length == IPL3_ASSET_BYTES && extra == EOF;
}

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  void *memory;

  (void)context;
  if (storage_out == NULL || size == 0u || alignment == 0u) return false;
  memory = malloc(size);
  if (memory == NULL || (uintptr_t)memory % alignment != 0u) {
    free(memory);
    return false;
  }
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = alignment;
  return true;
}

static void test_release(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

static const t64_host_allocator allocator =
    T64_HOST_ALLOCATOR_INIT(test_allocate, test_release, NULL);

static t64_machine *create_admitted(const char *profile_id) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t asset[IPL3_ASSET_BYTES];

  T64_CHECK_INT(t64_profile_lookup(profile_id, &profile), T64_STATUS_OK);
  if (profile == NULL) return NULL;
  T64_CHECK_INT(t64_machine_create(
                    &(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(
                        &allocator, profile),
                    &machine),
                T64_STATUS_OK);
  if (machine == NULL) return NULL;
  if (!read_openipl3_asset(asset)) {
    T64_CHECK(false);
    t64_machine_destroy(machine);
    return NULL;
  }
  T64_CHECK_INT(t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES),
                T64_STATUS_OK);
  return machine;
}

/* ------------------------------------------------------------------------ */
/* capacity-rule                                                             */
/* ------------------------------------------------------------------------ */

typedef struct {
  const char *label;
  uint32_t capacity;       /* profile event_queue_capacity */
  size_t queue_count;
  uint64_t next_sequence;
  uint32_t width_bits;     /* profile sequence_width_bits */
  bool exhausted;
  uint64_t tick;
  size_t event_count;
  uint64_t max_delay;
  uint32_t expected;
} capacity_row;

/*
 * Hand-derived: the limit is the profile capacity (never above 256); the
 * order is queue room, then sequence width, then sequence headroom, then the
 * tick sum. Headroom for n events is n minus one sequences above next.
 */
static const capacity_row capacity_rows[] = {
    {"room equals count", 8u, 5u, 0u, 8u, false, 0u, 3u, 10u,
     T64_STATUS_OK},
    {"room one short", 8u, 6u, 0u, 8u, false, 0u, 3u, 10u,
     T64_STATUS_QUEUE_FULL},
    {"queue above limit", 8u, 9u, 0u, 8u, false, 0u, 1u, 10u,
     T64_STATUS_QUEUE_FULL},
    {"zero events at a full queue", 8u, 8u, 0u, 8u, false, 0u, 0u, 10u,
     T64_STATUS_OK},
    {"width zero after queue passed", 8u, 0u, 0u, 0u, false, 0u, 1u, 10u,
     T64_STATUS_INVALID_ARGUMENT},
    {"width above 64 after queue passed", 8u, 0u, 0u, 65u, false, 0u, 1u,
     10u, T64_STATUS_INVALID_ARGUMENT},
    {"queue wins over a bad width", 8u, 8u, 0u, 0u, false, 0u, 1u, 10u,
     T64_STATUS_QUEUE_FULL},
    {"headroom exactly count minus one", 8u, 0u, 253u, 8u, false, 0u, 3u,
     10u, T64_STATUS_OK},
    {"headroom one short", 8u, 0u, 254u, 8u, false, 0u, 3u, 10u,
     T64_STATUS_SEQUENCE_EXHAUSTED},
    {"exhausted flag", 8u, 0u, 0u, 8u, true, 0u, 1u, 10u,
     T64_STATUS_SEQUENCE_EXHAUSTED},
    {"next above the width limit", 8u, 0u, 256u, 8u, false, 0u, 1u, 10u,
     T64_STATUS_SEQUENCE_EXHAUSTED},
    {"width 64 one event at the maximum", 8u, 0u, UINT64_MAX, 64u, false,
     0u, 1u, 10u, T64_STATUS_OK},
    {"width 64 two events at the maximum", 8u, 0u, UINT64_MAX, 64u, false,
     0u, 2u, 10u, T64_STATUS_SEQUENCE_EXHAUSTED},
    {"width 64 three events one below", 8u, 0u, UINT64_MAX - 1u, 64u, false,
     0u, 3u, 10u, T64_STATUS_SEQUENCE_EXHAUSTED},
    {"width 64 two events one below", 8u, 0u, UINT64_MAX - 1u, 64u, false,
     0u, 2u, 10u, T64_STATUS_OK},
    {"tick one below the maximum, delay 1", 8u, 0u, 0u, 8u, false,
     UINT64_MAX - 1u, 1u, 1u, T64_STATUS_OK},
    {"tick one below the maximum, delay 2", 8u, 0u, 0u, 8u, false,
     UINT64_MAX - 1u, 1u, 2u, T64_STATUS_TICK_OVERFLOW},
    {"queue wins over exhausted and overflow", 8u, 8u, 0u, 8u, true,
     UINT64_MAX, 1u, 2u, T64_STATUS_QUEUE_FULL},
    {"sequence wins over tick overflow", 8u, 0u, 0u, 8u, true,
     UINT64_MAX - 1u, 1u, 2u, T64_STATUS_SEQUENCE_EXHAUSTED},
};

static void case_capacity_rule(void) {
  t64_machine *machine = create_admitted(SI_STEM_ID);
  size_t index;

  T64_CHECK_INT(t64_schedule_check_capacity(NULL, 1u, 1u),
                T64_STATUS_INVALID_ARGUMENT);
  if (machine == NULL) return;
  for (index = 0u; index < sizeof capacity_rows / sizeof capacity_rows[0];
       index++) {
    const capacity_row *row = &capacity_rows[index];
    t64_status status;

    /* N6: edit the in-memory copy of the profile after create; never create
       a machine from a mutated descriptor. */
    machine->profile.event_queue_capacity = row->capacity;
    machine->profile.sequence_width_bits = row->width_bits;
    machine->queue_head = 0u;
    machine->queue_count = row->queue_count;
    machine->next_sequence = row->next_sequence;
    machine->sequence_exhausted = row->exhausted;
    machine->tick = row->tick;
    status = t64_schedule_check_capacity(machine, row->event_count,
                                         row->max_delay);
    if ((uint32_t)status != row->expected) {
      fprintf(stderr, "capacity row '%s': got %u want %u\n", row->label,
              (unsigned)status, (unsigned)row->expected);
    }
    T64_CHECK_INT(status, row->expected);
  }

  /* The tick-overflow row equals the status the reserve path returns for the
     same condition, observed from an actual reserve_after call on the same
     state. */
  machine->profile.event_queue_capacity = 8u;
  machine->profile.sequence_width_bits = 8u;
  machine->queue_head = 0u;
  machine->queue_count = 0u;
  machine->next_sequence = 0u;
  machine->sequence_exhausted = false;
  machine->tick = UINT64_MAX - 1u;
  {
    t64_event_reservation reservation;
    t64_status reserve_status = t64_schedule_reserve_after(
        machine, 2u, T64_DEVICE_SYNTHETIC_TEST, T64_TEST_EVENT_NOOP, 0u, 0u,
        t64_input_event_handler, &reservation);

    T64_CHECK_INT(reserve_status, T64_STATUS_TICK_OVERFLOW);
    T64_CHECK_INT(t64_schedule_check_capacity(machine, 1u, 2u),
                  reserve_status);
    reserve_status = t64_schedule_reserve_after(
        machine, 1u, T64_DEVICE_SYNTHETIC_TEST, T64_TEST_EVENT_NOOP, 0u, 0u,
        t64_input_event_handler, &reservation);
    T64_CHECK_INT(reserve_status, T64_STATUS_OK);
    T64_CHECK_INT(t64_schedule_check_capacity(machine, 1u, 1u),
                  reserve_status);
  }
  t64_machine_destroy(machine);
}

/* ------------------------------------------------------------------------ */
/* cpu-store-budget                                                          */
/* ------------------------------------------------------------------------ */

#define AI_STEM_ID "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1"
#define LEGACY_T1_ID "ntsc-cart-4m-openipl3-rdram-b-t1-v1"

/* Authored program at RDRAM 0x1000: build the SI base 0xA4800000 in $t0
 * (lui $t0, 0xA480), the value 0x1FC007C0 in $t1 (lui $t1, 0x1FC0;
 * ori $t1, $t1, 0x07C0), then one store. */
#define OP_LUI_T0_SI UINT32_C(0x3C08A480)
#define OP_LUI_T1 UINT32_C(0x3C091FC0)
#define OP_ORI_T1 UINT32_C(0x352907C0)
#define OP_SW_T1_RD64B UINT32_C(0xAD090004) /* sw $t1, 4($t0)  */
#define OP_SW_T1_WR64B UINT32_C(0xAD090010) /* sw $t1, 16($t0) */
#define OP_SW_ZERO_STATUS UINT32_C(0xAD000018) /* sw $zero, 24($t0) */
#define OP_SD_T1_BASE UINT32_C(0xFD090000)  /* sd $t1, 0($t0)  */
/* Partial word stores whose merge is a full word at the register (plan 09-10,
 * review L1 WR-01): SWR at byte 3 and SWL at byte 0 of the word write all four
 * bytes, so the bus sees a full-mask word write and starts the DMA. */
#define OP_SWR_T1_RD64B UINT32_C(0xB9090007) /* swr $t1, 7($t0)  */
#define OP_SWL_T1_RD64B UINT32_C(0xA9090004) /* swl $t1, 4($t0)  */
#define OP_SWR_T1_WR64B UINT32_C(0xB9090013) /* swr $t1, 19($t0) */
#define OP_SWL_T1_WR64B UINT32_C(0xA9090010) /* swl $t1, 16($t0) */
/* Reverse-endian user mode (plan 09-10, re-check R1 RC-05): the lane of a full
 * word store is exchanged (address ^ 4), so the word at byte 0 of the page
 * reaches RD64B and the word at byte 4 reaches DRAM_ADDR. */
#define OP_SW_T1_BASE UINT32_C(0xAD090000) /* sw $t1, 0($t0)  */

#define BUDGET_QUEUE_LIMIT 16u
#define PROGRAM_ADDRESS UINT32_C(0x1000)

static void put_be32(uint8_t *bytes, uint32_t value) {
  bytes[0] = (uint8_t)(value >> 24);
  bytes[1] = (uint8_t)(value >> 16);
  bytes[2] = (uint8_t)(value >> 8);
  bytes[3] = (uint8_t)value;
}

static bool cancel_boot_cpu_event(t64_machine *machine) {
  uint64_t cancelled_tick = UINT64_MAX;
  t64_status status = t64_schedule_cancel(
      machine, T64_DEVICE_CPU, T64_CPU_EVENT_STEP, 0u, &cancelled_tick);

  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(cancelled_tick, 1u);
  return status == T64_STATUS_OK && cancelled_tick == 1u;
}

/* A machine that has run the three setup instructions and is positioned at
 * the store, with the free queue room and sequence state set directly. */
static t64_machine *make_store_machine(const char *profile_id,
                                       uint32_t store_opcode, size_t room,
                                       uint32_t width_bits,
                                       uint64_t next_sequence) {
  t64_machine *machine = create_admitted(profile_id);
  const uint32_t program[4] = {OP_LUI_T0_SI, OP_LUI_T1, OP_ORI_T1,
                               store_opcode};
  size_t index;

  if (machine == NULL) return NULL;
  if (!cancel_boot_cpu_event(machine)) {
    t64_machine_destroy(machine);
    return NULL;
  }
  for (index = 0u; index < 4u; index++) {
    put_be32(machine->ram + PROGRAM_ADDRESS + 4u * index, program[index]);
  }
  machine->cpu.pc = UINT32_C(0x80001000);
  machine->cpu.pc_full = t64_cpu_canonical_address(machine->cpu.pc);
  /* N6: edit the in-memory copy of the profile after create. */
  machine->profile.event_queue_capacity = BUDGET_QUEUE_LIMIT;
  for (index = 0u; index < 3u; index++) {
    T64_CHECK_INT(t64_cpu_step(machine, NULL), T64_STATUS_OK);
  }
  T64_CHECK_INT(machine->cpu.retired_instructions, 3u);
  T64_CHECK_INT(machine->cpu.gpr[8], UINT64_C(0xffffffffa4800000));
  T64_CHECK_INT(machine->cpu.gpr[9], UINT64_C(0x1fc007c0));
  machine->queue_head = 0u;
  machine->queue_count = BUDGET_QUEUE_LIMIT - room;
  machine->profile.sequence_width_bits = width_bits;
  machine->next_sequence = next_sequence;
  machine->sequence_exhausted = false;
  return machine;
}

typedef struct {
  t64_status status;
  uint64_t retired;
  size_t queue_count;
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  t64_si_state si_before;
} store_result;

/* A refused step still records the attempt (last PCs, last opcode and a host
 * error outcome) by design; the expected pre-state carries that record so
 * the digest comparison isolates every other effect. */
static store_result run_store(t64_machine *machine, uint32_t store_opcode) {
  store_result result;

  (void)memset(&result, 0, sizeof result);
  result.si_before = machine->si;
  machine->cpu.last_pc = machine->cpu.pc;
  machine->cpu.last_pc_full = machine->cpu.pc_full;
  machine->cpu.last_opcode = store_opcode;
  machine->cpu.attempt_outcome = T64_CPU_ATTEMPT_HOST_ERROR;
  T64_CHECK_INT(t64_machine_digest(machine, result.digest_before),
                T64_STATUS_OK);
  result.status = t64_cpu_step(machine, NULL);
  result.retired = machine->cpu.retired_instructions;
  result.queue_count = machine->queue_count;
  T64_CHECK_INT(t64_machine_digest(machine, result.digest_after),
                T64_STATUS_OK);
  return result;
}

/* A capacity refusal happens before any mutation: nothing retired beyond the
 * three setup instructions, SI state untouched, digest bit-identical. */
static void check_refused(const char *label, const store_result *result,
                          t64_status expected, size_t room,
                          const t64_machine *machine) {
  if (result->status != expected) {
    fprintf(stderr,
            "budget row '%s': status %u (want %u), retired %llu, queue %zu\n",
            label, (unsigned)result->status, (unsigned)expected,
            (unsigned long long)result->retired, result->queue_count);
  }
  T64_CHECK_INT(result->status, expected);
  T64_CHECK_INT(result->retired, 3u);
  T64_CHECK_INT(result->queue_count, BUDGET_QUEUE_LIMIT - room);
  T64_CHECK(memcmp(&result->si_before, &machine->si, sizeof machine->si) == 0);
  T64_CHECK(memcmp(result->digest_before, result->digest_after,
                   T64_DIGEST_SIZE) == 0);
}

static bool refused_for_capacity(t64_status status) {
  return status == T64_STATUS_QUEUE_FULL ||
         status == T64_STATUS_SEQUENCE_EXHAUSTED;
}

/* One refuse-then-accept pair on a fresh machine each: the refusing room has
 * one event too few for the store, the accepting room has exactly enough. */
static void budget_room_pair_ex(const char *label, const char *profile_id,
                                uint32_t store_opcode, size_t refuse_room,
                                size_t accept_room, bool must_retire) {
  t64_machine *machine;
  store_result result;

  machine = make_store_machine(profile_id, store_opcode, accept_room, 64u, 0u);
  if (machine == NULL) return;
  result = run_store(machine, store_opcode);
  /* Accepted baseline in the same setup: the capacity rule did not refuse,
   * and where the store is performed it retires as the fourth instruction. */
  if (refused_for_capacity(result.status)) {
    fprintf(stderr, "budget row '%s': accept room %zu refused with %u\n",
            label, accept_room, (unsigned)result.status);
  }
  T64_CHECK(!refused_for_capacity(result.status));
  if (must_retire) {
    T64_CHECK_INT(result.status, T64_STATUS_OK);
    T64_CHECK_INT(result.retired, 4u);
  }
  t64_machine_destroy(machine);

  machine = make_store_machine(profile_id, store_opcode, refuse_room, 64u, 0u);
  if (machine == NULL) return;
  result = run_store(machine, store_opcode);
  check_refused(label, &result, T64_STATUS_QUEUE_FULL, refuse_room, machine);
  t64_machine_destroy(machine);
}

static void budget_room_pair(const char *label, const char *profile_id,
                             uint32_t store_opcode, size_t refuse_room,
                             size_t accept_room) {
  budget_room_pair_ex(label, profile_id, store_opcode, refuse_room,
                      accept_room, false);
}

/* A user-mode program with the SI page reached through the TLB (plan 09-10,
 * re-check R1 RC-05). One TLB pair at user address 0x00400000: the even page
 * maps the SI registers (uncached), the odd page maps the program page at
 * RDRAM 0x1000. The machine is positioned at the store with $t0 aiming at the
 * even page; Status holds user mode, plus Status.RE and Config.BE when
 * `reverse_endian` asks for the reverse-endian lane exchange. */
#define RE_USER_PAGE UINT64_C(0x00400000)
#define RE_STATUS_USER UINT64_C(0x00000010)
#define RE_STATUS_RE UINT64_C(0x02000000)
#define RE_CONFIG_BE UINT64_C(0x00008000)
#define RE_CP0_STATUS 12u
#define RE_CP0_CONFIG 16u
#define RE_ENTRYLO_BITS UINT64_C(0x17) /* uncached, valid, dirty, global */

static uint64_t re_entry_lo(uint32_t physical_page) {
  return (((uint64_t)physical_page >> 12) << 6) | RE_ENTRYLO_BITS;
}

static t64_machine *make_user_store_machine(uint32_t store_opcode, size_t room,
                                            bool reverse_endian) {
  t64_machine *machine =
      make_store_machine(SI_STEM_ID, store_opcode, room, 64u, 0u);
  size_t index;

  if (machine == NULL) return NULL;
  for (index = 0u; index < T64_VR4300_TLB_ENTRY_COUNT; index++) {
    machine->cpu.tlb[index] = (t64_vr4300_tlb_pair){0};
    machine->cpu.tlb[index].asid = UINT32_C(0xff);
  }
  machine->cpu.tlb[0].page_mask = 0u;
  machine->cpu.tlb[0].vpn2 = RE_USER_PAGE;
  machine->cpu.tlb[0].asid = 0u;
  machine->cpu.tlb[0].entry_lo0 = re_entry_lo(UINT32_C(0x04800000));
  machine->cpu.tlb[0].entry_lo1 = re_entry_lo(PROGRAM_ADDRESS);
  machine->cpu.gpr[8] = RE_USER_PAGE;
  machine->cpu.pc = (uint32_t)(RE_USER_PAGE + PROGRAM_ADDRESS + 12u);
  machine->cpu.pc_full = RE_USER_PAGE + PROGRAM_ADDRESS + 12u;
  machine->cpu.cp0[RE_CP0_STATUS] =
      RE_STATUS_USER | (reverse_endian ? RE_STATUS_RE : UINT64_C(0));
  if (reverse_endian) machine->cpu.cp0[RE_CP0_CONFIG] |= RE_CONFIG_BE;
  return machine;
}

/* One refuse-then-accept pair on the user-mode machine: the accepting room has
 * exactly enough for the store, which retires as the fourth instruction; the
 * refusing room is one event short and leaves everything untouched. */
static void budget_room_pair_user(const char *label, uint32_t store_opcode,
                                  size_t refuse_room, size_t accept_room,
                                  bool reverse_endian) {
  t64_machine *machine;
  store_result result;

  machine = make_user_store_machine(store_opcode, accept_room, reverse_endian);
  if (machine == NULL) return;
  result = run_store(machine, store_opcode);
  if (result.status != T64_STATUS_OK) {
    fprintf(stderr, "user budget row '%s': accept room %zu gave status %u\n",
            label, accept_room, (unsigned)result.status);
  }
  T64_CHECK_INT(result.status, T64_STATUS_OK);
  T64_CHECK_INT(result.retired, 4u);
  t64_machine_destroy(machine);

  machine = make_user_store_machine(store_opcode, refuse_room, reverse_endian);
  if (machine == NULL) return;
  result = run_store(machine, store_opcode);
  check_refused(label, &result, T64_STATUS_QUEUE_FULL, refuse_room, machine);
  t64_machine_destroy(machine);
}

/* Sequence analogue: width 8 gives a limit of 255. needed events need
 * headroom needed - 1 above next_sequence. */
static void budget_sequence_pair(const char *label, const char *profile_id,
                                 uint32_t store_opcode,
                                 uint64_t refuse_next,
                                 uint64_t accept_next) {
  t64_machine *machine;
  store_result result;

  machine = make_store_machine(profile_id, store_opcode, 8u, 8u, accept_next);
  if (machine == NULL) return;
  result = run_store(machine, store_opcode);
  if (refused_for_capacity(result.status)) {
    fprintf(stderr, "sequence row '%s': accept next %llu refused with %u\n",
            label, (unsigned long long)accept_next, (unsigned)result.status);
  }
  T64_CHECK(!refused_for_capacity(result.status));
  t64_machine_destroy(machine);

  machine = make_store_machine(profile_id, store_opcode, 8u, 8u, refuse_next);
  if (machine == NULL) return;
  result = run_store(machine, store_opcode);
  check_refused(label, &result, T64_STATUS_SEQUENCE_EXHAUSTED, 8u, machine);
  t64_machine_destroy(machine);
}

static t64_si_state family_si_after;

static t64_status eight_byte_store_status(const char *profile_id,
                                          uint32_t window_address,
                                          store_result *result_out,
                                          bool *stopped_out) {
  t64_machine *machine;
  store_result result;
  t64_status status;

  machine = make_store_machine(profile_id, OP_SD_T1_BASE, 8u, 64u, 0u);
  if (machine == NULL) return T64_STATUS_INVALID_ARGUMENT;
  /* Re-aim $t0 at the requested window (the AI analog uses 0xA4500000). */
  machine->cpu.gpr[8] = t64_cpu_canonical_address(window_address);
  result = run_store(machine, OP_SD_T1_BASE);
  status = result.status;
  *result_out = result;
  *stopped_out = machine->cpu.stopped;
  family_si_after = machine->si;
  t64_machine_destroy(machine);
  return status;
}

static void case_cpu_store_budget(void) {
  store_result result;
  store_result analog;
  t64_status analog_status;
  t64_status family_status;
  bool analog_stopped = false;
  bool family_stopped = false;

  /* D-44: on the family, RD64B needs three events (one successor, two SI
     events) and WR64B needs two; the cap is the free room. */
  budget_room_pair("family RD64B", SI_STEM_ID, OP_SW_T1_RD64B, 2u, 3u);
  budget_room_pair("family WR64B", SI_STEM_ID, OP_SW_T1_WR64B, 1u, 2u);
  /* Sequence analogue: next 254 leaves headroom 1 (RD64B needs 2), 253
     leaves 2; WR64B needs 1, so next 255 (headroom 0) refuses and 254 does
     not. */
  budget_sequence_pair("family RD64B sequence", SI_STEM_ID, OP_SW_T1_RD64B,
                       254u, 253u);
  budget_sequence_pair("family WR64B sequence", SI_STEM_ID, OP_SW_T1_WR64B,
                       255u, 254u);
  /* The partial stores that merge into a full word at the register start the
     DMA like sw, so they carry the same budget: refused with room one short,
     retired as the fourth instruction with exactly enough. */
  budget_room_pair_ex("family RD64B swr byte 3", SI_STEM_ID, OP_SWR_T1_RD64B,
                      2u, 3u, true);
  budget_room_pair_ex("family RD64B swl byte 0", SI_STEM_ID, OP_SWL_T1_RD64B,
                      2u, 3u, true);
  budget_room_pair_ex("family WR64B swr byte 3", SI_STEM_ID, OP_SWR_T1_WR64B,
                      1u, 2u, true);
  budget_room_pair_ex("family WR64B swl byte 0", SI_STEM_ID, OP_SWL_T1_WR64B,
                      1u, 2u, true);
  budget_sequence_pair("family RD64B swr sequence", SI_STEM_ID,
                       OP_SWR_T1_RD64B, 254u, 253u);
  budget_sequence_pair("family WR64B swl sequence", SI_STEM_ID,
                       OP_SWL_T1_WR64B, 255u, 254u);
  /* the accept halves of the plain rows assert the store retired too */
  budget_room_pair_ex("family RD64B retires", SI_STEM_ID, OP_SW_T1_RD64B, 2u,
                      3u, true);
  budget_room_pair_ex("family WR64B retires", SI_STEM_ID, OP_SW_T1_WR64B, 1u,
                      2u, true);
  /* Reverse-endian user mode through a TLB-mapped SI page (re-check R1 RC-05).
     The user-mode rows without the reverse-endian mode are the accepted
     baseline of the same mapping: the page reaches the registers as the
     kernel window does. With Status.RE and Config.BE the word at byte 0 of
     the page reaches RD64B (two events) and the word at byte 4 reaches
     DRAM_ADDR (a legacy one-event store). WR64B has the legacy budget of one
     event, so the lane exchange cannot be told apart there and has no row. */
  budget_room_pair_user("user RD64B, no reverse endian", OP_SW_T1_RD64B, 2u,
                        3u, false);
  budget_room_pair_user("user WR64B, no reverse endian", OP_SW_T1_WR64B, 1u,
                        2u, false);
  budget_room_pair_user("user byte 0 is DRAM_ADDR, no reverse endian",
                        OP_SW_T1_BASE, 1u, 2u, false);
  budget_room_pair_user("reverse endian byte 0 reaches RD64B", OP_SW_T1_BASE,
                        2u, 3u, true);
  budget_room_pair_user("reverse endian byte 4 reaches DRAM_ADDR",
                        OP_SW_T1_RD64B, 1u, 2u, true);
  /* Every other SI address and every legacy profile keep the legacy budget
     of one extra event: refused at room 1, accepted at room 2. */
  budget_room_pair("family STATUS store", SI_STEM_ID, OP_SW_ZERO_STATUS, 1u,
                   2u);
  budget_room_pair("ai profile RD64B address", AI_STEM_ID, OP_SW_T1_RD64B,
                   1u, 2u);
  budget_room_pair("legacy t1 profile RD64B address", LEGACY_T1_ID,
                   OP_SW_T1_RD64B, 1u, 2u);
  budget_room_pair("legacy t1 profile STATUS store", LEGACY_T1_ID,
                   OP_SW_ZERO_STATUS, 1u, 2u);

  /* An 8-byte store keeps budget 0: it needs room for the successor only, so
     it is not refused for capacity at room 1; its outcome is the bus refusal
     status, recorded from the equivalent 8-byte store to the AI window on
     the AI profile as the independent analog. */
  analog_status = eight_byte_store_status(AI_STEM_ID, UINT32_C(0xa4500000),
                                          &analog, &analog_stopped);
  T64_CHECK(!refused_for_capacity(analog_status));
  T64_CHECK(analog_status != T64_STATUS_OK);
  T64_CHECK(analog_stopped);
  family_status = eight_byte_store_status(SI_STEM_ID, UINT32_C(0xa4800000),
                                          &result, &family_stopped);
  /* The SI window refuses the 8-byte store exactly as the AI window does:
     the same status, nothing retired, no event queued and the SI registers
     untouched (the CPU stops on the bus refusal in both, which is why the
     digest is not compared for this row). */
  T64_CHECK_INT(family_status, analog_status);
  T64_CHECK(family_stopped);
  T64_CHECK_INT(result.retired, analog.retired);
  T64_CHECK_INT(result.queue_count, analog.queue_count);
  T64_CHECK(memcmp(&result.si_before, &family_si_after, sizeof family_si_after) ==
            0);
}

/* ------------------------------------------------------------------------ */
/* Bus helpers shared by the device cases                                    */
/* ------------------------------------------------------------------------ */

#define SI_DRAM_ADDR UINT32_C(0x04800000)
#define SI_RD64B UINT32_C(0x04800004)
#define SI_WR64B UINT32_C(0x04800010)
#define SI_STATUS UINT32_C(0x04800018)
#define PIF_ADDRESS UINT32_C(0x1FC007C0)
#define STATUS_BUSY UINT32_C(0x1)
#define STATUS_ERROR UINT32_C(0x8)
#define STATUS_INTERRUPT UINT32_C(0x1000)

static bool advance_guest_to(t64_machine *machine, uint64_t tick) {
  t64_stop_request request = T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK, tick);
  t64_stop_result result = T64_STOP_RESULT_INIT;
  t64_status status = t64_machine_advance(machine, &request, &result);

  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) return false;
  T64_CHECK_INT(result.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(result.tick, tick);
  return result.reason == T64_STOP_REASON_BOUND && result.tick == tick;
}

static uint32_t bus_read32(t64_machine *machine, uint32_t address) {
  uint32_t value = UINT32_MAX;

  T64_CHECK_INT(t64_machine_bus_read(machine, address, 4u, &value),
                T64_STATUS_OK);
  return value;
}

static uint8_t bus_read8(t64_machine *machine, uint32_t address) {
  uint32_t value = UINT32_MAX;

  T64_CHECK_INT(t64_machine_bus_read(machine, address, 1u, &value),
                T64_STATUS_OK);
  return (uint8_t)value;
}

static void bus_write_bytes(t64_machine *machine, uint32_t address,
                            const uint8_t *bytes, size_t count) {
  size_t index;

  for (index = 0u; index < count; index++) {
    T64_CHECK_INT(t64_machine_bus_write(machine, address + (uint32_t)index,
                                        1u, bytes[index]),
                  T64_STATUS_OK);
  }
}

/* A machine with the boot CPU event cancelled so the SI is driven by host
 * bus writes alone. */
static t64_machine *make_idle_machine(void) {
  t64_machine *machine = create_admitted(SI_STEM_ID);

  if (machine == NULL) return NULL;
  if (!cancel_boot_cpu_event(machine)) {
    t64_machine_destroy(machine);
    return NULL;
  }
  return machine;
}

/* ------------------------------------------------------------------------ */
/* tracer-poll                                                               */
/* ------------------------------------------------------------------------ */

/* Literal frame: channel 0 and channel 1 each carry command 0x01 expecting
 * four bytes, then the end escape, and byte 63 requests a parse. */
static const uint8_t poll_frame[64] = {
    0x01, 0x04, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
    0x01, 0x04, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFE, [63] = 0x01};

static void case_tracer_poll(void) {
  t64_machine *machine = make_idle_machine();
  uint8_t image[64];
  size_t index;
  /* Independent literals from the policy: entry 0 is channel 0 with one
     command byte and four response bytes (2000 + 188 + 438 + 8000 + 250 =
     10,876 ticks); entry 1 is the absent channel 1 (2000 + 188 + 3907 =
     6,095 ticks). H is their sum, X the read tail. */
  const uint64_t start = 3000u;
  const uint64_t handshake = 10876u + 6095u;
  const uint64_t execute = start + handshake;
  const uint64_t complete = execute + 4096u;

  if (machine == NULL) return;
  bus_write_bytes(machine, UINT32_C(0x3800), poll_frame, 64u);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_DRAM_ADDR, 4u, 0x3800u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_WR64B, 4u, PIF_ADDRESS),
                T64_STATUS_OK);
  T64_CHECK_INT(bus_read32(machine, SI_STATUS), STATUS_BUSY);
  T64_CHECK(advance_guest_to(machine, 2500u));
  T64_CHECK_INT(bus_read32(machine, SI_STATUS), STATUS_INTERRUPT);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_STATUS, 4u, 0u),
                T64_STATUS_OK);
  T64_CHECK_INT(bus_read32(machine, SI_STATUS), 0u);
  /* PIF RAM holds the frame with the command byte cleared and the parsed
     table has the two channels. */
  (void)memcpy(image, poll_frame, sizeof image);
  image[63] = 0u;
  T64_CHECK(memcmp(machine->si.pif_ram, image, sizeof image) == 0);
  T64_CHECK(machine->si.table.parsed);
  T64_CHECK_INT(machine->si.table.entry_count, 2u);

  T64_CHECK(advance_guest_to(machine, start));
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_DRAM_ADDR, 4u, 0x3900u),
                T64_STATUS_OK);
  /* State: A = A pressed, x 1, y -1, stamped one tick below E. */
  T64_CHECK_INT(t64_machine_queue_input(machine, execute - 1u, 0u, 0x8000u,
                                        1, -1),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_RD64B, 4u, PIF_ADDRESS),
                T64_STATUS_OK);
  T64_CHECK_INT(machine->si.execute_tick, execute);
  T64_CHECK_INT(machine->si.complete_tick, complete);
  T64_CHECK_INT(bus_read32(machine, SI_STATUS), STATUS_BUSY);

  /* One tick before completion: still busy, interrupt clear, destination
     bytes old (zero). */
  T64_CHECK(advance_guest_to(machine, complete - 1u));
  T64_CHECK_INT(bus_read32(machine, SI_STATUS), STATUS_BUSY);
  T64_CHECK_INT(bus_read8(machine, UINT32_C(0x3903)), 0u);
  /* At completion: idle, interrupt set, the image is in RDRAM. */
  T64_CHECK(advance_guest_to(machine, complete));
  T64_CHECK_INT(bus_read32(machine, SI_STATUS), STATUS_INTERRUPT);
  T64_CHECK_INT(bus_read8(machine, UINT32_C(0x3903)), 0x80u);
  T64_CHECK_INT(bus_read8(machine, UINT32_C(0x3904)), 0x00u);
  T64_CHECK_INT(bus_read8(machine, UINT32_C(0x3905)), 0x01u);
  T64_CHECK_INT(bus_read8(machine, UINT32_C(0x3906)), 0xFFu);
  T64_CHECK_INT(bus_read8(machine, UINT32_C(0x3901)), 0x04u);
  T64_CHECK_INT(bus_read8(machine, UINT32_C(0x3908)), 0x84u);
  for (index = 10u; index < 14u; index++) {
    T64_CHECK_INT(bus_read8(machine, UINT32_C(0x3900) + (uint32_t)index),
                  0xFFu);
  }
  t64_machine_destroy(machine);
}

/* ------------------------------------------------------------------------ */
/* Device-case helpers                                                       */
/* ------------------------------------------------------------------------ */

static const uint8_t frame_empty[64] = {0xFE, [63] = 0x01};
static const uint8_t frame_state_one[64] = {0x01, 0x04, 0x01, 0xFF, 0xFF,
                                            0xFF, 0xFF, 0xFE, [63] = 0x01};
static const uint8_t frame_absent_only[64] = {0x00, 0x01, 0x04, 0x01, 0xFF,
                                              0xFF, 0xFF, 0xFF, 0xFE,
                                              [63] = 0x01};

#define SRC_DRAM UINT32_C(0x2000)
#define DST_DRAM UINT32_C(0x2100)

static void put_frame(t64_machine *machine, uint32_t dram,
                      const uint8_t frame[64]) {
  (void)memcpy(machine->ram + dram, frame, 64u);
}

static t64_status start_dma(t64_machine *machine, uint32_t reg,
                            uint32_t dram) {
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_DRAM_ADDR, 4u, dram),
                T64_STATUS_OK);
  return t64_machine_bus_write(machine, reg, 4u, PIF_ADDRESS);
}

static void digest_of(t64_machine *machine, uint8_t digest[T64_DIGEST_SIZE]) {
  T64_CHECK_INT(t64_machine_digest(machine, digest), T64_STATUS_OK);
}

/* One complete accepted WR64B (frame in RDRAM at 0x3800, a parse request in
 * byte 63): advances past the 2,500-tick completion and acknowledges. */
static void settle_write(t64_machine *machine, const uint8_t frame[64]) {
  put_frame(machine, UINT32_C(0x3800), frame);
  T64_CHECK_INT(start_dma(machine, SI_WR64B, UINT32_C(0x3800)),
                T64_STATUS_OK);
  T64_CHECK(advance_guest_to(machine, machine->tick + 2500u));
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_STATUS, 4u, 0u),
                T64_STATUS_OK);
}

/* ------------------------------------------------------------------------ */
/* digest-section (tracer part)                                              */
/* ------------------------------------------------------------------------ */

#define SPAN_CAPACITY 4096u
#define AI_PROFILE_ID_V24 AI_STEM_ID

typedef struct {
  uint8_t *bytes;
  size_t length;
  t64_digest_span spans[SPAN_CAPACITY];
  size_t span_count;
} stream;

static bool capture_stream(const t64_machine *machine, stream *out) {
  t64_digest_sink sink;
  size_t length;

  (void)memset(out, 0, sizeof *out);
  (void)memset(&sink, 0, sizeof sink);
  T64_CHECK_INT(t64_digest_stream(machine, &sink), T64_STATUS_OK);
  length = sink.offset;
  out->bytes = (uint8_t *)malloc(length > 0u ? length : 1u);
  if (out->bytes == NULL) {
    T64_CHECK(false);
    return false;
  }
  (void)memset(&sink, 0, sizeof sink);
  sink.buffer = out->bytes;
  sink.capacity = length;
  sink.spans = out->spans;
  sink.span_capacity = SPAN_CAPACITY;
  T64_CHECK_INT(t64_digest_stream(machine, &sink), T64_STATUS_OK);
  T64_CHECK(!sink.overflow);
  T64_CHECK_INT(sink.offset, length);
  out->length = length;
  out->span_count = sink.span_count;
  return !sink.overflow && sink.offset == length;
}

/* Body bytes outside every non-body span. */
static size_t strip_stream(const stream *captured, uint8_t *out) {
  size_t position = 0u;
  size_t written = 0u;
  size_t index;

  for (index = 0u; index <= captured->span_count; index++) {
    const size_t end = index < captured->span_count
                           ? captured->spans[index].offset
                           : captured->length;

    if (end > position) {
      if (out != NULL) {
        (void)memcpy(out + written, captured->bytes + position,
                     end - position);
      }
      written += end - position;
    }
    if (index < captured->span_count) {
      position = captured->spans[index].offset + captured->spans[index].length;
    }
  }
  return written;
}

static uint32_t be32(const uint8_t *bytes) {
  return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
         ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

/* The marker span count and ascending ids; returns the span index of marker
 * `id` or SIZE_MAX. */
static size_t find_marker_span(const stream *captured, uint32_t id) {
  size_t index;
  uint32_t seen = 0u;

  for (index = 0u; index < captured->span_count; index++) {
    if (captured->spans[index].class != T64_DIGEST_CLASS_MARKER) continue;
    seen++;
    if (be32(captured->bytes + captured->spans[index].offset) == id) {
      return seen == id ? index : SIZE_MAX;
    }
  }
  return SIZE_MAX;
}

/* ---- section 10: independent reference encoder and field flips ----------- */

typedef struct {
  uint8_t bytes[1024];
  size_t length;
} blob;

static void blob_u32(blob *out, uint32_t value) {
  if (out->length + 4u > sizeof out->bytes) {
    T64_CHECK(false);
    return;
  }
  out->bytes[out->length++] = (uint8_t)(value >> 24);
  out->bytes[out->length++] = (uint8_t)(value >> 16);
  out->bytes[out->length++] = (uint8_t)(value >> 8);
  out->bytes[out->length++] = (uint8_t)value;
}

static void blob_u64(blob *out, uint64_t value) {
  blob_u32(out, (uint32_t)(value >> 32));
  blob_u32(out, (uint32_t)value);
}

/* Written from the section 10 field table of docs/spec/si-pif-controller.md
 * (big-endian, in the new-span class); it reads only the SI state fields and
 * calls no production digest code. */
static void encode_section10(blob *out, const t64_si_state *si) {
  size_t index;

  out->length = 0u;
  blob_u32(out, si->dram_address);
  blob_u32(out, si->error ? 1u : 0u);
  blob_u32(out, si->active_valid ? 1u : 0u);
  blob_u32(out, si->active_direction);
  blob_u32(out, si->active_dram_address);
  blob_u64(out, si->start_tick);
  blob_u64(out, si->execute_tick);
  blob_u64(out, si->complete_tick);
  blob_u64(out, si->active_token);
  blob_u64(out, si->next_token);
  blob_u32(out, si->token_exhausted ? 1u : 0u);
  blob_u32(out, si->execute_pending ? 1u : 0u);
  for (index = 0u; index < 64u; index++) {
    out->bytes[out->length++] = si->pif_ram[index];
  }
  blob_u32(out, si->table.parsed ? 1u : 0u);
  blob_u32(out, si->table.entry_count);
  for (index = 0u; index < 5u; index++) {
    blob_u32(out, si->table.entries[index].kind);
    blob_u32(out, si->table.entries[index].channel);
    blob_u32(out, si->table.entries[index].offset);
    blob_u32(out, si->table.entries[index].tx_count);
    blob_u32(out, si->table.entries[index].rx_count);
    blob_u32(out, si->table.entries[index].flags);
  }
  for (index = 0u; index < 4u; index++) {
    blob_u32(out, si->latch[index].current_buttons);
    blob_u32(out, si->latch[index].current_x);
    blob_u32(out, si->latch[index].current_y);
    blob_u32(out, si->latch[index].next_buttons);
    blob_u32(out, si->latch[index].next_x);
    blob_u32(out, si->latch[index].next_y);
    blob_u64(out, si->latch[index].next_visible_from);
    blob_u32(out, si->latch[index].next_valid ? 1u : 0u);
  }
}

static void check_encoder_matches(const char *label, t64_machine *machine) {
  stream captured;
  blob expected;
  size_t marker;

  if (!capture_stream(machine, &captured)) return;
  marker = find_marker_span(&captured, 10u);
  T64_CHECK(marker != SIZE_MAX);
  if (marker != SIZE_MAX) {
    const size_t begin = captured.spans[marker].offset +
                         captured.spans[marker].length;

    encode_section10(&expected, &machine->si);
    T64_CHECK_INT(captured.length - begin, expected.length);
    if (captured.length - begin != expected.length ||
        memcmp(captured.bytes + begin, expected.bytes, expected.length) != 0) {
      fprintf(stderr, "section 10 differs from the reference in state '%s'\n",
              label);
      T64_CHECK(false);
    }
  }
  free(captured.bytes);
}

typedef struct {
  const char *name;
  size_t offset;
} flip_field;

#define FLIP(member) {#member, offsetof(t64_si_state, member)}

static const flip_field flip_fields[] = {
    FLIP(dram_address), FLIP(error), FLIP(active_valid),
    FLIP(active_direction), FLIP(active_dram_address), FLIP(start_tick),
    FLIP(execute_tick), FLIP(complete_tick), FLIP(active_token),
    FLIP(next_token), FLIP(token_exhausted), FLIP(execute_pending),
    FLIP(pif_ram[0]), FLIP(pif_ram[31]), FLIP(pif_ram[63]),
    FLIP(table.parsed), FLIP(table.entry_count),
    FLIP(table.entries[0].kind), FLIP(table.entries[0].channel),
    FLIP(table.entries[0].offset), FLIP(table.entries[0].tx_count),
    FLIP(table.entries[0].rx_count), FLIP(table.entries[0].flags),
    FLIP(table.entries[4].kind), FLIP(table.entries[4].channel),
    FLIP(table.entries[4].offset), FLIP(table.entries[4].tx_count),
    FLIP(table.entries[4].rx_count), FLIP(table.entries[4].flags),
    FLIP(latch[0].current_buttons), FLIP(latch[0].current_x),
    FLIP(latch[0].current_y), FLIP(latch[0].next_buttons),
    FLIP(latch[0].next_x), FLIP(latch[0].next_y),
    FLIP(latch[0].next_visible_from), FLIP(latch[0].next_valid),
    FLIP(latch[3].current_buttons), FLIP(latch[3].current_x),
    FLIP(latch[3].current_y), FLIP(latch[3].next_buttons),
    FLIP(latch[3].next_x), FLIP(latch[3].next_y),
    FLIP(latch[3].next_visible_from), FLIP(latch[3].next_valid),
};

static void check_field_flips(t64_machine *machine) {
  uint8_t baseline[T64_DIGEST_SIZE];
  uint8_t flipped[T64_DIGEST_SIZE];
  size_t index;

  digest_of(machine, baseline);
  for (index = 0u; index < sizeof flip_fields / sizeof flip_fields[0];
       index++) {
    unsigned char *byte = (unsigned char *)&machine->si + flip_fields[index].offset;

    *byte ^= 1u;
    digest_of(machine, flipped);
    if (memcmp(baseline, flipped, T64_DIGEST_SIZE) == 0) {
      fprintf(stderr, "field '%s' does not alter the digest\n",
              flip_fields[index].name);
      T64_CHECK(false);
    }
    *byte ^= 1u;
  }
  digest_of(machine, flipped);
  T64_CHECK(memcmp(baseline, flipped, T64_DIGEST_SIZE) == 0);
}

static void check_section_reference(void) {
  t64_machine *machine;
  size_t port;

  /* Mid read DMA after the execute event: parsed table, executed PIF image,
     promoted non-neutral latch. */
  machine = make_idle_machine();
  if (machine == NULL) return;
  settle_write(machine, poll_frame);
  T64_CHECK(advance_guest_to(machine, 3000u));
  T64_CHECK_INT(t64_machine_queue_input(machine, 3100u, 0u, 0x8000u, 1, -1),
                T64_STATUS_OK);
  T64_CHECK_INT(start_dma(machine, SI_RD64B, DST_DRAM), T64_STATUS_OK);
  T64_CHECK(advance_guest_to(machine, 3000u + 16971u + 10u));
  T64_CHECK(machine->si.active_valid && !machine->si.execute_pending);
  check_encoder_matches("mid read after execute", machine);
  /* Non-neutral values on every port, an exhausted token and an error flag
     by direct state, covering fields no scenario reaches. */
  for (port = 0u; port < 4u; port++) {
    machine->si.latch[port].current_buttons = (uint16_t)(0x1111u * (port + 1u));
    machine->si.latch[port].current_x = (uint8_t)(0x10u + port);
    machine->si.latch[port].current_y = (uint8_t)(0x20u + port);
    machine->si.latch[port].next_buttons = (uint16_t)(0x2222u * (port + 1u));
    machine->si.latch[port].next_x = (uint8_t)(0x30u + port);
    machine->si.latch[port].next_y = (uint8_t)(0x40u + port);
    machine->si.latch[port].next_visible_from = 0x0102030405060708u + port;
    machine->si.latch[port].next_valid = (port & 1u) != 0u;
  }
  machine->si.error = true;
  machine->si.token_exhausted = true;
  machine->si.next_token = 0x1122334455667788u;
  check_encoder_matches("direct-state latch and flags", machine);
  /* Every field family changes the digest when flipped; the table entries
     beyond the two parsed ones are exercised through direct state. */
  machine->si.table.entries[4].kind = 1u;
  machine->si.table.entries[4].channel = 4u;
  machine->si.table.entries[4].offset = 50u;
  machine->si.table.entries[4].tx_count = 2u;
  machine->si.table.entries[4].rx_count = 3u;
  machine->si.table.entries[4].flags = 0x40u;
  check_encoder_matches("direct-state table entry", machine);
  check_field_flips(machine);
  t64_machine_destroy(machine);

  /* Mid write DMA: parsed at the start tick, completion pending. */
  machine = make_idle_machine();
  if (machine == NULL) return;
  put_frame(machine, UINT32_C(0x3800), poll_frame);
  T64_CHECK(advance_guest_to(machine, 100u));
  T64_CHECK_INT(start_dma(machine, SI_WR64B, UINT32_C(0x3800)), T64_STATUS_OK);
  T64_CHECK(advance_guest_to(machine, 1000u));
  T64_CHECK_INT(machine->si.active_direction, 1u);
  check_encoder_matches("mid write", machine);
  t64_machine_destroy(machine);

  /* Mid read before the execute event with a pending, not yet promoted
     input value. */
  machine = make_idle_machine();
  if (machine == NULL) return;
  settle_write(machine, poll_frame);
  T64_CHECK(advance_guest_to(machine, 3000u));
  T64_CHECK_INT(t64_machine_queue_input(machine, 3050u, 0u, 0x0123u, 7, -7),
                T64_STATUS_OK);
  T64_CHECK_INT(start_dma(machine, SI_RD64B, DST_DRAM), T64_STATUS_OK);
  T64_CHECK(advance_guest_to(machine, 3100u));
  T64_CHECK(machine->si.execute_pending);
  T64_CHECK(machine->si.latch[0].next_valid);
  check_encoder_matches("mid read before execute", machine);
  t64_machine_destroy(machine);
}

static void case_digest_section(void) {
  t64_machine *family = make_idle_machine();
  t64_machine *phase8 = NULL;
  stream family_stream;
  stream phase8_stream;
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  uint32_t id;

  if (family == NULL) return;
  phase8 = create_admitted(AI_PROFILE_ID_V24);
  if (phase8 == NULL || !cancel_boot_cpu_event(phase8) ||
      !capture_stream(family, &family_stream)) {
    t64_machine_destroy(family);
    t64_machine_destroy(phase8);
    return;
  }
  if (!capture_stream(phase8, &phase8_stream)) {
    free(family_stream.bytes);
    t64_machine_destroy(family);
    t64_machine_destroy(phase8);
    return;
  }

  /* Markers 1 to 10 are present, ascending, and section 10 is the last one:
     its body is one new-class span running to the end of the stream. */
  for (id = 1u; id <= 10u; id++) {
    T64_CHECK(find_marker_span(&family_stream, id) != SIZE_MAX);
  }
  T64_CHECK(find_marker_span(&family_stream, 11u) == SIZE_MAX);
  {
    const size_t marker = find_marker_span(&family_stream, 10u);

    T64_CHECK(marker != SIZE_MAX);
    if (marker != SIZE_MAX && marker + 1u < family_stream.span_count) {
      const t64_digest_span *body = &family_stream.spans[marker + 1u];

      T64_CHECK_INT(body->class, T64_DIGEST_CLASS_NEW);
      T64_CHECK_INT(body->offset,
                    family_stream.spans[marker].offset +
                        family_stream.spans[marker].length);
      T64_CHECK_INT(body->offset + body->length, family_stream.length);
      T64_CHECK_INT(marker + 2u, family_stream.span_count);
    } else {
      T64_CHECK(false);
    }
  }
  /* Strip differential: with markers, identity spans and the new section
     removed, the family stream equals the Phase 8 stream. */
  {
    const size_t old_length = strip_stream(&phase8_stream, NULL);
    const size_t new_length = strip_stream(&family_stream, NULL);
    uint8_t *old_body = (uint8_t *)malloc(old_length > 0u ? old_length : 1u);
    uint8_t *new_body = (uint8_t *)malloc(new_length > 0u ? new_length : 1u);

    T64_CHECK_INT(new_length, old_length);
    if (old_body != NULL && new_body != NULL && old_length == new_length) {
      (void)strip_stream(&phase8_stream, old_body);
      (void)strip_stream(&family_stream, new_body);
      T64_CHECK(memcmp(old_body, new_body, old_length) == 0);
    } else {
      T64_CHECK(false);
    }
    free(old_body);
    free(new_body);
  }
  /* One PIF byte flip changes the digest. */
  T64_CHECK_INT(t64_machine_digest(family, digest_before), T64_STATUS_OK);
  family->si.pif_ram[5] ^= 0x01u;
  T64_CHECK_INT(t64_machine_digest(family, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, T64_DIGEST_SIZE) != 0);
  family->si.pif_ram[5] ^= 0x01u;
  T64_CHECK_INT(t64_machine_digest(family, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, T64_DIGEST_SIZE) == 0);

  free(family_stream.bytes);
  free(phase8_stream.bytes);
  t64_machine_destroy(family);
  t64_machine_destroy(phase8);

  check_section_reference();
}

/* ------------------------------------------------------------------------ */
/* registers                                                                 */
/* ------------------------------------------------------------------------ */

typedef struct {
  bool source_moved_to_pif;      /* PIF RAM equals the source image */
  bool source_unchanged;         /* RDRAM source untouched */
  bool pif_moved_to_destination; /* destination equals the PIF image */
  bool pif_unchanged;            /* PIF RAM untouched by the read */
} direction_outcome;

/* The expectation of a correct model: write moves RDRAM to PIF and leaves
 * RDRAM alone; read moves PIF to RDRAM and leaves PIF alone. */
static bool direction_matches(const direction_outcome *actual,
                              const direction_outcome *expected) {
  return actual->source_moved_to_pif == expected->source_moved_to_pif &&
         actual->source_unchanged == expected->source_unchanged &&
         actual->pif_moved_to_destination ==
             expected->pif_moved_to_destination &&
         actual->pif_unchanged == expected->pif_unchanged;
}

static void case_registers(void) {
  t64_machine *machine = make_idle_machine();
  uint32_t value = UINT32_MAX;
  uint8_t source[64];
  uint8_t destination_before[64];
  uint8_t pif_after_write[64];
  direction_outcome actual;
  const direction_outcome expected = {true, true, true, true};
  const direction_outcome swapped = {false, false, false, false};
  static const uint32_t unmodeled[] = {0x08u, 0x0Cu, 0x14u, 0x1Cu, 0x44u};
  size_t index;

  if (machine == NULL) return;
  /* DRAM_ADDR keeps 24 bits. */
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_DRAM_ADDR, 4u, 0xFF123456u),
                T64_STATUS_OK);
  T64_CHECK_INT(bus_read32(machine, SI_DRAM_ADDR), 0x00123456u);
  /* RD64B and WR64B are write-only; every other offset is unmodeled. */
  T64_CHECK_INT(t64_machine_bus_read(machine, SI_RD64B, 4u, &value),
                T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(t64_machine_bus_read(machine, SI_WR64B, 4u, &value),
                T64_STATUS_BUS_UNSUPPORTED);
  for (index = 0u; index < sizeof unmodeled / sizeof unmodeled[0]; index++) {
    T64_CHECK_INT(t64_machine_bus_read(machine, SI_DRAM_ADDR + unmodeled[index],
                                       4u, &value),
                  T64_STATUS_BUS_UNSUPPORTED);
    T64_CHECK_INT(t64_machine_bus_write(machine,
                                        SI_DRAM_ADDR + unmodeled[index], 4u,
                                        0u),
                  T64_STATUS_BUS_UNSUPPORTED);
  }
  /* Only 32-bit accesses decode; narrower accesses are unsupported and the
     wide forms are refused by the window exactly as the AI window refuses
     them. */
  T64_CHECK_INT(t64_machine_bus_read(machine, SI_DRAM_ADDR, 1u, &value),
                T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(t64_machine_bus_read(machine, SI_STATUS, 2u, &value),
                T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_DRAM_ADDR, 1u, 1u),
                T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_STATUS, 2u, 0u),
                T64_STATUS_BUS_UNSUPPORTED);
  {
    uint64_t wide = 0u;

    T64_CHECK_INT(t64_bus_read_wide(machine, SI_DRAM_ADDR, &wide),
                  T64_STATUS_BUS_DEVICE_REFUSED);
    T64_CHECK_INT(t64_bus_write_masked(machine, SI_DRAM_ADDR, 8u, 0xFFu, 0u),
                  T64_STATUS_BUS_DEVICE_REFUSED);
  }

  /* Direction with distinct source and destination bytes. */
  for (index = 0u; index < 64u; index++) {
    source[index] = (uint8_t)(0x40u + index);
  }
  source[63] = 0u; /* replace the PIF RAM without parsing */
  put_frame(machine, SRC_DRAM, source);
  (void)memset(machine->ram + DST_DRAM, 0xEE, 64u);
  (void)memcpy(destination_before, machine->ram + DST_DRAM, 64u);
  T64_CHECK_INT(start_dma(machine, SI_WR64B, SRC_DRAM), T64_STATUS_OK);
  T64_CHECK(advance_guest_to(machine, 2500u));
  (void)memcpy(pif_after_write, machine->si.pif_ram, 64u);
  actual.source_moved_to_pif = memcmp(machine->si.pif_ram, source, 64u) == 0;
  actual.source_unchanged =
      memcmp(machine->ram + SRC_DRAM, source, 64u) == 0;
  /* The write has not touched the destination yet. */
  T64_CHECK(memcmp(machine->ram + DST_DRAM, destination_before, 64u) == 0);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_STATUS, 4u, 0u),
                T64_STATUS_OK);
  T64_CHECK_INT(start_dma(machine, SI_RD64B, DST_DRAM), T64_STATUS_OK);
  T64_CHECK(advance_guest_to(machine, 2500u + 4096u));
  actual.pif_moved_to_destination =
      memcmp(machine->ram + DST_DRAM, pif_after_write, 64u) == 0;
  actual.pif_unchanged =
      memcmp(machine->si.pif_ram, pif_after_write, 64u) == 0;
  T64_CHECK(direction_matches(&actual, &expected));
  /* The comparator rejects the swapped expectation (RD64B taken as RDRAM to
     PIF and WR64B as PIF to RDRAM). */
  T64_CHECK(!direction_matches(&actual, &swapped));
  t64_machine_destroy(machine);
}

/* ------------------------------------------------------------------------ */
/* status                                                                    */
/* ------------------------------------------------------------------------ */

#define STATUS_OTHER_BITS (~(STATUS_BUSY | STATUS_ERROR | STATUS_INTERRUPT))

static void case_status(void) {
  t64_machine *machine = make_idle_machine();
  const uint64_t start = 0u;
  const uint64_t complete = start + 4096u; /* empty table, E == S */
  uint32_t mask_before;
  uint32_t pending_before;

  if (machine == NULL) return;
  T64_CHECK_INT(start_dma(machine, SI_RD64B, DST_DRAM), T64_STATUS_OK);
  /* Busy from the start tick to one tick before completion; the interrupt
     and error bits are clear; every other bit reads zero. */
  T64_CHECK_INT(bus_read32(machine, SI_STATUS), STATUS_BUSY);
  T64_CHECK(advance_guest_to(machine, start + 1u));
  T64_CHECK_INT(bus_read32(machine, SI_STATUS), STATUS_BUSY);
  T64_CHECK(advance_guest_to(machine, complete - 1u));
  T64_CHECK_INT(bus_read32(machine, SI_STATUS) & STATUS_OTHER_BITS, 0u);
  T64_CHECK_INT(bus_read32(machine, SI_STATUS), STATUS_BUSY);
  T64_CHECK(advance_guest_to(machine, complete));
  T64_CHECK_INT(bus_read32(machine, SI_STATUS), STATUS_INTERRUPT);

  /* Any STATUS write acknowledges only the SI interrupt: other pending MI
     bits and the mask are untouched. */
  t64_mi_set_pending(machine, T64_MI_INTERRUPT_AI | UINT32_C(0x10));
  machine->mi_mask = UINT32_C(0x15);
  mask_before = machine->mi_mask;
  pending_before = machine->mi_pending;
  T64_CHECK((pending_before & T64_MI_INTERRUPT_SI) != 0u);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_STATUS, 4u, 0u),
                T64_STATUS_OK);
  T64_CHECK_INT(machine->mi_pending, pending_before & ~T64_MI_INTERRUPT_SI);
  T64_CHECK_INT(machine->mi_mask, mask_before);
  T64_CHECK_INT(bus_read32(machine, SI_STATUS), 0u);
  /* The all-ones write after a second completion does the same. */
  T64_CHECK_INT(start_dma(machine, SI_RD64B, DST_DRAM), T64_STATUS_OK);
  T64_CHECK(advance_guest_to(machine, complete + 4096u));
  T64_CHECK_INT(bus_read32(machine, SI_STATUS), STATUS_INTERRUPT);
  pending_before = machine->mi_pending;
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_STATUS, 4u, 0xFFFFFFFFu),
                T64_STATUS_OK);
  T64_CHECK_INT(machine->mi_pending, pending_before & ~T64_MI_INTERRUPT_SI);
  T64_CHECK_INT(machine->mi_mask, mask_before);
  T64_CHECK_INT(bus_read32(machine, SI_STATUS), 0u);

  /* Bit 3 is sticky: a misaligned start sets it, a STATUS write and a later
     accepted DMA leave it set. */
  T64_CHECK_INT(start_dma(machine, SI_RD64B, UINT32_C(0x3804)),
                T64_STATUS_BUS_DEVICE_REFUSED);
  T64_CHECK_INT(bus_read32(machine, SI_STATUS), STATUS_ERROR);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_STATUS, 4u, 0u),
                T64_STATUS_OK);
  T64_CHECK_INT(bus_read32(machine, SI_STATUS), STATUS_ERROR);
  T64_CHECK_INT(start_dma(machine, SI_RD64B, DST_DRAM), T64_STATUS_OK);
  T64_CHECK_INT(bus_read32(machine, SI_STATUS), STATUS_ERROR | STATUS_BUSY);
  T64_CHECK(advance_guest_to(machine, complete + 2u * 4096u));
  T64_CHECK_INT(bus_read32(machine, SI_STATUS), STATUS_ERROR | STATUS_INTERRUPT);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_STATUS, 4u, 0u),
                T64_STATUS_OK);
  T64_CHECK_INT(bus_read32(machine, SI_STATUS), STATUS_ERROR);
  t64_machine_destroy(machine);
}

/* ------------------------------------------------------------------------ */
/* pif-command                                                               */
/* ------------------------------------------------------------------------ */

static void case_pif_command(void) {
  t64_machine *machine = make_idle_machine();
  uint8_t frame[64];
  uint8_t second[64];
  uint8_t image[64];
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  t64_si_state before;
  static const uint8_t bad_bytes[] = {0x02u, 0x04u, 0x08u, 0x10u,
                                      0x20u, 0x40u, 0x80u, 0x03u};
  size_t index;

  if (machine == NULL) return;
  /* 0x01 parses and then clears the byte. */
  settle_write(machine, poll_frame);
  (void)memcpy(image, poll_frame, 64u);
  image[63] = 0u;
  T64_CHECK(memcmp(machine->si.pif_ram, image, 64u) == 0);
  T64_CHECK(machine->si.table.parsed);
  T64_CHECK_INT(machine->si.table.entry_count, 2u);

  /* 0x00 replaces the RAM without re-parsing: the table of the first frame
     stays, and the next read re-executes it over the new RAM bytes. */
  (void)memcpy(second, poll_frame, 64u);
  second[10] = 0xAAu;
  second[11] = 0xAAu;
  second[12] = 0xAAu;
  second[13] = 0xAAu;
  second[14] = 0xFEu;
  second[63] = 0x00u;
  settle_write(machine, second);
  T64_CHECK(machine->si.table.parsed);
  T64_CHECK_INT(machine->si.table.entry_count, 2u);
  T64_CHECK_INT(machine->si.pif_ram[10], 0xAAu);
  (void)memset(machine->ram + DST_DRAM, 0, 64u);
  T64_CHECK_INT(start_dma(machine, SI_RD64B, DST_DRAM), T64_STATUS_OK);
  T64_CHECK(advance_guest_to(machine, machine->si.complete_tick));
  T64_CHECK_INT(machine->ram[DST_DRAM + 8u], 0x84u);
  T64_CHECK_INT(machine->ram[DST_DRAM + 10u], 0xAAu);
  T64_CHECK_INT(machine->ram[DST_DRAM + 13u], 0xAAu);
  T64_CHECK_INT(machine->si.table.entry_count, 2u);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_STATUS, 4u, 0u),
                T64_STATUS_OK);

  /* Every other command-byte bit is an unmodeled control function: the start
     is refused with PIF RAM, the table, the error flag and the digest
     unchanged. The accepted baseline above ran in the same setup. */
  for (index = 0u; index < sizeof bad_bytes; index++) {
    (void)memcpy(frame, poll_frame, 64u);
    frame[63] = bad_bytes[index];
    put_frame(machine, UINT32_C(0x3800), frame);
    T64_CHECK_INT(t64_machine_bus_write(machine, SI_DRAM_ADDR, 4u, 0x3800u),
                  T64_STATUS_OK);
    before = machine->si;
    digest_of(machine, digest_before);
    T64_CHECK_INT(t64_machine_bus_write(machine, SI_WR64B, 4u, PIF_ADDRESS),
                  T64_STATUS_BUS_DEVICE_REFUSED);
    T64_CHECK(memcmp(&before, &machine->si, sizeof before) == 0);
    T64_CHECK(!machine->si.error);
    digest_of(machine, digest_after);
    T64_CHECK(memcmp(digest_before, digest_after, T64_DIGEST_SIZE) == 0);
  }
  t64_machine_destroy(machine);
}

/* ------------------------------------------------------------------------ */
/* dma-timing                                                                */
/* ------------------------------------------------------------------------ */

typedef enum {
  TIMING_EQUAL = 0,
  TIMING_EXECUTE_MISMATCH = 1,
  TIMING_COMPLETE_MISMATCH = 2
} timing_kind;

static timing_kind compare_timing(uint64_t expect_execute,
                                  uint64_t expect_complete,
                                  uint64_t actual_execute,
                                  uint64_t actual_complete) {
  if (expect_execute != actual_execute) return TIMING_EXECUTE_MISMATCH;
  if (expect_complete != actual_complete) return TIMING_COMPLETE_MISMATCH;
  return TIMING_EQUAL;
}

typedef struct {
  const char *label;
  const uint8_t *frame;
  uint64_t handshake; /* independent literal, policy ticks */
} timing_row;

static void run_timing_row(const timing_row *row) {
  t64_machine *machine = make_idle_machine();
  const uint64_t start = 3000u;
  const uint64_t execute = start + row->handshake;
  const uint64_t complete = execute + 4096u;

  if (machine == NULL) return;
  settle_write(machine, row->frame);
  T64_CHECK(advance_guest_to(machine, start));
  T64_CHECK_INT(start_dma(machine, SI_RD64B, DST_DRAM), T64_STATUS_OK);
  T64_CHECK_INT(compare_timing(execute, complete, machine->si.execute_tick,
                               machine->si.complete_tick),
                TIMING_EQUAL);
  /* The comparator reports a one-tick error as a mismatch kind. */
  T64_CHECK_INT(compare_timing(execute + 1u, complete,
                               machine->si.execute_tick,
                               machine->si.complete_tick),
                TIMING_EXECUTE_MISMATCH);
  T64_CHECK_INT(compare_timing(execute, complete + 1u,
                               machine->si.execute_tick,
                               machine->si.complete_tick),
                TIMING_COMPLETE_MISMATCH);
  if (row->handshake > 0u) {
    T64_CHECK(advance_guest_to(machine, execute - 1u));
    T64_CHECK(machine->si.execute_pending);
  }
  T64_CHECK(advance_guest_to(machine, execute));
  T64_CHECK(!machine->si.execute_pending);
  T64_CHECK(advance_guest_to(machine, complete - 1u));
  T64_CHECK_INT(bus_read32(machine, SI_STATUS), STATUS_BUSY);
  T64_CHECK(advance_guest_to(machine, complete));
  T64_CHECK_INT(bus_read32(machine, SI_STATUS), STATUS_INTERRUPT);
  if (row->handshake == 0u) {
    fprintf(stderr, "timing row '%s' ok\n", row->label);
  }
  t64_machine_destroy(machine);
}

/* ------------------------------------------------------------------------ */
/* Plan 09-10 (review L5 WR-01 and WR-03, L1 WR-02): the active descriptor is  */
/* immutable and the RD64B completion invalidates what it overwrites           */
/* ------------------------------------------------------------------------ */

#define OTHER_DRAM UINT32_C(0x4000)
#define CANARY 0xCDu

/* A write to SI_DRAM_ADDR while a DMA is active changes only the latch: the
 * transfer uses the address latched at its start. Each arm runs next to the
 * accepted baseline of the same setup (no rewrite of the latch), and the
 * register reads the rewritten latch afterwards. */
static void check_active_descriptor_immutable(void) {
  uint8_t pattern[64];
  size_t byte;
  unsigned rewrite;

  for (byte = 0u; byte < 64u; byte++) pattern[byte] = (uint8_t)(0x31u + byte);
  pattern[63] = 0u;
  for (rewrite = 0u; rewrite < 2u; rewrite++) {
    /* RD64B: the image of the PIF RAM lands at the latched address */
    t64_machine *machine = make_idle_machine();

    if (machine == NULL) return;
    settle_write(machine, pattern);
    T64_CHECK(memcmp(machine->si.pif_ram, pattern, 64u) == 0);
    (void)memset(machine->ram + DST_DRAM, 0xEE, 64u);
    (void)memset(machine->ram + OTHER_DRAM, CANARY, 64u);
    T64_CHECK_INT(start_dma(machine, SI_RD64B, DST_DRAM), T64_STATUS_OK);
    T64_CHECK(advance_guest_to(machine, machine->tick + 1000u));
    if (rewrite != 0u) {
      T64_CHECK_INT(t64_machine_bus_write(machine, SI_DRAM_ADDR, 4u,
                                          OTHER_DRAM),
                    T64_STATUS_OK);
      T64_CHECK_INT(bus_read32(machine, SI_DRAM_ADDR), OTHER_DRAM);
    }
    T64_CHECK(advance_guest_to(machine, machine->si.complete_tick));
    T64_CHECK(memcmp(machine->ram + DST_DRAM, pattern, 64u) == 0);
    {
      uint8_t canary[64];

      (void)memset(canary, CANARY, sizeof canary);
      T64_CHECK(memcmp(machine->ram + OTHER_DRAM, canary, 64u) == 0);
    }
    T64_CHECK_INT(bus_read32(machine, SI_DRAM_ADDR),
                  rewrite != 0u ? OTHER_DRAM : DST_DRAM);
    t64_machine_destroy(machine);

    /* WR64B: the source was read at the start, so a later latch rewrite and a
     * later change of the bytes at the new latch change nothing */
    machine = make_idle_machine();
    if (machine == NULL) return;
    put_frame(machine, SRC_DRAM, pattern);
    (void)memset(machine->ram + OTHER_DRAM, 0x5A, 64u);
    T64_CHECK_INT(start_dma(machine, SI_WR64B, SRC_DRAM), T64_STATUS_OK);
    if (rewrite != 0u) {
      T64_CHECK_INT(t64_machine_bus_write(machine, SI_DRAM_ADDR, 4u,
                                          OTHER_DRAM),
                    T64_STATUS_OK);
    }
    T64_CHECK(advance_guest_to(machine, machine->si.complete_tick));
    T64_CHECK(memcmp(machine->si.pif_ram, pattern, 64u) == 0);
    T64_CHECK_INT(bus_read32(machine, SI_DRAM_ADDR),
                  rewrite != 0u ? OTHER_DRAM : SRC_DRAM);
    t64_machine_destroy(machine);
  }
}

/* The RD64B completion clears the RDP memory coverage marks and the CPU link of
 * the span it overwrote and nothing outside it; the baseline arm of the same
 * setup completes the DMA to a span that does not overlap them. */
static void check_rd64b_invalidation(void) {
  uint8_t pattern[64];
  size_t byte;
  unsigned overlaps;

  for (byte = 0u; byte < 64u; byte++) pattern[byte] = (uint8_t)(0x71u + byte);
  pattern[63] = 0u;
  for (overlaps = 0u; overlaps < 2u; overlaps++) {
    t64_machine *machine = make_idle_machine();
    const uint32_t destination = overlaps != 0u ? DST_DRAM : DST_DRAM + 0x400u;
    uint32_t offset;

    if (machine == NULL) return;
    settle_write(machine, pattern);
    for (offset = 0u; offset < 68u; offset += 2u) {
      t64_rdp_memory_coverage_write(machine, DST_DRAM - 2u + offset, 2u, 8u);
    }
    t64_rdp_memory_coverage_write(machine, 0x5000u, 2u, 8u);
    machine->cpu.linked_valid = true;
    machine->cpu.linked_physical_address = DST_DRAM + 8u;
    machine->cpu.linked_width = 4u;
    T64_CHECK_INT(start_dma(machine, SI_RD64B, destination), T64_STATUS_OK);
    T64_CHECK(advance_guest_to(machine, machine->si.complete_tick));
    T64_CHECK(memcmp(machine->ram + destination, pattern, 64u) == 0);
    T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, 0x5000u, 2u), 8u);
    if (overlaps != 0u) {
      for (offset = 0u; offset < 64u; offset += 2u) {
        T64_CHECK_INT(
            t64_rdp_memory_coverage_read(machine, DST_DRAM + offset, 2u), 0u);
      }
      T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, DST_DRAM - 2u, 2u), 8u);
      T64_CHECK_INT(t64_rdp_memory_coverage_read(machine, DST_DRAM + 64u, 2u),
                    8u);
      T64_CHECK(!machine->cpu.linked_valid);
    } else {
      for (offset = 0u; offset < 64u; offset += 2u) {
        T64_CHECK_INT(
            t64_rdp_memory_coverage_read(machine, DST_DRAM + offset, 2u), 8u);
      }
      T64_CHECK(machine->cpu.linked_valid);
      T64_CHECK_INT(machine->cpu.linked_physical_address, DST_DRAM + 8u);
    }
    t64_machine_destroy(machine);
  }
}

static void case_dma_timing(void) {
  /* H from the D-14 literals: present device 2000n + 188 + 438 + 2000m + 250
     with n 1 and m 4 is 10,876; an absent channel 2000n + 188 + 3,907 with
     n 1 is 6,095; an empty frame is 0; the two-channel pad-poll frame is
     their sum. X is the 4,096-tick read tail. */
  const timing_row rows[] = {
      {"one controller state command", frame_state_one, 10876u},
      {"one absent channel", frame_absent_only, 6095u},
      {"empty frame", frame_empty, 0u},
      {"pad-poll two-channel frame", poll_frame, 16971u},
  };
  size_t index;
  t64_machine *machine = make_idle_machine();

  for (index = 0u; index < sizeof rows / sizeof rows[0]; index++) {
    run_timing_row(&rows[index]);
  }
  if (machine == NULL) return;
  /* In-flight witnesses (the named difference from the PI model): the WR64B
     source is read at the start tick, so RDRAM changed afterwards does not
     reach the PIF RAM; the RD64B destination is written at the completion
     tick only, so a read before it sees the bytes RDRAM held. */
  {
    uint8_t first[64];
    uint8_t changed[64];
    uint8_t held[64];
    size_t byte;

    for (byte = 0u; byte < 64u; byte++) {
      first[byte] = (uint8_t)(0x10u + byte);
      changed[byte] = (uint8_t)(0xC0u + byte);
      held[byte] = (uint8_t)(0x70u + byte);
    }
    first[63] = 0u;
    changed[63] = 0u;
    put_frame(machine, SRC_DRAM, first);
    T64_CHECK(advance_guest_to(machine, 20000u));
    T64_CHECK_INT(start_dma(machine, SI_WR64B, SRC_DRAM), T64_STATUS_OK);
    put_frame(machine, SRC_DRAM, changed);
    T64_CHECK(advance_guest_to(machine, 20000u + 2500u));
    T64_CHECK(memcmp(machine->si.pif_ram, first, 64u) == 0);
    T64_CHECK_INT(t64_machine_bus_write(machine, SI_STATUS, 4u, 0u),
                  T64_STATUS_OK);
    (void)memcpy(machine->ram + DST_DRAM, held, 64u);
    T64_CHECK_INT(start_dma(machine, SI_RD64B, DST_DRAM), T64_STATUS_OK);
    /* Right after the start the destination still holds the old bytes. */
    T64_CHECK_INT(bus_read8(machine, DST_DRAM), held[0]);
    T64_CHECK(memcmp(machine->ram + DST_DRAM, held, 64u) == 0);
    (void)memcpy(machine->ram + DST_DRAM, changed, 64u);
    T64_CHECK(advance_guest_to(machine, machine->si.complete_tick - 1u));
    T64_CHECK_INT(bus_read8(machine, DST_DRAM), changed[0]);
    T64_CHECK_INT(bus_read8(machine, DST_DRAM + 63u), changed[63]);
    T64_CHECK(advance_guest_to(machine, machine->si.complete_tick));
    T64_CHECK(memcmp(machine->ram + DST_DRAM, first, 64u) == 0);
    T64_CHECK_INT(t64_machine_bus_write(machine, SI_STATUS, 4u, 0u),
                  T64_STATUS_OK);
  }
  /* WR64B completes at S + 2,500 with the interrupt set at that tick. */
  put_frame(machine, UINT32_C(0x3800), frame_empty);
  T64_CHECK(advance_guest_to(machine, 40000u));
  T64_CHECK_INT(start_dma(machine, SI_WR64B, UINT32_C(0x3800)), T64_STATUS_OK);
  T64_CHECK_INT(machine->si.complete_tick, 40000u + 2500u);
  T64_CHECK(advance_guest_to(machine, 40000u + 2499u));
  T64_CHECK_INT(bus_read32(machine, SI_STATUS), STATUS_BUSY);
  T64_CHECK(advance_guest_to(machine, 40000u + 2500u));
  T64_CHECK_INT(bus_read32(machine, SI_STATUS), STATUS_INTERRUPT);
  t64_machine_destroy(machine);
  check_active_descriptor_immutable();
  check_rd64b_invalidation();
}

/* ------------------------------------------------------------------------ */
/* refusals                                                                  */
/* ------------------------------------------------------------------------ */

/* An accepted baseline in the same setup, then the refusal. The setup is a
 * fresh idle machine with an accepted empty read DMA completed and
 * acknowledged. */
static t64_machine *refusal_machine(void) {
  t64_machine *machine = make_idle_machine();

  if (machine == NULL) return NULL;
  T64_CHECK_INT(start_dma(machine, SI_RD64B, DST_DRAM), T64_STATUS_OK);
  T64_CHECK(advance_guest_to(machine, 4096u));
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_STATUS, 4u, 0u),
                T64_STATUS_OK);
  return machine;
}

static void expect_refused_unchanged(t64_machine *machine, uint32_t reg,
                                     uint32_t value, t64_status status) {
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  const size_t queue_before = machine->queue_count;
  const uint64_t sequence_before = machine->next_sequence;

  digest_of(machine, digest_before);
  T64_CHECK_INT(t64_machine_bus_write(machine, reg, 4u, value), status);
  T64_CHECK_INT(machine->queue_count, queue_before);
  T64_CHECK_INT(machine->next_sequence, sequence_before);
  digest_of(machine, digest_after);
  T64_CHECK(memcmp(digest_before, digest_after, T64_DIGEST_SIZE) == 0);
}

static void case_refusals(void) {
  static const uint32_t bad_pif[] = {0x1FC007C4u, 0x1FC00000u, 0u,
                                     0x1FD007C0u};
  static const uint32_t bad_span[] = {0x003FFFC8u, 0x00400000u};
  static const uint32_t starts[2] = {SI_WR64B, SI_RD64B};
  t64_machine *machine;
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_expected[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  size_t index;
  size_t first;
  size_t second;

  /* A bad PIF address refuses without the sticky error. */
  for (index = 0u; index < sizeof bad_pif / sizeof bad_pif[0]; index++) {
    machine = refusal_machine();
    if (machine == NULL) return;
    T64_CHECK_INT(t64_machine_bus_write(machine, SI_DRAM_ADDR, 4u, DST_DRAM),
                  T64_STATUS_OK);
    expect_refused_unchanged(machine, SI_RD64B, bad_pif[index],
                             T64_STATUS_BUS_DEVICE_REFUSED);
    T64_CHECK(!machine->si.error);
    t64_machine_destroy(machine);
  }
  /* Misaligned DRAM address: refused and the sticky error is set; the digest
     differs from the pre-state only by that flag (compared with a direct-state
     copy that has only the flag set). */
  machine = refusal_machine();
  if (machine == NULL) return;
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_DRAM_ADDR, 4u, 0x3804u),
                T64_STATUS_OK);
  digest_of(machine, digest_before);
  machine->si.error = true;
  digest_of(machine, digest_expected);
  machine->si.error = false;
  T64_CHECK(memcmp(digest_before, digest_expected, T64_DIGEST_SIZE) != 0);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_RD64B, 4u, PIF_ADDRESS),
                T64_STATUS_BUS_DEVICE_REFUSED);
  T64_CHECK(machine->si.error);
  digest_of(machine, digest_after);
  T64_CHECK(memcmp(digest_expected, digest_after, T64_DIGEST_SIZE) == 0);
  t64_machine_destroy(machine);
  /* An out-of-range span refuses without the error; the last in-range span
     (0x003FFFC0) is accepted in the same setup. */
  machine = refusal_machine();
  if (machine == NULL) return;
  T64_CHECK_INT(start_dma(machine, SI_RD64B, UINT32_C(0x003FFFC0)),
                T64_STATUS_OK);
  T64_CHECK(advance_guest_to(machine, machine->si.complete_tick));
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_STATUS, 4u, 0u),
                T64_STATUS_OK);
  t64_machine_destroy(machine);
  for (index = 0u; index < sizeof bad_span / sizeof bad_span[0]; index++) {
    machine = refusal_machine();
    if (machine == NULL) return;
    T64_CHECK_INT(t64_machine_bus_write(machine, SI_DRAM_ADDR, 4u,
                                        bad_span[index]),
                  T64_STATUS_OK);
    expect_refused_unchanged(machine, SI_WR64B, PIF_ADDRESS,
                             T64_STATUS_BUS_DEVICE_REFUSED);
    T64_CHECK(!machine->si.error);
    t64_machine_destroy(machine);
  }
  /* A second start while busy: every write/read combination refuses, sets the
     error, and the original DMA completes unchanged. */
  for (first = 0u; first < 2u; first++) {
    for (second = 0u; second < 2u; second++) {
      uint64_t original_complete;
      uint64_t original_token;
      uint32_t original_direction;

      machine = refusal_machine();
      if (machine == NULL) return;
      put_frame(machine, UINT32_C(0x3800), frame_empty);
      T64_CHECK_INT(start_dma(machine, starts[first], UINT32_C(0x3800)),
                    T64_STATUS_OK);
      original_complete = machine->si.complete_tick;
      original_token = machine->si.active_token;
      original_direction = machine->si.active_direction;
      T64_CHECK_INT(t64_machine_bus_write(machine, starts[second], 4u,
                                          PIF_ADDRESS),
                    T64_STATUS_BUS_DEVICE_REFUSED);
      T64_CHECK(machine->si.error);
      T64_CHECK_INT(machine->si.complete_tick, original_complete);
      T64_CHECK_INT(machine->si.active_token, original_token);
      T64_CHECK_INT(machine->si.active_direction, original_direction);
      T64_CHECK(advance_guest_to(machine, original_complete));
      T64_CHECK(!machine->si.active_valid);
      T64_CHECK_INT(bus_read32(machine, SI_STATUS),
                    STATUS_ERROR | STATUS_INTERRUPT);
      t64_machine_destroy(machine);
    }
  }
  /* Token exhaustion refuses with RESOURCE_EXHAUSTED before busy is visible
     and leaves the digest as it was. */
  machine = refusal_machine();
  if (machine == NULL) return;
  machine->si.token_exhausted = true;
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_DRAM_ADDR, 4u, DST_DRAM),
                T64_STATUS_OK);
  expect_refused_unchanged(machine, SI_RD64B, PIF_ADDRESS,
                           T64_STATUS_RESOURCE_EXHAUSTED);
  T64_CHECK(!machine->si.active_valid);
  T64_CHECK(!machine->si.error);
  t64_machine_destroy(machine);
}

/* ------------------------------------------------------------------------ */
/* reset-replace                                                             */
/* ------------------------------------------------------------------------ */

static void case_reset_replace(void) {
  t64_machine *machine = make_idle_machine();
  t64_machine *fresh = make_idle_machine();
  uint8_t asset[IPL3_ASSET_BYTES];
  uint8_t digest_fresh[T64_DIGEST_SIZE];
  uint8_t digest_replaced[T64_DIGEST_SIZE];
  uint8_t digest_used[T64_DIGEST_SIZE];

  if (machine == NULL || fresh == NULL) {
    t64_machine_destroy(machine);
    t64_machine_destroy(fresh);
    return;
  }
  /* A poll leaves a parsed table, a PIF image, a non-neutral latch, a
     pending interrupt, an in-flight read, the error flag and a pending
     input event. */
  settle_write(machine, poll_frame);
  T64_CHECK_INT(t64_machine_queue_input(machine, 2600u, 0u, 0x8000u, 1, -1),
                T64_STATUS_OK);
  T64_CHECK(advance_guest_to(machine, 3000u));
  T64_CHECK_INT(start_dma(machine, SI_RD64B, DST_DRAM), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_DRAM_ADDR, 4u, 0x3804u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_WR64B, 4u, PIF_ADDRESS),
                T64_STATUS_BUS_DEVICE_REFUSED);
  digest_of(machine, digest_used);
  T64_CHECK(machine->si.active_valid && machine->si.error &&
            machine->si.table.parsed);
  /* The real replacement entry is admission on an admitted machine. */
  T64_CHECK(read_openipl3_asset(asset));
  T64_CHECK_INT(t64_machine_admit_rom(machine, asset, IPL3_ASSET_BYTES),
                T64_STATUS_OK);
  digest_of(machine, digest_replaced);
  digest_of(fresh, digest_fresh);
  T64_CHECK(memcmp(digest_used, digest_replaced, T64_DIGEST_SIZE) != 0);
  /* Boot event of the replaced machine is queued again; the fresh machine
     cancelled its own, so compare after cancelling it too. */
  T64_CHECK(cancel_boot_cpu_event(machine));
  digest_of(machine, digest_replaced);
  T64_CHECK(memcmp(digest_fresh, digest_replaced, T64_DIGEST_SIZE) == 0);
  T64_CHECK(!machine->si.active_valid);
  T64_CHECK(!machine->si.error);
  T64_CHECK(!machine->si.table.parsed);
  T64_CHECK_INT(machine->si.next_token, 1u);
  T64_CHECK_INT(machine->si.latch[0].current_buttons, 0u);
  T64_CHECK(!machine->si.latch[0].next_valid);
  T64_CHECK_INT(bus_read32(machine, SI_STATUS), 0u);
  t64_machine_destroy(machine);
  t64_machine_destroy(fresh);
}

/* ------------------------------------------------------------------------ */
/* atomic-queue-full                                                         */
/* ------------------------------------------------------------------------ */

#define ATOMIC_QUEUE_LIMIT 8u

/* A host-initiated start under a small queue: fill the queue with synthetic
 * events until `room` slots are free (N6: edit the in-memory profile). */
static t64_machine *atomic_machine(size_t room, uint32_t width_bits,
                                   uint64_t next_sequence) {
  t64_machine *machine = make_idle_machine();
  size_t index;

  if (machine == NULL) return NULL;
  machine->profile.event_queue_capacity = ATOMIC_QUEUE_LIMIT;
  for (index = 0u; index < ATOMIC_QUEUE_LIMIT - room; index++) {
    T64_CHECK_INT(t64_test_schedule_synthetic(machine, 1000000u + index,
                                              T64_TEST_EVENT_NOOP, 0u),
                  T64_STATUS_OK);
  }
  machine->profile.sequence_width_bits = width_bits;
  machine->next_sequence = next_sequence;
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_DRAM_ADDR, 4u, DST_DRAM),
                T64_STATUS_OK);
  return machine;
}

/* Accepted: exactly `added` entries queued and the sequence advanced by the
 * same count. Refused: queue, next_sequence and digest bit-identical and no
 * sticky error. */
static void atomic_attempt(const char *label, size_t room, uint32_t width_bits,
                           uint64_t next_sequence, uint64_t tick,
                           uint32_t reg, t64_status expected, size_t added) {
  t64_machine *machine = atomic_machine(room, width_bits, next_sequence);
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  size_t queue_before;
  uint64_t sequence_before;

  if (machine == NULL) return;
  machine->tick = tick;
  queue_before = machine->queue_count;
  sequence_before = machine->next_sequence;
  digest_of(machine, digest_before);
  if (t64_machine_bus_write(machine, reg, 4u, PIF_ADDRESS) != expected) {
    fprintf(stderr, "atomic row '%s' failed\n", label);
  }
  T64_CHECK_INT(machine->queue_count, queue_before + added);
  {
    /* Using the last sequence of the width leaves next_sequence at the limit
       and sets the exhausted flag instead of advancing. */
    const uint64_t limit =
        width_bits == 64u ? UINT64_MAX : (UINT64_C(1) << width_bits) - 1u;

    if (added > 0u && sequence_before + added - 1u >= limit) {
      T64_CHECK_INT(machine->next_sequence, limit);
      T64_CHECK(machine->sequence_exhausted);
    } else {
      T64_CHECK_INT(machine->next_sequence, sequence_before + added);
      T64_CHECK(!machine->sequence_exhausted ||
                (machine->sequence_exhausted && next_sequence > limit));
    }
  }
  if (expected != T64_STATUS_OK) {
    digest_of(machine, digest_after);
    T64_CHECK(memcmp(digest_before, digest_after, T64_DIGEST_SIZE) == 0);
    T64_CHECK(!machine->si.error);
    T64_CHECK(!machine->si.active_valid);
  } else {
    T64_CHECK(machine->si.active_valid);
  }
  t64_machine_destroy(machine);
}

static void case_atomic_queue_full(void) {
  /* RD64B needs two events and WR64B one. */
  atomic_attempt("RD64B room 1", 1u, 64u, 0u, 0u, SI_RD64B,
                 T64_STATUS_QUEUE_FULL, 0u);
  atomic_attempt("RD64B room 2", 2u, 64u, 0u, 0u, SI_RD64B, T64_STATUS_OK, 2u);
  atomic_attempt("WR64B room 0", 0u, 64u, 0u, 0u, SI_WR64B,
                 T64_STATUS_QUEUE_FULL, 0u);
  atomic_attempt("WR64B room 1", 1u, 64u, 0u, 0u, SI_WR64B, T64_STATUS_OK, 1u);
  /* A narrow sequence width: limit 255. RD64B needs next at most 254, WR64B
     at most 255. */
  atomic_attempt("RD64B sequence 255", 8u, 8u, 255u, 0u, SI_RD64B,
                 T64_STATUS_SEQUENCE_EXHAUSTED, 0u);
  atomic_attempt("RD64B sequence 254", 8u, 8u, 254u, 0u, SI_RD64B,
                 T64_STATUS_OK, 2u);
  atomic_attempt("WR64B sequence 256", 8u, 8u, 256u, 0u, SI_WR64B,
                 T64_STATUS_SEQUENCE_EXHAUSTED, 0u);
  atomic_attempt("WR64B sequence 255", 8u, 8u, 255u, 0u, SI_WR64B,
                 T64_STATUS_OK, 1u);
  /* A tick near the maximum: the 4,096-tick read tail and the 2,500-tick
     write completion overflow. */
  atomic_attempt("RD64B tick overflow", 8u, 64u, 0u, UINT64_MAX - 100u,
                 SI_RD64B, T64_STATUS_TICK_OVERFLOW, 0u);
  atomic_attempt("WR64B tick overflow", 8u, 64u, 0u, UINT64_MAX - 100u,
                 SI_WR64B, T64_STATUS_TICK_OVERFLOW, 0u);
  /* The CPU-path store at the same rooms is refused before the store; that
     matrix is the cpu-store-budget case. The shared capacity rule is the
     capacity-rule case, which runs in the same suite. */
}

/* ------------------------------------------------------------------------ */
/* input latch                                                               */
/* ------------------------------------------------------------------------ */

typedef struct {
  int64_t offset; /* ticks from the execute tick E */
  uint16_t buttons;
  int x;
  int y;
} stamp;

/* The three states of the poll oracle. */
#define STATE_A 0x8000u, 1, -1
#define STATE_B 0x4021u, 127, -128
#define STATE_C 0x1802u, -1, 1

typedef enum { PARTITION_WHOLE, PARTITION_STEP, PARTITION_STOPS } partition_mode;

/* One poll through the real SI timeline: a parsed one-controller frame is
 * written, the read starts at tick 3,000, so E = 3,000 + 10,876 = 13,876
 * (independent policy literals) and C = E + 4,096. Inputs are submitted in the
 * given order (before or after the read start), the run is advanced with the
 * given partition, and the four data bytes of the executed image are
 * returned with the final digest. */
#define LATCH_START UINT64_C(3000)
#define LATCH_EXECUTE (LATCH_START + UINT64_C(10876))
#define LATCH_COMPLETE (LATCH_EXECUTE + UINT64_C(4096))

static bool run_latch_scenario(const stamp *stamps, const size_t *order,
                               size_t count, bool submit_after_start,
                               partition_mode mode, uint64_t step,
                               uint8_t data[4],
                               uint8_t digest[T64_DIGEST_SIZE]) {
  t64_machine *machine = make_idle_machine();
  size_t index;

  if (machine == NULL) return false;
  settle_write(machine, frame_state_one);
  T64_CHECK(advance_guest_to(machine, LATCH_START));
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_DRAM_ADDR, 4u, DST_DRAM),
                T64_STATUS_OK);
  if (!submit_after_start) {
    for (index = 0u; index < count; index++) {
      const stamp *row = &stamps[order[index]];

      T64_CHECK_INT(t64_machine_queue_input(
                        machine, (uint64_t)((int64_t)LATCH_EXECUTE + row->offset),
                        0u, row->buttons, row->x, row->y),
                    T64_STATUS_OK);
    }
  }
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_RD64B, 4u, PIF_ADDRESS),
                T64_STATUS_OK);
  T64_CHECK_INT(machine->si.execute_tick, LATCH_EXECUTE);
  if (submit_after_start) {
    for (index = 0u; index < count; index++) {
      const stamp *row = &stamps[order[index]];

      T64_CHECK_INT(t64_machine_queue_input(
                        machine, (uint64_t)((int64_t)LATCH_EXECUTE + row->offset),
                        0u, row->buttons, row->x, row->y),
                    T64_STATUS_OK);
    }
  }
  if (mode == PARTITION_WHOLE) {
    T64_CHECK(advance_guest_to(machine, LATCH_COMPLETE));
  } else if (mode == PARTITION_STEP) {
    while (machine->tick < LATCH_COMPLETE) {
      uint64_t next = machine->tick + step;

      if (next > LATCH_COMPLETE) next = LATCH_COMPLETE;
      T64_CHECK(advance_guest_to(machine, next));
    }
  } else {
    T64_CHECK(advance_guest_to(machine, LATCH_EXECUTE - 1u));
    T64_CHECK(advance_guest_to(machine, LATCH_EXECUTE));
    T64_CHECK(advance_guest_to(machine, LATCH_EXECUTE + 1u));
    T64_CHECK(advance_guest_to(machine, LATCH_COMPLETE));
  }
  (void)memcpy(data, machine->si.pif_ram + 3u, 4u);
  digest_of(machine, digest);
  T64_CHECK(!machine->si.execute_pending);
  t64_machine_destroy(machine);
  return true;
}

static void expect_bytes(const char *label, const uint8_t data[4],
                         uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
  if (data[0] != b0 || data[1] != b1 || data[2] != b2 || data[3] != b3) {
    fprintf(stderr, "latch '%s': %02x %02x %02x %02x\n", label,
            (unsigned)data[0], (unsigned)data[1], (unsigned)data[2],
            (unsigned)data[3]);
  }
  T64_CHECK_INT(data[0], b0);
  T64_CHECK_INT(data[1], b1);
  T64_CHECK_INT(data[2], b2);
  T64_CHECK_INT(data[3], b3);
}

static void case_latch_strictly_after(void) {
  static const size_t natural[3] = {0u, 1u, 2u};
  static const size_t reversed[3] = {1u, 0u, 2u};
  uint8_t data[4];
  uint8_t digest[T64_DIGEST_SIZE];
  t64_machine *machine = make_idle_machine();
  uint16_t buttons = 0u;
  uint8_t x = 0u;
  uint8_t y = 0u;

  /* The literal example: state X stamped at 10 and then state Y stamped at 10
     (the later host call wins), after an earlier state W at 5. A sample at 10
     sees W; a sample at 11 sees Y. */
  if (machine != NULL) {
    T64_CHECK_INT(t64_machine_queue_input(machine, 5u, 0u, 0x0101u, 1, 1),
                  T64_STATUS_OK);
    T64_CHECK_INT(t64_machine_queue_input(machine, 10u, 0u, 0x0202u, 2, 2),
                  T64_STATUS_OK);
    T64_CHECK_INT(t64_machine_queue_input(machine, 10u, 0u, 0x0404u, 4, 4),
                  T64_STATUS_OK);
    T64_CHECK(advance_guest_to(machine, 10u));
    t64_si_latch_sample(machine, 0u, 10u, &buttons, &x, &y);
    T64_CHECK_INT(buttons, 0x0101u);
    T64_CHECK_INT(x, 1u);
    T64_CHECK_INT(y, 1u);
    t64_si_latch_sample(machine, 0u, 11u, &buttons, &x, &y);
    T64_CHECK_INT(buttons, 0x0404u);
    T64_CHECK_INT(x, 4u);
    T64_CHECK_INT(y, 4u);
    t64_machine_destroy(machine);
  }

  /* Inputs stamped E-1, E and E+1: the sample at E sees the E-1 value only
     (A, bytes 80 00 01 FF), in either submission order and whether they are
     submitted before or after the read start. */
  {
    const stamp stamps[3] = {{-1, STATE_A}, {0, STATE_B}, {1, STATE_C}};

    T64_CHECK(run_latch_scenario(stamps, natural, 3u, false, PARTITION_WHOLE,
                                 0u, data, digest));
    expect_bytes("E-1, E, E+1", data, 0x80u, 0x00u, 0x01u, 0xFFu);
    T64_CHECK(run_latch_scenario(stamps, reversed, 3u, true, PARTITION_WHOLE,
                                 0u, data, digest));
    expect_bytes("E-1, E, E+1 reversed after start", data, 0x80u, 0x00u,
                 0x01u, 0xFFu);
  }
  /* E and E+1 only: nothing is visible at E (neutral). */
  {
    const stamp stamps[2] = {{0, STATE_B}, {1, STATE_C}};
    static const size_t two[2] = {0u, 1u};

    T64_CHECK(run_latch_scenario(stamps, two, 2u, false, PARTITION_WHOLE, 0u,
                                 data, digest));
    expect_bytes("E, E+1", data, 0x00u, 0x00u, 0x00u, 0x00u);
  }
  /* E-2 then E-1: the later stamp is the last one below E (B, 40 21 7F 80). */
  {
    const stamp stamps[2] = {{-2, STATE_A}, {-1, STATE_B}};
    static const size_t two[2] = {0u, 1u};

    T64_CHECK(run_latch_scenario(stamps, two, 2u, false, PARTITION_WHOLE, 0u,
                                 data, digest));
    expect_bytes("E-2, E-1", data, 0x40u, 0x21u, 0x7Fu, 0x80u);
  }
  /* A later host call wins at an equal stamp, in both submission orders. */
  {
    const stamp stamps[2] = {{-1, STATE_A}, {-1, STATE_B}};
    static const size_t forward[2] = {0u, 1u};
    static const size_t backward[2] = {1u, 0u};

    T64_CHECK(run_latch_scenario(stamps, forward, 2u, false, PARTITION_WHOLE,
                                 0u, data, digest));
    expect_bytes("equal stamp, B submitted last", data, 0x40u, 0x21u, 0x7Fu,
                 0x80u);
    T64_CHECK(run_latch_scenario(stamps, backward, 2u, false, PARTITION_WHOLE,
                                 0u, data, digest));
    expect_bytes("equal stamp, A submitted last", data, 0x80u, 0x00u, 0x01u,
                 0xFFu);
  }
}

static void case_latch_partition(void) {
  const stamp stamps[3] = {{-1, STATE_A}, {0, STATE_B}, {1, STATE_C}};
  static const size_t orders[6][3] = {{0u, 1u, 2u}, {0u, 2u, 1u},
                                      {1u, 0u, 2u}, {1u, 2u, 0u},
                                      {2u, 0u, 1u}, {2u, 1u, 0u}};
  static const uint64_t steps[] = {1u, 3u, 7u, 4096u};
  uint8_t data[4];
  uint8_t digest[T64_DIGEST_SIZE];
  uint8_t reference[T64_DIGEST_SIZE];
  size_t order;
  size_t index;

  /* Every submission order and every partition samples the same bytes
     (A at E-1). For one submission order every partition also reaches the
     same digest; digests across different submission orders are not
     compared. */
  for (order = 0u; order < 6u; order++) {
    T64_CHECK(run_latch_scenario(stamps, orders[order], 3u, false,
                                 PARTITION_WHOLE, 0u, data, reference));
    expect_bytes("whole", data, 0x80u, 0x00u, 0x01u, 0xFFu);
    for (index = 0u; index < sizeof steps / sizeof steps[0]; index++) {
      T64_CHECK(run_latch_scenario(stamps, orders[order], 3u, false,
                                   PARTITION_STEP, steps[index], data,
                                   digest));
      expect_bytes("step", data, 0x80u, 0x00u, 0x01u, 0xFFu);
      T64_CHECK(memcmp(reference, digest, T64_DIGEST_SIZE) == 0);
    }
    T64_CHECK(run_latch_scenario(stamps, orders[order], 3u, false,
                                 PARTITION_STOPS, 0u, data, digest));
    expect_bytes("stops at E-1, E, E+1", data, 0x80u, 0x00u, 0x01u, 0xFFu);
    T64_CHECK(memcmp(reference, digest, T64_DIGEST_SIZE) == 0);
    T64_CHECK(run_latch_scenario(stamps, orders[order], 3u, true,
                                 PARTITION_STOPS, 0u, data, digest));
    expect_bytes("submitted after the start", data, 0x80u, 0x00u, 0x01u,
                 0xFFu);
  }
}

static void case_latch_port_decode(void) {
  t64_machine *machine = make_idle_machine();
  t64_machine *legacy = NULL;
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  uint32_t port;
  size_t index;
  bool found = false;
  t64_si_state zero_si; /* snapshot taken before the input event runs */

  if (machine == NULL) return;
  /* The family accepts port 0 and refuses ports 1 to 4 with a bit-identical
     digest (the t64.input v1 parser and its port-0-only rule are unchanged). */
  T64_CHECK_INT(t64_machine_queue_input(machine, 50u, 0u, 0x1234u, -1, 1),
                T64_STATUS_OK);
  for (port = 1u; port <= 4u; port++) {
    digest_of(machine, digest_before);
    T64_CHECK_INT(t64_machine_queue_input(machine, 60u, port, 0x1234u, 0, 0),
                  T64_STATUS_INVALID_ARGUMENT);
    digest_of(machine, digest_after);
    T64_CHECK(memcmp(digest_before, digest_after, T64_DIGEST_SIZE) == 0);
  }
  /* The family value for port 0 equals the legacy packing 0x01ff1234. */
  for (index = 0u; index < machine->queue_count; index++) {
    const t64_event *event =
        &machine->events[(machine->queue_head + index) % T64_EVENT_QUEUE_CAPACITY];

    if (event->device == T64_TRACE_DEVICE_INPUT) {
      T64_CHECK_INT(event->value, UINT64_C(0x01ff1234));
      found = true;
    }
  }
  T64_CHECK(found);
  /* An event injected through the scheduler with port bits 2 updates only
     the port 2 latch. */
  {
    t64_event_reservation reservation;

    T64_CHECK_INT(t64_schedule_reserve(
                      machine, 70u, T64_TRACE_DEVICE_INPUT,
                      T64_TRACE_KIND_INPUT_SUBMITTED,
                      UINT64_C(0x01ff1234) | (UINT64_C(2) << 32), 0u,
                      t64_input_event_handler, &reservation),
                  T64_STATUS_OK);
    T64_CHECK_INT(t64_schedule_commit(machine, &reservation), T64_STATUS_OK);
  }
  T64_CHECK(advance_guest_to(machine, 80u));
  T64_CHECK_INT(machine->si.latch[2].next_buttons, 0x1234u);
  T64_CHECK_INT(machine->si.latch[2].next_x, 0xFFu);
  T64_CHECK_INT(machine->si.latch[2].next_y, 0x01u);
  T64_CHECK(machine->si.latch[2].next_valid);
  T64_CHECK_INT(machine->si.latch[2].next_visible_from, 71u);
  T64_CHECK_INT(machine->si.latch[1].next_buttons, 0u);
  T64_CHECK(!machine->si.latch[1].next_valid);
  T64_CHECK(!machine->si.latch[3].next_valid);
  /* Port 0 got the host call at tick 50 (promoted by the later event). */
  T64_CHECK_INT(machine->si.latch[0].next_buttons, 0x1234u);
  t64_machine_destroy(machine);

  /* A legacy profile still refuses port 1, keeps the value 0x01ff1234 and an
     observer-only handler that changes no state. */
  legacy = create_admitted(AI_STEM_ID);
  if (legacy == NULL) return;
  T64_CHECK_INT(t64_machine_queue_input(legacy, 60u, 1u, 0x1234u, 0, 0),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK_INT(t64_machine_queue_input(legacy, 50u, 0u, 0x1234u, -1, 1),
                T64_STATUS_OK);
  found = false;
  for (index = 0u; index < legacy->queue_count; index++) {
    const t64_event *event =
        &legacy->events[(legacy->queue_head + index) % T64_EVENT_QUEUE_CAPACITY];

    if (event->device == T64_TRACE_DEVICE_INPUT) {
      T64_CHECK_INT(event->value, UINT64_C(0x01ff1234));
      found = true;
    }
  }
  T64_CHECK(found);
  zero_si = legacy->si; /* the SI state of a legacy profile is never used */
  T64_CHECK(cancel_boot_cpu_event(legacy));
  T64_CHECK(advance_guest_to(legacy, 60u));
  T64_CHECK(memcmp(&legacy->si, &zero_si, sizeof zero_si) == 0);
  /* Calling the handler directly on the legacy profile also changes nothing. */
  {
    t64_event event;

    (void)memset(&event, 0, sizeof event);
    event.device = T64_TRACE_DEVICE_INPUT;
    event.kind = T64_TRACE_KIND_INPUT_SUBMITTED;
    event.value = UINT64_C(0x01ff1234);
    event.tick = 60u;
    digest_of(legacy, digest_before);
    T64_CHECK_INT(t64_input_event_handler(legacy, &event), T64_STATUS_OK);
    digest_of(legacy, digest_after);
    T64_CHECK(memcmp(digest_before, digest_after, T64_DIGEST_SIZE) == 0);
  }
  t64_machine_destroy(legacy);
}

/* ------------------------------------------------------------------------ */
/* Dispatch                                                                  */
/* ------------------------------------------------------------------------ */

typedef struct {
  const char *name;
  void (*run)(void);
  const char *suite;        /* --suite group, or NULL */
  const char *finish_name;  /* name printed by T64_TEST_FINISH for --case */
} test_case;

static const test_case cases[] = {
    {"cpu-store-budget", case_cpu_store_budget, NULL,
     "unit.core.cpu.si-store-budget"},
    {"capacity-rule", case_capacity_rule, "atomic",
     "unit.core.si.atomic-queue-full"},
    {"atomic-queue-full", case_atomic_queue_full, "atomic",
     "unit.core.si.atomic-queue-full"},
    {"tracer-poll", case_tracer_poll, "device", "unit.core.si.device"},
    {"registers", case_registers, "device", "unit.core.si.device"},
    {"status", case_status, "device", "unit.core.si.device"},
    {"pif-command", case_pif_command, "device", "unit.core.si.device"},
    {"dma-timing", case_dma_timing, "device", "unit.core.si.device"},
    {"refusals", case_refusals, "device", "unit.core.si.device"},
    {"reset-replace", case_reset_replace, "device", "unit.core.si.device"},
    {"digest-section", case_digest_section, "device",
     "unit.core.si.device"},
    {"latch-strictly-after", case_latch_strictly_after, "latch",
     "unit.core.input.latch-partition"},
    {"latch-partition", case_latch_partition, "latch",
     "unit.core.input.latch-partition"},
    {"latch-port-decode", case_latch_port_decode, "latch",
     "unit.core.input.latch-partition"},
};

static const char *suite_finish_name(const char *suite) {
  if (strcmp(suite, "atomic") == 0) return "unit.core.si.atomic-queue-full";
  if (strcmp(suite, "device") == 0) return "unit.core.si.device";
  if (strcmp(suite, "latch") == 0) return "unit.core.input.latch-partition";
  return NULL;
}

int main(int argc, char **argv) {
  size_t index;
  const size_t count = sizeof cases / sizeof cases[0];

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (index = 0u; index < count; index++) puts(cases[index].name);
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--suite") == 0) {
    const char *finish = suite_finish_name(argv[2]);

    if (finish == NULL) {
      fprintf(stderr, "unknown SI suite: %s\n", argv[2]);
      return 2;
    }
    for (index = 0u; index < count; index++) {
      if (cases[index].suite != NULL &&
          strcmp(cases[index].suite, argv[2]) == 0) {
        printf("case %s\n", cases[index].name);
        cases[index].run();
      }
    }
    return T64_TEST_FINISH(finish);
  }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    for (index = 0u; index < count; index++) {
      if (strcmp(cases[index].name, argv[2]) == 0) {
        cases[index].run();
        return T64_TEST_FINISH(cases[index].finish_name);
      }
    }
    fprintf(stderr, "unknown SI case: %s\n", argv[2]);
    return 2;
  }
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--list-cases|--suite NAME|--case NAME]\n",
            argv[0]);
    return 2;
  }
  for (index = 0u; index < count; index++) {
    printf("case %s\n", cases[index].name);
    cases[index].run();
  }
  return T64_TEST_FINISH("unit.core.si");
}
