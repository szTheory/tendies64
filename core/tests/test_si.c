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

static const t64_host_allocator allocator = {test_allocate, test_release,
                                             NULL};

static t64_machine *create_admitted(const char *profile_id) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t asset[IPL3_ASSET_BYTES];

  T64_CHECK_INT(t64_profile_lookup(profile_id, &profile), T64_STATUS_OK);
  if (profile == NULL) return NULL;
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
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
static void budget_room_pair(const char *label, const char *profile_id,
                             uint32_t store_opcode, size_t refuse_room,
                             size_t accept_room) {
  t64_machine *machine;
  store_result result;

  machine = make_store_machine(profile_id, store_opcode, accept_room, 64u, 0u);
  if (machine == NULL) return;
  result = run_store(machine, store_opcode);
  /* Accepted baseline in the same setup: the capacity rule did not refuse. */
  if (refused_for_capacity(result.status)) {
    fprintf(stderr, "budget row '%s': accept room %zu refused with %u\n",
            label, accept_room, (unsigned)result.status);
  }
  T64_CHECK(!refused_for_capacity(result.status));
  t64_machine_destroy(machine);

  machine = make_store_machine(profile_id, store_opcode, refuse_room, 64u, 0u);
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
  t64_stop_request request = {T64_STOP_GUEST_TICK, tick};
  t64_stop_result result = {0u, 0u};
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
    {"capacity-rule", case_capacity_rule, "atomic",
     "unit.core.si.atomic-queue-full"},
    {"cpu-store-budget", case_cpu_store_budget, NULL,
     "unit.core.cpu.si-store-budget"},
    {"tracer-poll", case_tracer_poll, "device", "unit.core.si.device"},
    {"digest-section", case_digest_section, "device",
     "unit.core.si.device"},
};

static const char *suite_finish_name(const char *suite) {
  if (strcmp(suite, "atomic") == 0) return "unit.core.si.atomic-queue-full";
  if (strcmp(suite, "device") == 0) return "unit.core.si.device";
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
