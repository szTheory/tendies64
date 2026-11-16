#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_test.h"
#include "t64_internal.h"
#include "tendies64/t64_machine.h"

#define RDP_VI_PROFILE_ID "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v1"
#define DP_START UINT32_C(0x04100000)
#define DP_END (DP_START + UINT32_C(4))
#define DP_CURRENT (DP_START + UINT32_C(8))
#define DP_STATUS (DP_START + UINT32_C(12))
#define DP_BUSY UINT32_C(1)
#define DP_ERROR UINT32_C(2)
#define TEST_ROM_CAPACITY 65536u

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  void *memory;

  (void)context;
  if (storage_out == NULL || alignment == 0u) return false;
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

static bool load_authored_rom(uint8_t *bytes, size_t capacity,
                              size_t *length_out) {
  const char *path = getenv("T64_TEST_RDP_FILL_PATH");
  FILE *file;
  long length;

  if (path == NULL || path[0] == '\0' || bytes == NULL || length_out == NULL)
    return false;
  file = fopen(path, "rb");
  if (file == NULL) return false;
  if (fseek(file, 0L, SEEK_END) != 0 || (length = ftell(file)) <= 0 ||
      (unsigned long)length > capacity || fseek(file, 0L, SEEK_SET) != 0) {
    (void)fclose(file);
    return false;
  }
  if (fread(bytes, 1u, (size_t)length, file) != (size_t)length ||
      fclose(file) != 0) {
    return false;
  }
  *length_out = (size_t)length;
  return true;
}

static t64_machine *create_test_machine(void) {
  const t64_host_allocator allocator = {test_allocate, test_release, NULL};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t rom[TEST_ROM_CAPACITY];
  size_t rom_length = 0u;

  T64_CHECK(load_authored_rom(rom, sizeof rom, &rom_length));
  if (rom_length == 0u) return NULL;
  T64_CHECK_INT(t64_profile_lookup(RDP_VI_PROFILE_ID, &profile), T64_STATUS_OK);
  if (profile == NULL) return NULL;
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  if (machine == NULL) return NULL;
  T64_CHECK_INT(t64_machine_admit_rom(machine, rom, rom_length),
                T64_STATUS_OK);
  return machine;
}

static void write_command(t64_machine *machine, uint32_t address,
                          uint64_t word) {
  uint32_t i;
  for (i = 0u; i < 8u; i++)
    machine->ram[address + i] = (uint8_t)(word >> (56u - i * 8u));
}

static t64_status dispatch_rdp_at_current_tick(t64_machine *machine) {
  const t64_event event = {machine->tick, 0u, T64_RDP_DEVICE,
                           T64_RDP_EVENT_WORK, 0u, 0u, NULL};
  return t64_rdp_event(machine, &event);
}

static void case_dpc_register_acceptance(void) {
  const t64_host_allocator allocator = {test_allocate, test_release, NULL};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint32_t value = 0u;

  T64_CHECK_INT(t64_profile_lookup(RDP_VI_PROFILE_ID, &profile), T64_STATUS_OK);
  T64_CHECK(profile != NULL);
  if (profile == NULL) return;
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_START, 4u, UINT32_C(0x100)),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_read(machine, DP_START, 4u, &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, UINT32_C(0x100));
  t64_machine_destroy(machine);
}

static void check_fill_command_byte(uint8_t byte) {
  const t64_host_allocator allocator = {test_allocate, test_release, NULL};
  const uint64_t word = ((uint64_t)byte << 56) | UINT64_C(0x000000f80107c1);
  const uint8_t command[8] = {
      (uint8_t)(word >> 56), (uint8_t)(word >> 48),
      (uint8_t)(word >> 40), (uint8_t)(word >> 32),
      (uint8_t)(word >> 24), (uint8_t)(word >> 16),
      (uint8_t)(word >> 8), (uint8_t)word};
  const t64_event event = {0u, 1u, T64_RDP_DEVICE, T64_RDP_EVENT_WORK,
                           0u, 0u, NULL};
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  t64_status status;

  T64_CHECK_INT(t64_profile_lookup(RDP_VI_PROFILE_ID, &profile),
                T64_STATUS_OK);
  if (profile == NULL) return;
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  if (machine == NULL) return;
  memcpy(machine->ram, command, sizeof command);
  machine->rdp.dpc_end = sizeof command;
  machine->rdp.busy = true;
  machine->rdp.dpc_status = T64_RDP_STATUS_BUSY;
  status = t64_rdp_event(machine, &event);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(machine->rdp.dpc_current, sizeof command);
  T64_CHECK_INT(machine->rdp.command_count, 1u);
  T64_CHECK(machine->rdp.last_original_word == word);
  T64_CHECK_INT(machine->rdp.last_command_byte, byte);
  T64_CHECK_INT(machine->rdp.last_opcode_field, UINT32_C(0x37));
  T64_CHECK_INT(machine->rdp.fill_color, UINT32_C(0xf80107c1));
  T64_CHECK(!machine->rdp.busy);
  t64_machine_destroy(machine);
}

