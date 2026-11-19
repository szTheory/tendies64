/*
 * Phase 9 scheduler ledger and same-tick order characterization (plan 09-01).
 *
 * ledger: the closed (device, kind) table of events the scheduler dispatches.
 * The expected rows are literals; the observed set comes from authored
 * scenarios that make every reservation site run. The SI rows (device 10) are
 * declared reserved and must not appear until a reservation site exists.
 *
 * characterize-fifo: two events at one guest tick dispatch in insertion
 * order, for both insertion orders. Every ledger pair is checked with
 * label-only events through the real scheduler; the pairs an existing builder
 * can place at one tick are also checked with the real device events.
 *
 * The evidence is a regression pin and self-comparison: a consistently wrong
 * tie would pass (D-36). No hardware evidence is claimed.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_internal.h"
#include "t64_test.h"

#define IPL3_ASSET_BYTES 4096u
#define FAMILY_STEM "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1"
#define SKELETON_PROFILE "ntsc-cart-4m-skeleton"
#define AUTHORED_ROM_BYTES 4096u
#define SYNTHETIC_DEVICE UINT32_C(0xffff0001)

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
  return read_error == 0 && close_error == 0 && length == IPL3_ASSET_BYTES &&
         extra == EOF;
}

/* Authored 4 KiB image the skeleton profile accepts. */
static void fill_authored_rom(uint8_t image[AUTHORED_ROM_BYTES]) {
  size_t i;

  for (i = 0u; i < AUTHORED_ROM_BYTES; i++) {
    image[i] = (uint8_t)((i * 13u + 7u) & 0xffu);
  }
  image[0] = 0x80u;
  image[1] = 0x37u;
  image[2] = 0x12u;
  image[3] = 0x40u;
  image[0x3eu] = (uint8_t)'E';
}

/* The family profile runs the CPU and the RSP-functional devices. The
 * skeleton profile keeps the legacy SP DMA with its own completion event
 * (device 4, kind 1) that the RSP-functional profiles replace with RSP issue
 * events (device 4, kind 2). */
static t64_machine *create_admitted_profile(const char *profile_id) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t asset[IPL3_ASSET_BYTES];
  uint8_t image[AUTHORED_ROM_BYTES];

  T64_CHECK_INT(t64_profile_lookup(profile_id, &profile), T64_STATUS_OK);
  if (profile == NULL) return NULL;
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  if (machine == NULL) return NULL;
  if (strcmp(profile_id, SKELETON_PROFILE) == 0) {
    fill_authored_rom(image);
    T64_CHECK_INT(t64_machine_admit_rom(machine, image, sizeof image),
                  T64_STATUS_OK);
  } else {
    T64_CHECK(read_openipl3_asset(asset));
    T64_CHECK_INT(t64_machine_admit_rom(machine, asset, sizeof asset),
                  T64_STATUS_OK);
  }
  return machine;
}

static t64_machine *create_admitted(void) {
  return create_admitted_profile(FAMILY_STEM);
}

/* ---- the closed ledger ---------------------------------------------------- */

typedef struct {
  uint32_t device;
  uint32_t kind;
  bool reserved;  /* declared, no reservation site yet */
} ledger_row;

#define LEDGER_ROWS 15u

/* Literal table (D-07). Reservation sites: CPU step, PI completion, SP
 * completion and RSP issue, timed input, RDP work, VI field, AI sample, the
 * synthetic test device; SI execute and complete are reserved. */
static const ledger_row ledger_table[LEDGER_ROWS] = {
    {5u, 1u, false},                  /* CPU step */
    {3u, 1u, false},                  /* PI complete */
    {4u, 1u, false},                  /* SP DMA complete */
    {4u, 2u, false},                  /* RSP issue */
    {6u, 1u, false},                  /* input submitted */
    {7u, 1u, false},                  /* RDP work */
    {8u, 1u, false},                  /* VI field */
    {9u, 1u, false},                  /* AI sample */
    {SYNTHETIC_DEVICE, 1u, false},    /* synthetic noop */
    {SYNTHETIC_DEVICE, 2u, false},    /* synthetic instruction */
    {SYNTHETIC_DEVICE, 3u, false},    /* synthetic frame */
    {SYNTHETIC_DEVICE, 4u, false},    /* synthetic halt */
    {SYNTHETIC_DEVICE, 5u, false},    /* synthetic repeat */
    {10u, 1u, true},                  /* SI execute (reserved) */
    {10u, 2u, true},                  /* SI complete (reserved) */
};

