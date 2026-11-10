#include "t64_internal.h"

#include <stdint.h>
#include <string.h>

static bool output_range_is_disjoint(const t64_machine *machine,
                                     t64_address_range output_range) {
  t64_address_range owned_range;

  if (!t64_internal_make_range(machine->machine_storage.data,
                               machine->machine_storage.capacity,
                               &owned_range) ||
      t64_internal_ranges_overlap(output_range, owned_range)) {
    return false;
  }
  if (machine->has_image &&
      (!t64_internal_make_range(machine->image_storage.data,
                                machine->image_storage.capacity,
                                &owned_range) ||
       t64_internal_ranges_overlap(output_range, owned_range))) {
    return false;
  }
  return true;
}

static bool output_range_is_valid(const t64_machine *machine, void *output,
                                  size_t size, t64_address_range *range_out) {
  return t64_internal_make_range(output, size, range_out) &&
         output_range_is_disjoint(machine, *range_out);
}

t64_status t64_machine_trace_set_enabled(t64_machine *machine, bool enabled) {
  if (machine == NULL) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  machine->trace_enabled = enabled;
  return T64_STATUS_OK;
}

t64_status t64_machine_trace_drain(
    t64_machine *machine, t64_trace_record *records_out, size_t capacity,
    size_t *count_out, uint64_t *dropped_out,
    bool *drop_count_saturated_out) {
  t64_address_range records_range;
  t64_address_range count_range;
  t64_address_range dropped_range;
  t64_address_range saturated_range;
  size_t copy_count;
  size_t i;

  if (machine == NULL || count_out == NULL || dropped_out == NULL ||
      drop_count_saturated_out == NULL ||
      (records_out == NULL && capacity != 0u) ||
      capacity > SIZE_MAX / sizeof *records_out ||
      !output_range_is_valid(machine, count_out, sizeof *count_out,
                             &count_range) ||
      !output_range_is_valid(machine, dropped_out, sizeof *dropped_out,
                             &dropped_range) ||
      !output_range_is_valid(machine, drop_count_saturated_out,
                             sizeof *drop_count_saturated_out,
                             &saturated_range) ||
      t64_internal_ranges_overlap(count_range, dropped_range) ||
      t64_internal_ranges_overlap(count_range, saturated_range) ||
      t64_internal_ranges_overlap(dropped_range, saturated_range)) {
    return T64_STATUS_INVALID_ARGUMENT;
  }
  if (capacity > 0u &&
      (!output_range_is_valid(machine, records_out,
                              capacity * sizeof *records_out,
                              &records_range) ||
       t64_internal_ranges_overlap(records_range, count_range) ||
       t64_internal_ranges_overlap(records_range, dropped_range) ||
       t64_internal_ranges_overlap(records_range, saturated_range))) {
    return T64_STATUS_INVALID_ARGUMENT;
  }

  copy_count = machine->trace_count < capacity ? machine->trace_count
                                                : capacity;
  for (i = 0u; i < copy_count; i++) {
    size_t index = (machine->trace_head + i) % T64_TRACE_RECORD_CAPACITY;

    records_out[i].tick = machine->trace_records[index].tick;
    records_out[i].device = machine->trace_records[index].device;
    records_out[i].kind = machine->trace_records[index].kind;
    records_out[i].value = machine->trace_records[index].value;
  }
  if (copy_count > 0u) {
    machine->trace_head =
        (machine->trace_head + copy_count) % T64_TRACE_RECORD_CAPACITY;
    machine->trace_count -= copy_count;
    if (machine->trace_count == 0u) {
      machine->trace_head = 0u;
    }
  }
  *count_out = copy_count;
  *dropped_out = machine->trace_drop_count;
  *drop_count_saturated_out = machine->trace_drop_count_saturated;
  return T64_STATUS_OK;
}

void t64_trace_reset(t64_machine *machine) {
  if (machine == NULL) {
    return;
  }
  machine->trace_head = 0u;
  machine->trace_count = 0u;
  machine->trace_drop_count = 0u;
  machine->trace_drop_count_saturated = false;
  (void)memset(machine->trace_records, 0, sizeof machine->trace_records);
}

void t64_trace_emit(t64_machine *machine, uint64_t tick, uint32_t device,
                    uint32_t kind, uint64_t value) {
  size_t index;
  t64_trace_record *record;

  if (machine == NULL || !machine->trace_enabled) {
    return;
  }
  if (machine->trace_count >= T64_TRACE_RECORD_CAPACITY) {
    if (machine->trace_drop_count == UINT64_MAX) {
      machine->trace_drop_count_saturated = true;
    } else {
      machine->trace_drop_count++;
    }
    return;
  }
  index = (machine->trace_head + machine->trace_count) %
          T64_TRACE_RECORD_CAPACITY;
  record = &machine->trace_records[index];
  record->tick = tick;
  record->device = device;
  record->kind = kind;
  record->value = value;
  machine->trace_count++;
}