static void case_direct_opcode_mask_preserves_original(void) {
  check_fill_command_byte(UINT8_C(0x37));
  check_fill_command_byte(UINT8_C(0xf7));
}

static void case_bad_span_admission_preserves_state(void) {
  t64_machine *machine = create_test_machine();
  const uint32_t invalid_starts[] = {
      UINT32_C(0x104), UINT32_C(0xfffffff8), UINT32_C(0x00400000)};
  size_t i;

  if (machine == NULL) return;
  machine->rdp.dpc_start = UINT32_C(0x100);
  machine->rdp.dpc_current = UINT32_C(0x100);
  machine->rdp.dpc_end = UINT32_C(0x100);
  machine->mi_pending = UINT32_C(0x20);
  machine->mi_mask = UINT32_C(0x20);
  memset(machine->ram + UINT32_C(0x100), 0xa5, 16u);

  for (i = 0u; i < sizeof invalid_starts / sizeof invalid_starts[0]; i++) {
    uint8_t digest_before[T64_DIGEST_SIZE];
    uint8_t digest_after[T64_DIGEST_SIZE];
    uint32_t start_before = machine->rdp.dpc_start;
    uint32_t current_before = machine->rdp.dpc_current;
    uint32_t end_before = machine->rdp.dpc_end;
    size_t queued_before = machine->queue_count;
    uint64_t tick_before = machine->tick;

    T64_CHECK_INT(t64_machine_digest(machine, digest_before), T64_STATUS_OK);
    T64_CHECK_INT(t64_machine_bus_write(machine, DP_START, 4u,
                                        invalid_starts[i]),
                  T64_STATUS_BUS_DEVICE_REFUSED);
    T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
    T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) == 0);
    T64_CHECK_INT(machine->rdp.dpc_start, start_before);
    T64_CHECK_INT(machine->rdp.dpc_current, current_before);
    T64_CHECK_INT(machine->rdp.dpc_end, end_before);
    T64_CHECK_INT(machine->queue_count, queued_before);
    T64_CHECK_INT(machine->tick, tick_before);
    T64_CHECK_INT(machine->mi_pending, UINT32_C(0x20));
    T64_CHECK_INT(machine->mi_mask, UINT32_C(0x20));
    T64_CHECK(memcmp(machine->ram + UINT32_C(0x100),
                     (const uint8_t[]){0xa5, 0xa5, 0xa5, 0xa5,
                                       0xa5, 0xa5, 0xa5, 0xa5,
                                       0xa5, 0xa5, 0xa5, 0xa5,
                                       0xa5, 0xa5, 0xa5, 0xa5},
                     16u) == 0);
  }
  t64_machine_destroy(machine);
}

