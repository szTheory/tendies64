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
};

static const char *suite_finish_name(const char *suite) {
  if (strcmp(suite, "atomic") == 0) return "unit.core.si.atomic-queue-full";
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