typedef enum { LEDGER_EQUAL = 0, LEDGER_TABLE_MISMATCH = 1 } ledger_result;

#define OBSERVED_CAPACITY 64u

typedef struct {
  uint32_t device[OBSERVED_CAPACITY];
  uint32_t kind[OBSERVED_CAPACITY];
  size_t count;
} observed_set;

static void observe_pair(observed_set *set, uint32_t device, uint32_t kind) {
  size_t i;

  for (i = 0u; i < set->count; i++) {
    if (set->device[i] == device && set->kind[i] == kind) return;
  }
  if (set->count < OBSERVED_CAPACITY) {
    set->device[set->count] = device;
    set->kind[set->count] = kind;
    set->count++;
  } else {
    T64_CHECK(false);
  }
}

static void observe_queue(const t64_machine *machine, observed_set *set) {
  size_t i;

  for (i = 0u; i < machine->queue_count; i++) {
    const t64_event *event =
        &machine->events[(machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY];

    observe_pair(set, event->device, event->kind);
  }
}

/* Step one guest tick at a time so every queued pair is sampled. */
static void observe_run(t64_machine *machine, observed_set *set,
                        uint64_t ticks) {
  uint64_t i;

  for (i = 0u; i < ticks; i++) {
    t64_stop_request request = {T64_STOP_GUEST_TICK, machine->tick + 1u};
    t64_stop_result stop = {0u, 0u};

    observe_queue(machine, set);
    if (t64_machine_advance(machine, &request, &stop) != T64_STATUS_OK) {
      T64_CHECK(false);
      return;
    }
  }
  observe_queue(machine, set);
}

static bool in_observed(const observed_set *set, uint32_t device,
                        uint32_t kind) {
  size_t i;

  for (i = 0u; i < set->count; i++) {
    if (set->device[i] == device && set->kind[i] == kind) return true;
  }
  return false;
}

/*
 * The observed set must equal the non-reserved rows of the table, and no
 * reserved row may be observed.
 */
static ledger_result compare_ledger(const ledger_row *table, size_t rows,
                                    const observed_set *set) {
  size_t expected = 0u;
  size_t i;

  for (i = 0u; i < rows; i++) {
    const bool seen = in_observed(set, table[i].device, table[i].kind);

    if (table[i].reserved) {
      if (seen) return LEDGER_TABLE_MISMATCH;
    } else {
      if (!seen) return LEDGER_TABLE_MISMATCH;
      expected++;
    }
  }
  return expected == set->count ? LEDGER_EQUAL : LEDGER_TABLE_MISMATCH;
}

static void install_spin_loop(t64_machine *machine) {
  machine->ram[UINT32_C(0x1000)] = 0x08u;
  machine->ram[UINT32_C(0x1001)] = 0x00u;
  machine->ram[UINT32_C(0x1002)] = 0x04u;
  machine->ram[UINT32_C(0x1003)] = 0x00u;
  (void)memset(machine->ram + UINT32_C(0x1004), 0, 4u);
  machine->cpu.pc = UINT32_C(0x80001000);
  machine->cpu.pc_full = t64_cpu_canonical_address(machine->cpu.pc);
}

static void write32(t64_machine *machine, uint32_t address, uint32_t value) {
  T64_CHECK_INT(t64_machine_bus_write(machine, address, 4u, value),
                T64_STATUS_OK);
}

#define PI_BASE UINT32_C(0x04600000)
#define SP_BASE UINT32_C(0x04040000)
#define DP_START_REGISTER UINT32_C(0x04100000)
#define DP_END_REGISTER UINT32_C(0x04100004)
#define VI_BASE UINT32_C(0x04400000)
#define AI_BASE UINT32_C(0x04500000)

/* ---- real-device arms (the same register programming the device tests use) */

static void arm_pi(t64_machine *machine) {
  write32(machine, PI_BASE, UINT32_C(0x200));
  write32(machine, PI_BASE + 4u, UINT32_C(0x10000000));
  write32(machine, PI_BASE + 12u, UINT32_C(23));
}

static void arm_sp_dma(t64_machine *machine) {
  write32(machine, SP_BASE, 0u);
  write32(machine, SP_BASE + 4u, UINT32_C(0x200));
  write32(machine, SP_BASE + 8u, 0u);
}

static void arm_rsp(t64_machine *machine) {
  write32(machine, UINT32_C(0x04001000), UINT32_C(0x24020009));
  T64_CHECK_INT(t64_rsp_start(machine), T64_STATUS_OK);
}

static void arm_rdp(t64_machine *machine) {
  size_t i;

  for (i = 0u; i < 4u; i++) {
    (void)memset(machine->ram + UINT32_C(0x100) + i * 8u, 0, 8u);
    machine->ram[UINT32_C(0x100) + i * 8u] = 0xe9u;
  }
  write32(machine, DP_START_REGISTER, UINT32_C(0x100));
  write32(machine, DP_END_REGISTER, UINT32_C(0x120));
}

static void arm_vi(t64_machine *machine) {
  size_t pixel;

  for (pixel = 0u; pixel < 12u; pixel++) {
    T64_CHECK_INT(t64_machine_bus_write(machine,
                                        UINT32_C(0x3000) + (uint32_t)(pixel * 2u),
                                        2u, UINT32_C(0xf801)),
                  T64_STATUS_OK);
  }
  write32(machine, VI_BASE + 0x04u, UINT32_C(0x3000));
  write32(machine, VI_BASE + 0x08u, UINT32_C(4));
  write32(machine, VI_BASE + 0x0cu, UINT32_C(2));
  write32(machine, VI_BASE + 0x14u, UINT32_C(0));
  write32(machine, VI_BASE + 0x18u, UINT32_C(8));
  write32(machine, VI_BASE + 0x1cu, UINT32_C(2207));
  write32(machine, VI_BASE + 0x24u, UINT32_C(4));
  write32(machine, VI_BASE + 0x28u, UINT32_C(0x00020008));
  write32(machine, VI_BASE + 0x30u, UINT32_C(0x00000400));
  write32(machine, VI_BASE + 0x34u, UINT32_C(0x00000400));
  write32(machine, VI_BASE + 0x00u, UINT32_C(2));
}

static void arm_ai(t64_machine *machine) {
  write32(machine, AI_BASE + 0x08u, 1u);
  write32(machine, AI_BASE + 0x10u, 1103u);
  write32(machine, AI_BASE + 0x14u, 15u);
  write32(machine, AI_BASE + 0x00u, UINT32_C(0x100));
  write32(machine, AI_BASE + 0x04u, 16u);
}

static void arm_input(t64_machine *machine) {
  T64_CHECK_INT(t64_machine_queue_input(machine, machine->tick + 7u, 0u,
                                        UINT16_C(0x8001), 5, -5),
                T64_STATUS_OK);
}

static void cancel_boot_cpu_event(t64_machine *machine) {
  uint64_t cancelled_tick = UINT64_MAX;

  T64_CHECK_INT(t64_schedule_cancel(machine, T64_DEVICE_CPU,
                                    T64_CPU_EVENT_STEP, 0u, &cancelled_tick),
                T64_STATUS_OK);
  T64_CHECK_INT(cancelled_tick, 1u);
}

static void schedule_cpu_step_at(t64_machine *machine, uint64_t tick) {
  t64_event_reservation reservation;
  t64_status status = t64_schedule_reserve(machine, tick, T64_DEVICE_CPU,
                                           T64_CPU_EVENT_STEP, 0u, 0u,
                                           t64_cpu_step, &reservation);

  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status == T64_STATUS_OK) {
    T64_CHECK_INT(t64_schedule_commit(machine, &reservation), T64_STATUS_OK);
  }
}