static void case_truncated_word_refuses_without_execution(void) {
  t64_machine *machine = create_test_machine();
  const t64_event event = {0u, 1u, T64_RDP_DEVICE, T64_RDP_EVENT_WORK,
                           0u, 0u, NULL};
  uint8_t before[16];
  size_t queued_before;

  if (machine == NULL) return;
  memset(machine->ram + UINT32_C(0x100), 0x6d, sizeof before);
  memcpy(before, machine->ram + UINT32_C(0x100), sizeof before);
  machine->rdp.dpc_start = UINT32_C(0x100);
  machine->rdp.dpc_current = UINT32_C(0x100);
  machine->rdp.dpc_end = UINT32_C(0x104);
  machine->rdp.dpc_status = DP_BUSY;
  machine->rdp.busy = true;
  queued_before = machine->queue_count;
  machine->mi_pending = UINT32_C(0x08);
  machine->mi_mask = UINT32_C(0x20);

  T64_CHECK_INT(t64_rdp_event(machine, &event), T64_STATUS_OK);
  T64_CHECK_INT(machine->rdp.refusal_reason,
                T64_RDP_REFUSAL_TRUNCATED_WORD);
  T64_CHECK_INT(machine->rdp.dpc_current, UINT32_C(0x100));
  T64_CHECK_INT(machine->rdp.dpc_end, UINT32_C(0x104));
  T64_CHECK_INT(machine->rdp.command_count, 0u);
  T64_CHECK_INT(machine->rdp.fill_count, 0u);
  T64_CHECK_INT(machine->rdp.fullsync_count, 0u);
  T64_CHECK_INT(machine->rdp.dpc_status, DP_ERROR);
  T64_CHECK(!machine->rdp.busy);
  T64_CHECK(!machine->rdp.render_active);
  T64_CHECK(!machine->rdp.fence_pending);
  T64_CHECK_INT(machine->queue_count, queued_before);
  T64_CHECK_INT(machine->mi_pending, UINT32_C(0x08));
  T64_CHECK_INT(machine->mi_mask, UINT32_C(0x20));
  T64_CHECK(memcmp(machine->ram + UINT32_C(0x100), before, sizeof before) == 0);
  t64_machine_destroy(machine);
}

static void seed_fill_context(t64_machine *machine, uint32_t end,
                              uint32_t scissor_xh, uint32_t scissor_xl) {
  machine->rdp.dpc_start = UINT32_C(0x100);
  machine->rdp.dpc_current = UINT32_C(0x100);
  machine->rdp.dpc_end = end;
  machine->rdp.dpc_status = DP_BUSY;
  machine->rdp.busy = true;
  machine->rdp.color_image_address = UINT32_C(0x3000);
  machine->rdp.color_image_width = 4u;
  machine->rdp.color_image_format = 0u;
  machine->rdp.color_image_size = 2u;
  machine->rdp.color_image_valid = true;
  machine->rdp.scissor_xh = scissor_xh;
  machine->rdp.scissor_yh = 0u;
  machine->rdp.scissor_xl = scissor_xl;
  machine->rdp.scissor_yl = 8u;
  machine->rdp.scissor_valid = true;
  machine->rdp.other_modes = UINT64_C(0x0030000000000000);
  machine->rdp.other_modes_valid = true;
  machine->rdp.fill_color = UINT32_C(0xf80107c1);
  machine->rdp.fill_color_valid = true;
  memset(machine->ram + UINT32_C(0x3000), 0xcc, 24u);
  write_command(machine, UINT32_C(0x100), UINT64_C(0x3600c00800000000));
}

static void case_unsupported_after_fill_preserves_completed_work(void) {
  t64_machine *machine = create_test_machine();
  uint32_t row;
  uint32_t byte;
  size_t queued_before_refusal;

  if (machine == NULL) return;
  machine->rdp.dpc_start = UINT32_C(0x100);
  machine->rdp.dpc_current = UINT32_C(0x100);
  machine->rdp.dpc_end = UINT32_C(0x130);
  machine->rdp.dpc_status = DP_BUSY;
  machine->rdp.busy = true;
  machine->mi_mask = UINT32_C(0x20);
  write_command(machine, UINT32_C(0x100), UINT64_C(0x3f10000300003000));
  write_command(machine, UINT32_C(0x108), UINT64_C(0x2d0000000000c00c));
  write_command(machine, UINT32_C(0x110), UINT64_C(0x2f30000000000000));
  write_command(machine, UINT32_C(0x118), UINT64_C(0x37000000f80107c1));
  write_command(machine, UINT32_C(0x120), UINT64_C(0x3600c00800000000));
  write_command(machine, UINT32_C(0x128), UINT64_C(0x3e00000000000000));
  memset(machine->ram + UINT32_C(0x3000), 0xcc, 24u);

  while (machine->rdp.dpc_current < UINT32_C(0x128) ||
         machine->rdp.render_active) {
    machine->tick++;
    T64_CHECK_INT(dispatch_rdp_at_current_tick(machine), T64_STATUS_OK);
  }
  T64_CHECK_INT(machine->rdp.dpc_current, UINT32_C(0x128));
  T64_CHECK_INT(machine->rdp.command_count, 5u);
  T64_CHECK_INT(machine->rdp.fill_count, 1u);
  T64_CHECK(!machine->rdp.render_active);
  for (row = 0u; row < 3u; row++) {
    for (byte = 0u; byte < 8u; byte++) {
      static const uint8_t expected_row[] = {
          0xf8, 0x01, 0x07, 0xc1, 0xf8, 0x01, 0x07, 0xc1};
      T64_CHECK_INT(machine->ram[UINT32_C(0x3000) + row * 8u + byte],
                    expected_row[byte]);
    }
  }

  queued_before_refusal = machine->queue_count;
  machine->tick++;
  T64_CHECK_INT(dispatch_rdp_at_current_tick(machine), T64_STATUS_OK);
  T64_CHECK_INT(machine->rdp.refusal_reason,
                T64_RDP_REFUSAL_UNSUPPORTED_OPCODE);
  T64_CHECK_INT(machine->rdp.last_command_byte, UINT8_C(0x3e));
  T64_CHECK_INT(machine->rdp.dpc_current, UINT32_C(0x128));
  T64_CHECK_INT(machine->rdp.command_count, 5u);
  T64_CHECK_INT(machine->rdp.fill_count, 1u);
  T64_CHECK_INT(machine->rdp.fullsync_count, 0u);
  T64_CHECK_INT(machine->queue_count, queued_before_refusal);
  T64_CHECK(machine->rdp.dpc_status & DP_ERROR);
  T64_CHECK(!machine->rdp.busy);
  T64_CHECK(!machine->rdp.fence_pending);
  T64_CHECK_INT(machine->mi_pending, 0u);
  T64_CHECK_INT(machine->mi_mask, UINT32_C(0x20));
  T64_CHECK_INT(machine->vi.packet_count, 0u);
  t64_machine_destroy(machine);
}