/* ---- case: ledger --------------------------------------------------------- */

typedef void (*arm_fn)(t64_machine *machine);

static void observe_scenario(observed_set *set, arm_fn arm, uint64_t ticks) {
  t64_machine *machine = create_admitted();

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  install_spin_loop(machine);
  if (arm != NULL) arm(machine);
  observe_run(machine, set, ticks);
  t64_machine_destroy(machine);
}

/* Skeleton profile: the legacy SP DMA completion event (device 4, kind 1). */
static void observe_legacy_sp_dma(observed_set *set) {
  t64_machine *machine = create_admitted_profile(SKELETON_PROFILE);

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  arm_sp_dma(machine);
  observe_run(machine, set, 20u);
  t64_machine_destroy(machine);
}

static void observe_synthetic(observed_set *set) {
  t64_machine *machine = create_admitted();
  uint32_t kind;

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  install_spin_loop(machine);
  for (kind = T64_TEST_EVENT_NOOP; kind <= T64_TEST_EVENT_REPEAT_INSTRUCTION;
       kind++) {
    /* The repeat event re-reserves itself forever; it is only observed. */
    T64_CHECK_INT(t64_test_schedule_synthetic(machine, 20u + kind, kind, 1u),
                  T64_STATUS_OK);
  }
  observe_queue(machine, set);
  /* Run until the halt event stops the guest at its tick. */
  {
    t64_stop_request request = {T64_STOP_GUEST_TICK, 30u};
    t64_stop_result stop = {0u, 0u};

    T64_CHECK_INT(t64_machine_advance(machine, &request, &stop),
                  T64_STATUS_OK);
  }
  observe_queue(machine, set);
  t64_machine_destroy(machine);
}