static void case_scheduler_full_leaves_end_unlatched(void) {
  t64_machine *machine = create_test_machine();
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  size_t full_count;
  uint64_t sequence_before;
  uint64_t tick_before;

  if (machine == NULL) return;
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_START, 4u,
                                      UINT32_C(0x100)), T64_STATUS_OK);
  full_count = machine->profile.event_queue_capacity;
  if (full_count > T64_EVENT_QUEUE_CAPACITY)
    full_count = T64_EVENT_QUEUE_CAPACITY;
  machine->queue_count = full_count;
  T64_CHECK_INT(t64_machine_digest(machine, digest_before), T64_STATUS_OK);
  sequence_before = machine->next_sequence;
  tick_before = machine->tick;

  T64_CHECK_INT(t64_machine_bus_write(machine, DP_END, 4u,
                                      UINT32_C(0x108)), T64_STATUS_QUEUE_FULL);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) == 0);
  T64_CHECK_INT(machine->rdp.dpc_start, UINT32_C(0x100));
  T64_CHECK_INT(machine->rdp.dpc_current, UINT32_C(0x100));
  T64_CHECK_INT(machine->rdp.dpc_end, 0u);
  T64_CHECK_INT(machine->rdp.dpc_status, 0u);
  T64_CHECK(!machine->rdp.busy);
  T64_CHECK_INT(machine->queue_count, full_count);
  T64_CHECK_INT(machine->next_sequence, sequence_before);
  T64_CHECK_INT(machine->tick, tick_before);
  T64_CHECK_INT(machine->mi_pending, 0u);
  T64_CHECK_INT(machine->mi_mask, 0u);
  t64_machine_destroy(machine);
}

static void case_empty_list_has_no_work(void) {
  t64_machine *machine = create_test_machine();
  size_t queued_before;

  if (machine == NULL) return;
  queued_before = machine->queue_count;
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_START, 4u,
                                      UINT32_C(0x100)), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_END, 4u,
                                      UINT32_C(0x100)), T64_STATUS_OK);
  T64_CHECK_INT(machine->rdp.dpc_start, UINT32_C(0x100));
  T64_CHECK_INT(machine->rdp.dpc_current, UINT32_C(0x100));
  T64_CHECK_INT(machine->rdp.dpc_end, UINT32_C(0x100));
  T64_CHECK_INT(machine->rdp.dpc_status, 0u);
  T64_CHECK_INT(machine->queue_count, queued_before);
  T64_CHECK_INT(machine->rdp.command_count, 0u);
  T64_CHECK_INT(machine->rdp.fill_count, 0u);
  T64_CHECK_INT(machine->rdp.fullsync_count, 0u);
  T64_CHECK_INT(machine->mi_pending, 0u);
  T64_CHECK_INT(machine->vi.packet_count, 0u);
  t64_machine_destroy(machine);
}