static void case_ledger(void) {
  observed_set set;
  ledger_row transposed[LEDGER_ROWS];
  size_t i;

  /* The literals match the production constants they stand for. */
  T64_CHECK_INT(T64_DEVICE_CPU, 5u);
  T64_CHECK_INT(T64_CPU_EVENT_STEP, 1u);
  T64_CHECK_INT(T64_DEVICE_PI, 3u);
  T64_CHECK_INT(T64_PI_EVENT_COMPLETE, 1u);
  T64_CHECK_INT(T64_DEVICE_SP, 4u);
  T64_CHECK_INT(T64_SP_EVENT_COMPLETE, 1u);
  T64_CHECK_INT(T64_SP_EVENT_RSP_ISSUE, 2u);
  T64_CHECK_INT(T64_TRACE_DEVICE_INPUT, 6u);
  T64_CHECK_INT(T64_TRACE_KIND_INPUT_SUBMITTED, 1u);
  T64_CHECK_INT(T64_RDP_DEVICE, 7u);
  T64_CHECK_INT(T64_RDP_EVENT_WORK, 1u);
  T64_CHECK_INT(T64_VI_DEVICE, 8u);
  T64_CHECK_INT(T64_VI_EVENT_FIELD, 1u);
  T64_CHECK_INT(T64_AI_DEVICE, 9u);
  T64_CHECK_INT(T64_AI_EVENT_SAMPLE, 1u);
  T64_CHECK_INT(T64_DEVICE_SYNTHETIC_TEST, SYNTHETIC_DEVICE);
  T64_CHECK_INT(T64_SI_DEVICE, 10u);
  T64_CHECK_INT(T64_SI_EVENT_EXECUTE, 1u);
  T64_CHECK_INT(T64_SI_EVENT_COMPLETE, 2u);

  (void)memset(&set, 0, sizeof set);
  observe_scenario(&set, NULL, 6u);        /* boot CPU step */
  observe_scenario(&set, arm_pi, 20u);     /* PI DMA completion */
  observe_legacy_sp_dma(&set);             /* SP DMA completion (legacy) */
  observe_scenario(&set, arm_rsp, 6u);     /* RSP issue */
  observe_scenario(&set, arm_rdp, 6u);     /* RDP work */
  observe_scenario(&set, arm_vi, 6u);      /* VI field */
  observe_scenario(&set, arm_ai, 6u);      /* AI sample */
  observe_scenario(&set, arm_input, 12u);  /* queued input */
  observe_synthetic(&set);                 /* synthetic kinds 1 to 5 */

  /* Each non-reserved row is observed at least once, no unlisted pair
   * appears, and the reserved SI rows are not observed. */
  for (i = 0u; i < LEDGER_ROWS; i++) {
    if (ledger_table[i].reserved) {
      T64_CHECK(!in_observed(&set, ledger_table[i].device,
                             ledger_table[i].kind));
    } else {
      T64_CHECK(in_observed(&set, ledger_table[i].device,
                            ledger_table[i].kind));
    }
  }
  if (compare_ledger(ledger_table, LEDGER_ROWS, &set) != LEDGER_EQUAL) {
    size_t k;

    for (k = 0u; k < set.count; k++) {
      fprintf(stderr, "observed pair %u/%u\n", (unsigned)set.device[k],
              (unsigned)set.kind[k]);
    }
  }
  T64_CHECK_INT(compare_ledger(ledger_table, LEDGER_ROWS, &set), LEDGER_EQUAL);
  T64_CHECK_INT(set.count, 13u);

  /* Transposed-table control: with the kinds of the PI row and the RSP issue
   * row swapped the same observation must be reported as a table mismatch. */
  (void)memcpy(transposed, ledger_table, sizeof transposed);
  T64_CHECK_INT(compare_ledger(transposed, LEDGER_ROWS, &set), LEDGER_EQUAL);
  transposed[1].kind = ledger_table[3].kind;
  transposed[3].kind = ledger_table[1].kind;
  T64_CHECK_INT(compare_ledger(transposed, LEDGER_ROWS, &set),
                LEDGER_TABLE_MISMATCH);
  /* A reserved row observed is also a mismatch. */
  (void)memcpy(transposed, ledger_table, sizeof transposed);
  transposed[13].reserved = false;
  T64_CHECK_INT(compare_ledger(transposed, LEDGER_ROWS, &set),
                LEDGER_TABLE_MISMATCH);
}