static void case_single_fill_completes_without_fullsync(void) {
  t64_machine *machine = create_test_machine();
  uint32_t i;

  if (machine == NULL) return;
  seed_fill_context(machine, UINT32_C(0x108), 0u, UINT32_C(12));
  for (i = 0u; machine->rdp.busy && i < 8u; i++) {
    machine->tick++;
    T64_CHECK_INT(dispatch_rdp_at_current_tick(machine), T64_STATUS_OK);
  }
  T64_CHECK_INT(machine->rdp.dpc_current, UINT32_C(0x108));
  T64_CHECK_INT(machine->rdp.command_count, 1u);
  T64_CHECK_INT(machine->rdp.fill_count, 1u);
  T64_CHECK_INT(machine->rdp.fullsync_count, 0u);
  T64_CHECK_INT(machine->rdp.dpc_status, 0u);
  T64_CHECK(!machine->rdp.busy);
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT(machine->mi_pending, 0u);
  T64_CHECK_INT(machine->vi.packet_count, 0u);
  for (i = 0u; i < 24u; i++) {
    static const uint8_t expected[] = {
        0xf8, 0x01, 0x07, 0xc1, 0xf8, 0x01, 0x07, 0xc1,
        0xf8, 0x01, 0x07, 0xc1, 0xf8, 0x01, 0x07, 0xc1,
        0xf8, 0x01, 0x07, 0xc1, 0xf8, 0x01, 0x07, 0xc1};
    T64_CHECK_INT(machine->ram[UINT32_C(0x3000) + i], expected[i]);
  }
  t64_machine_destroy(machine);
}

static void case_zero_area_fill_is_noop(void) {
  t64_machine *machine = create_test_machine();
  size_t queued_before;
  uint32_t i;

  if (machine == NULL) return;
  seed_fill_context(machine, UINT32_C(0x108), UINT32_C(12), UINT32_C(8));
  queued_before = machine->queue_count;
  T64_CHECK_INT(dispatch_rdp_at_current_tick(machine), T64_STATUS_OK);
  T64_CHECK_INT(machine->rdp.dpc_current, UINT32_C(0x108));
  T64_CHECK_INT(machine->rdp.command_count, 1u);
  T64_CHECK_INT(machine->rdp.fill_count, 0u);
  T64_CHECK_INT(machine->rdp.fullsync_count, 0u);
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK(!machine->rdp.render_active);
  T64_CHECK(!machine->rdp.busy);
  T64_CHECK_INT(machine->rdp.dpc_status, 0u);
  T64_CHECK_INT(machine->queue_count, queued_before);
  T64_CHECK_INT(machine->mi_pending, 0u);
  T64_CHECK_INT(machine->vi.packet_count, 0u);
  for (i = 0u; i < 24u; i++)
    T64_CHECK_INT(machine->ram[UINT32_C(0x3000) + i], UINT8_C(0xcc));
  t64_machine_destroy(machine);
}

typedef struct {
  const char *name;
  void (*run)(void);
} test_case;

static const test_case cases[] = {
    {"dpc-register-acceptance", case_dpc_register_acceptance},
    {"direct-opcode-mask-preserves-original",
     case_direct_opcode_mask_preserves_original},
    {"bad-span-admission-preserves-state",
     case_bad_span_admission_preserves_state},
    {"truncated-word-refuses-without-execution",
     case_truncated_word_refuses_without_execution},
    {"unsupported-after-fill-preserves-completed-work",
     case_unsupported_after_fill_preserves_completed_work},
    {"scheduler-full-leaves-end-unlatched",
     case_scheduler_full_leaves_end_unlatched},
    {"empty-list-has-no-work", case_empty_list_has_no_work},
    {"single-fill-completes-without-fullsync",
     case_single_fill_completes_without_fullsync},
    {"zero-area-fill-is-noop", case_zero_area_fill_is_noop},
};

int main(int argc, char **argv) {
  size_t i;

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) {
      puts(cases[i].name);
    }
    return 0;
  }
  if (argc != 1) return 2;
  for (i = 0u; i < sizeof cases / sizeof cases[0]; i++) cases[i].run();
  return T64_TEST_FINISH("unit.core.rdp.fill");
}