/* ---- case: characterize-fifo ---------------------------------------------- */

#define ORDER_LOG_CAPACITY 8u
#define NON_RESERVED_ROWS 13u

static uint32_t order_log_device[ORDER_LOG_CAPACITY];
static uint32_t order_log_kind[ORDER_LOG_CAPACITY];
static size_t order_log_count;

/* Label-only handler: records which (device, kind) dispatched, in order. */
static t64_status recording_handler(t64_machine *machine,
                                    const t64_event *event) {
  (void)machine;
  if (order_log_count < ORDER_LOG_CAPACITY) {
    order_log_device[order_log_count] = event->device;
    order_log_kind[order_log_count] = event->kind;
    order_log_count++;
  }
  return T64_STATUS_OK;
}

static void reserve_labeled(t64_machine *machine, uint64_t tick,
                            const ledger_row *row) {
  t64_event_reservation reservation;
  t64_status status = t64_schedule_reserve(machine, tick, row->device,
                                           row->kind, 0u, 0u,
                                           recording_handler, &reservation);

  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status == T64_STATUS_OK) {
    T64_CHECK_INT(t64_schedule_commit(machine, &reservation), T64_STATUS_OK);
  }
}

/* Reached: both events are queued at tick `tick`, first before second. */
static bool queued_in_order(const t64_machine *machine, uint64_t tick,
                            const ledger_row *first, const ledger_row *second) {
  uint64_t first_sequence = UINT64_MAX;
  uint64_t second_sequence = UINT64_MAX;
  size_t i;

  for (i = 0u; i < machine->queue_count; i++) {
    const t64_event *event =
        &machine->events[(machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY];

    if (event->tick != tick) continue;
    if (event->device == first->device && event->kind == first->kind) {
      first_sequence = event->sequence;
    }
    if (event->device == second->device && event->kind == second->kind) {
      second_sequence = event->sequence;
    }
  }
  return first_sequence != UINT64_MAX && second_sequence != UINT64_MAX &&
         first_sequence < second_sequence;
}

/* Insert `first` then `second` at one tick; dispatch must be first, second. */
static void check_label_pair(size_t first_index, size_t second_index) {
  t64_machine *machine = create_admitted();
  const ledger_row *first = &ledger_table[first_index];
  const ledger_row *second = &ledger_table[second_index];
  t64_stop_request request;
  t64_stop_result stop = {0u, 0u};
  uint64_t tick;

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  cancel_boot_cpu_event(machine);
  T64_CHECK_INT(machine->queue_count, 0u);
  tick = machine->tick + 10u;
  order_log_count = 0u;
  reserve_labeled(machine, tick, first);
  reserve_labeled(machine, tick, second);
  T64_CHECK_INT(machine->queue_count, 2u);
  T64_CHECK(queued_in_order(machine, tick, first, second));
  request = (t64_stop_request){T64_STOP_GUEST_TICK, tick};
  T64_CHECK_INT(t64_machine_advance(machine, &request, &stop), T64_STATUS_OK);
  T64_CHECK_INT(order_log_count, 2u);
  if (order_log_count == 2u) {
    /* Literal expectation: dispatch order equals insertion order. */
    T64_CHECK_INT(order_log_device[0], first->device);
    T64_CHECK_INT(order_log_kind[0], first->kind);
    T64_CHECK_INT(order_log_device[1], second->device);
    T64_CHECK_INT(order_log_kind[1], second->kind);
  }
  t64_machine_destroy(machine);
}

/* Index of the first trace record at `tick` carrying the pair, or SIZE_MAX. */
static size_t first_record(const t64_trace_record *records, size_t count,
                           uint64_t tick, uint32_t device, uint32_t kind) {
  size_t i;

  for (i = 0u; i < count; i++) {
    if (records[i].tick == tick && records[i].device == device &&
        records[i].kind == kind) {
      return i;
    }
  }
  return SIZE_MAX;
}

static bool queued_pair_tick(const t64_machine *machine, uint32_t device,
                             uint32_t kind, uint64_t *tick_out) {
  size_t i;

  for (i = 0u; i < machine->queue_count; i++) {
    const t64_event *event =
        &machine->events[(machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY];

    if (event->device == device && event->kind == kind) {
      *tick_out = event->tick;
      return true;
    }
  }
  return false;
}

/* Dispatch to `tick` with the trace on and return the dispatch order of the
 * two pairs from the first trace record each leaves at that tick. */
static bool dispatch_order(t64_machine *machine, uint64_t tick,
                           const ledger_row *first, const ledger_row *second,
                           size_t *first_index, size_t *second_index) {
  t64_trace_record records[T64_TRACE_RECORD_CAPACITY];
  size_t count = 0u;
  uint64_t dropped = 0u;
  bool saturated = false;
  t64_stop_request request = {T64_STOP_GUEST_TICK, tick};
  t64_stop_result stop = {0u, 0u};

  T64_CHECK_INT(t64_machine_trace_set_enabled(machine, true), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_advance(machine, &request, &stop), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_trace_drain(machine, records,
                                        T64_TRACE_RECORD_CAPACITY, &count,
                                        &dropped, &saturated),
                T64_STATUS_OK);
  T64_CHECK_INT(dropped, 0u);
  *first_index = first_record(records, count, tick, first->device, first->kind);
  *second_index =
      first_record(records, count, tick, second->device, second->kind);
  return *first_index != SIZE_MAX && *second_index != SIZE_MAX;
}

/*
 * Real-device pair: `arm_a` then `arm_b` host programming leaves one event
 * of each device at the same tick; the dispatch order must be the insertion
 * order. The pair's expected tick is learned from a dry run.
 */
static void check_real_pair(const char *label, const char *profile_id,
                            arm_fn arm_a, arm_fn arm_b, size_t index_a,
                            size_t index_b) {
  const ledger_row *row_a = &ledger_table[index_a];
  const ledger_row *row_b = &ledger_table[index_b];
  int order;

  for (order = 0; order < 2; order++) {
    t64_machine *machine = create_admitted_profile(profile_id);
    arm_fn first_arm = order == 0 ? arm_a : arm_b;
    arm_fn second_arm = order == 0 ? arm_b : arm_a;
    const ledger_row *first = order == 0 ? row_a : row_b;
    const ledger_row *second = order == 0 ? row_b : row_a;
    uint64_t first_tick = 0u;
    uint64_t second_tick = 0u;
    size_t first_index = SIZE_MAX;
    size_t second_index = SIZE_MAX;

    T64_CHECK(machine != NULL);
    if (machine == NULL) continue;
    if (strcmp(profile_id, SKELETON_PROFILE) != 0) {
      cancel_boot_cpu_event(machine);
      install_spin_loop(machine);
    }
    first_arm(machine);
    second_arm(machine);
    /* Reached: both events are queued at one tick, the first inserted first. */
    T64_CHECK(queued_pair_tick(machine, first->device, first->kind,
                               &first_tick));
    T64_CHECK(queued_pair_tick(machine, second->device, second->kind,
                               &second_tick));
    if (first_tick != second_tick) {
      fprintf(stderr, "%s order %d: events not at one tick (%llu, %llu)\n",
              label, order, (unsigned long long)first_tick,
              (unsigned long long)second_tick);
    }
    T64_CHECK_INT(first_tick, second_tick);
    T64_CHECK(queued_in_order(machine, first_tick, first, second));
    if (dispatch_order(machine, first_tick, first, second, &first_index,
                       &second_index)) {
      if (!(first_index < second_index)) {
        fprintf(stderr, "%s order %d: dispatch order differs from insertion\n",
                label, order);
      }
      T64_CHECK(first_index < second_index);
    } else {
      fprintf(stderr, "%s order %d: dispatch records missing\n", label, order);
      T64_CHECK(false);
    }
    t64_machine_destroy(machine);
  }
}

/*
 * CPU against a real device: the device event's tick is learned from a dry
 * run, then a CPU step is reserved at that tick either before the device is
 * armed (CPU first) or after (device first).
 */
static void check_cpu_pair(const char *label, arm_fn arm, size_t device_index) {
  const ledger_row *device_row = &ledger_table[device_index];
  const ledger_row *cpu_row = &ledger_table[0];
  uint64_t tick = 0u;
  int order;

  {
    t64_machine *dry = create_admitted();

    T64_CHECK(dry != NULL);
    if (dry == NULL) return;
    cancel_boot_cpu_event(dry);
    install_spin_loop(dry);
    arm(dry);
    T64_CHECK(queued_pair_tick(dry, device_row->device, device_row->kind,
                               &tick));
    t64_machine_destroy(dry);
  }
  for (order = 0; order < 2; order++) {
    t64_machine *machine = create_admitted();
    const ledger_row *first = order == 0 ? cpu_row : device_row;
    const ledger_row *second = order == 0 ? device_row : cpu_row;
    size_t first_index = SIZE_MAX;
    size_t second_index = SIZE_MAX;
    uint64_t device_tick = 0u;

    T64_CHECK(machine != NULL);
    if (machine == NULL) continue;
    cancel_boot_cpu_event(machine);
    install_spin_loop(machine);
    if (order == 0) schedule_cpu_step_at(machine, tick);
    arm(machine);
    if (order == 1) schedule_cpu_step_at(machine, tick);
    /* Reached: the device event and the CPU step share one tick, in order. */
    T64_CHECK(queued_pair_tick(machine, device_row->device, device_row->kind,
                               &device_tick));
    T64_CHECK_INT(device_tick, tick);
    T64_CHECK(queued_in_order(machine, tick, first, second));
    if (dispatch_order(machine, tick, first, second, &first_index,
                       &second_index)) {
      if (!(first_index < second_index)) {
        fprintf(stderr, "%s cpu %s: dispatch order differs from insertion\n",
                label, order == 0 ? "first" : "second");
      }
      T64_CHECK(first_index < second_index);
    } else {
      fprintf(stderr, "%s cpu %s: dispatch records missing\n", label,
              order == 0 ? "first" : "second");
      T64_CHECK(false);
    }
    t64_machine_destroy(machine);
  }
}

static void case_characterize_fifo(void) {
  size_t first;
  size_t second;

  /* Every non-reserved ledger pair, both insertion orders, label-only events
   * through the real scheduler: 13 rows, 156 ordered pairs. */
  for (first = 0u; first < NON_RESERVED_ROWS; first++) {
    for (second = 0u; second < NON_RESERVED_ROWS; second++) {
      if (first != second) check_label_pair(first, second);
    }
  }

  /* Pairs an existing builder places at one tick, with real device events. */
  check_real_pair("pi-sp-complete", SKELETON_PROFILE, arm_pi, arm_sp_dma, 1u,
                  2u);
  check_real_pair("vi-ai-sample", FAMILY_STEM, arm_vi, arm_ai, 6u, 7u);
  check_cpu_pair("cpu-pi", arm_pi, 1u);
  check_cpu_pair("cpu-rsp-issue", arm_rsp, 3u);
  check_cpu_pair("cpu-input", arm_input, 4u);
  check_cpu_pair("cpu-rdp", arm_rdp, 5u);
  check_cpu_pair("cpu-vi", arm_vi, 6u);
  check_cpu_pair("cpu-ai", arm_ai, 7u);
}

int main(int argc, char **argv) {
  static const char *const cases[] = {"ledger", "characterize-fifo"};

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    size_t index;

    for (index = 0u; index < sizeof cases / sizeof cases[0]; index++) {
      printf("%s\n", cases[index]);
    }
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    if (strcmp(argv[2], "ledger") == 0) {
      case_ledger();
      return T64_TEST_FINISH("unit.core.sched.ledger");
    }
    if (strcmp(argv[2], "characterize-fifo") == 0) {
      case_characterize_fifo();
      return T64_TEST_FINISH("unit.core.sched.characterize-fifo");
    }
    fprintf(stderr, "unknown case: %s\n", argv[2]);
    return 2;
  }
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--list-cases|--case NAME]\n", argv[0]);
    return 2;
  }
  case_ledger();
  case_characterize_fifo();
  return T64_TEST_FINISH("unit.core.sched");
}
