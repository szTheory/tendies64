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
#define RSP_IMEM_BASE UINT32_C(0x04001000)
#define RSP_FUNCTIONAL_PROFILE_ID "ntsc-cart-4m-openipl3-rsp-functional-v1"

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

static void clear_queued_events(t64_machine *machine) {
  machine->queue_head = 0u;
  machine->queue_count = 0u;
}

static t64_status dispatch_next_queued_event(t64_machine *machine) {
  t64_event event;

  if (machine->queue_count == 0u) return T64_STATUS_NOT_READY;
  event = machine->events[machine->queue_head];
  machine->queue_head = (machine->queue_head + 1u) % T64_EVENT_QUEUE_CAPACITY;
  machine->queue_count--;
  machine->tick = event.tick;
  return event.handler == NULL ? T64_STATUS_INVALID_ARGUMENT
                               : event.handler(machine, &event);
}

#ifndef T64_TEST_DP_TRANSPORT
static t64_status dispatch_rdp_at_current_tick(t64_machine *machine) {
  const t64_event event = {machine->tick, 0u, T64_RDP_DEVICE,
                           T64_RDP_EVENT_WORK, 0u, 0u, NULL};
  return t64_rdp_event(machine, &event);
}
#endif

#ifndef T64_TEST_DP_TRANSPORT
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
  const uint64_t word = ((uint64_t)byte << 56) | UINT64_C(0x000000f80107c1);
  t64_machine *machine = create_test_machine();

  if (machine == NULL) return;
  clear_queued_events(machine);
  write_command(machine, UINT32_C(0x100), word);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_START, 4u,
                                      UINT32_C(0x100)), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_END, 4u,
                                      UINT32_C(0x108)), T64_STATUS_OK);
  T64_CHECK_INT(dispatch_next_queued_event(machine), T64_STATUS_OK);
  T64_CHECK_INT(machine->rdp.dpc_current, UINT32_C(0x108));
  T64_CHECK(machine->rdp.command_pending);
  T64_CHECK_INT(dispatch_next_queued_event(machine), T64_STATUS_OK);
  T64_CHECK_INT(machine->rdp.dpc_current, UINT32_C(0x108));
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

static void begin_fill_transport(t64_machine *machine, uint32_t end) {
  clear_queued_events(machine);
  machine->rdp.dpc_start = 0u;
  machine->rdp.dpc_current = 0u;
  machine->rdp.dpc_end = 0u;
  machine->rdp.dpc_status = 0u;
  machine->rdp.busy = false;
  machine->rdp.dpc_dma_active = false;
  machine->rdp.dpc_start_valid = false;
  machine->rdp.dpc_end_valid = false;
  machine->rdp.command_pending = false;
  machine->rdp.render_active = false;
  machine->rdp.fence_pending = false;
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_START, 4u,
                                      UINT32_C(0x100)), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_END, 4u, end),
                T64_STATUS_OK);
  T64_CHECK_INT(machine->queue_count, 1u);
}

static void case_unsupported_after_fill_preserves_completed_work(void) {
  t64_machine *machine = create_test_machine();
  uint32_t row;
  uint32_t byte;
  size_t queued_before_refusal;

  if (machine == NULL) return;
  machine->mi_mask = UINT32_C(0x20);
  write_command(machine, UINT32_C(0x100), UINT64_C(0x3f10000300003000));
  write_command(machine, UINT32_C(0x108), UINT64_C(0x2d0000000000c00c));
  write_command(machine, UINT32_C(0x110), UINT64_C(0x2f30000000000000));
  write_command(machine, UINT32_C(0x118), UINT64_C(0x37000000f80107c1));
  write_command(machine, UINT32_C(0x120), UINT64_C(0x3600c00800000000));
  write_command(machine, UINT32_C(0x128), UINT64_C(0x3e00000000000000));
  memset(machine->ram + UINT32_C(0x3000), 0xcc, 24u);
  begin_fill_transport(machine, UINT32_C(0x130));

  while (machine->rdp.dpc_current < UINT32_C(0x128) ||
         machine->rdp.render_active || machine->rdp.command_pending) {
    T64_CHECK_INT(dispatch_next_queued_event(machine), T64_STATUS_OK);
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
  begin_fill_transport(machine, UINT32_C(0x108));
  for (i = 0u; machine->rdp.busy && i < 16u; i++)
    T64_CHECK_INT(dispatch_next_queued_event(machine), T64_STATUS_OK);
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
  unsigned steps;
  uint32_t i;

  if (machine == NULL) return;
  seed_fill_context(machine, UINT32_C(0x108), UINT32_C(12), UINT32_C(8));
  begin_fill_transport(machine, UINT32_C(0x108));
  for (steps = 0u; steps < 4u && machine->queue_count != 0u; steps++)
    T64_CHECK_INT(dispatch_next_queued_event(machine), T64_STATUS_OK);
  T64_CHECK_INT(machine->rdp.dpc_current, UINT32_C(0x108));
  T64_CHECK_INT(machine->rdp.command_count, 1u);
  T64_CHECK_INT(machine->rdp.fill_count, 0u);
  T64_CHECK_INT(machine->rdp.fullsync_count, 0u);
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK(!machine->rdp.render_active);
  T64_CHECK(!machine->rdp.busy);
  T64_CHECK_INT(machine->rdp.dpc_status, 0u);
  T64_CHECK_INT(machine->queue_count, 0u);
  T64_CHECK_INT(machine->mi_pending, 0u);
  T64_CHECK_INT(machine->vi.packet_count, 0u);
  for (i = 0u; i < 24u; i++)
    T64_CHECK_INT(machine->ram[UINT32_C(0x3000) + i], UINT8_C(0xcc));
  t64_machine_destroy(machine);
}
#else
#define DPC_XBUS_READ UINT32_C(0x00000001)
#define DPC_SET_XBUS UINT32_C(0x00000002)
#define DPC_FREEZE UINT32_C(0x00000002)
#define DPC_FLUSH UINT32_C(0x00000004)
#define DPC_PIPE_BUSY UINT32_C(0x00000020)
#define DPC_CMD_BUSY UINT32_C(0x00000040)
#define DPC_DMA_BUSY UINT32_C(0x00000100)
#define DPC_END_VALID UINT32_C(0x00000200)
#define DPC_START_VALID UINT32_C(0x00000400)
#define MI_MODE UINT32_C(0x04300000)
#define MI_INTR_MASK UINT32_C(0x0430000c)
#define SP_DMEM UINT32_C(0x04000000)
#define DPC_CLOCK_COUNTER UINT32_C(0x04100010)
#define DPC_BUFFER_COUNTER UINT32_C(0x04100014)
#define DPC_PIPE_COUNTER UINT32_C(0x04100018)
#define DPC_TMEM_COUNTER UINT32_C(0x0410001c)

static t64_machine *create_rsp_only_test_machine(void) {
  const t64_host_allocator allocator = {test_allocate, test_release, NULL};
  const t64_profile_descriptor *profile = NULL;
  t64_host_storage image = {NULL, 0u, 0u};
  t64_machine *machine = NULL;

  if (t64_profile_lookup(RSP_FUNCTIONAL_PROFILE_ID, &profile) !=
          T64_STATUS_OK ||
      profile == NULL ||
      t64_machine_create(&allocator, profile, &machine) != T64_STATUS_OK ||
      machine == NULL) {
    return NULL;
  }
  if (!test_allocate(NULL, 2u, 1u, &image)) {
    t64_machine_destroy(machine);
    return NULL;
  }
  ((uint8_t *)image.data)[0] = UINT8_C(0x5a);
  ((uint8_t *)image.data)[1] = UINT8_C(0xa5);
  machine->image_storage = image;
  machine->image_length = 2u;
  machine->has_image = true;
  return machine;
}

static uint32_t encode_rsp_cop0(bool write, uint32_t rt, uint32_t rd) {
  return (UINT32_C(0x10) << 26) |
         ((write ? UINT32_C(4) : UINT32_C(0)) << 21) | (rt << 16) |
         (rd << 11);
}

static bool install_rsp_program(t64_machine *machine,
                                const uint32_t *instructions,
                                size_t instruction_count) {
  size_t index;

  if (machine == NULL || instructions == NULL || instruction_count == 0u ||
      instruction_count > 1024u) {
    return false;
  }
  machine->rsp.pc = 0u;
  machine->rsp.sp_status |= UINT32_C(1);
  for (index = 0u; index < instruction_count; index++) {
    if (t64_machine_bus_write(machine,
                              RSP_IMEM_BASE + (uint32_t)(index * 4u), 4u,
                              instructions[index]) != T64_STATUS_OK) {
      return false;
    }
  }
  return true;
}

static bool advance_to_tick(t64_machine *machine, uint64_t target_tick) {
  while (machine != NULL && machine->tick < target_tick) {
    t64_stop_request request = {T64_STOP_GUEST_TICK,
                                machine->tick + UINT64_C(1)};
    t64_stop_result result = {0u, 0u};

    if (t64_machine_advance(machine, &request, &result) != T64_STATUS_OK ||
        result.tick != request.value) {
      return false;
    }
  }
  return machine != NULL && machine->tick == target_tick;
}

static bool queued_sequence(const t64_machine *machine, uint64_t tick,
                            uint32_t device, uint32_t kind,
                            uint64_t *sequence_out) {
  size_t index;

  if (machine == NULL || sequence_out == NULL) return false;
  for (index = 0u; index < machine->queue_count; index++) {
    size_t event_index =
        (machine->queue_head + index) % T64_EVENT_QUEUE_CAPACITY;
    const t64_event *event = &machine->events[event_index];

    if (event->tick == tick && event->device == device &&
        event->kind == kind) {
      *sequence_out = event->sequence;
      return true;
    }
  }
  return false;
}

static bool seed_transport(t64_machine *machine, uint32_t start,
                           uint32_t end) {
  clear_queued_events(machine);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_START, 4u, start),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_END, 4u, end),
                T64_STATUS_OK);
  return machine->rdp.dpc_start == start && machine->rdp.dpc_current == start;
}

static void seed_fill_state_only(t64_machine *machine) {
  machine->rdp.color_image_address = UINT32_C(0x3000);
  machine->rdp.color_image_width = 4u;
  machine->rdp.color_image_format = 0u;
  machine->rdp.color_image_size = 2u;
  machine->rdp.color_image_valid = true;
  machine->rdp.scissor_xh = 0u;
  machine->rdp.scissor_yh = 0u;
  machine->rdp.scissor_xl = UINT32_C(12);
  machine->rdp.scissor_yl = UINT32_C(8);
  machine->rdp.scissor_valid = true;
  machine->rdp.other_modes = UINT64_C(0x0030000000000000);
  machine->rdp.other_modes_valid = true;
  machine->rdp.fill_color = UINT32_C(0xf80107c1);
  machine->rdp.fill_color_valid = true;
  (void)memset(machine->ram + UINT32_C(0x3000), 0xcc, 24u);
}

static void case_current_end_retains_busy_for_pipeline(void) {
  t64_machine *machine = create_test_machine();
  uint32_t status = 0u;

  if (machine == NULL) return;
  seed_fill_state_only(machine);
  write_command(machine, UINT32_C(0x100), UINT64_C(0x3600c00800000000));
  (void)seed_transport(machine, UINT32_C(0x100), UINT32_C(0x108));
  T64_CHECK_INT(dispatch_next_queued_event(machine), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_read(machine, DP_CURRENT, 4u, &status),
                T64_STATUS_OK);
  T64_CHECK_INT(status, UINT32_C(0x108));
  T64_CHECK(machine->rdp.command_pending);
  T64_CHECK_INT(dispatch_next_queued_event(machine), T64_STATUS_OK);
  T64_CHECK(machine->rdp.busy);
  T64_CHECK(machine->rdp.render_active);
  T64_CHECK(!machine->rdp.command_pending);
  T64_CHECK_INT(t64_machine_bus_read(machine, DP_STATUS, 4u, &status),
                T64_STATUS_OK);
  T64_CHECK((status & DPC_PIPE_BUSY) != 0u);
  T64_CHECK((status & (DPC_CMD_BUSY | DPC_DMA_BUSY)) == 0u);
  t64_machine_destroy(machine);
}

static void case_fullsync_after_prior_render(void) {
  t64_machine *machine = create_test_machine();
  bool saw_fence = false;
  unsigned steps;

  if (machine == NULL) return;
  seed_fill_state_only(machine);
  write_command(machine, UINT32_C(0x100), UINT64_C(0x3600c00800000000));
  write_command(machine, UINT32_C(0x108), UINT64_C(0xe900000000000000));
  machine->mi_mask = UINT32_C(0x20);
  (void)seed_transport(machine, UINT32_C(0x100), UINT32_C(0x110));
  for (steps = 0u; steps < 32u && machine->queue_count != 0u; steps++) {
    T64_CHECK_INT(dispatch_next_queued_event(machine), T64_STATUS_OK);
    if (machine->rdp.fence_pending) {
      saw_fence = true;
      T64_CHECK_INT(machine->rdp.fill_count, 1u);
      T64_CHECK_INT(machine->mi_pending, 0u);
      T64_CHECK(machine->rdp.busy);
      break;
    }
  }
  T64_CHECK(saw_fence);
  T64_CHECK_INT(dispatch_next_queued_event(machine), T64_STATUS_OK);
  T64_CHECK_INT(machine->mi_pending, UINT32_C(0x20));
  T64_CHECK_INT(machine->rdp.fullsync_count, 1u);
  t64_machine_destroy(machine);
}

static void case_dp_pending_mask_clear_independent(void) {
  t64_machine *machine = create_test_machine();
  uint32_t mask = 0u;

  if (machine == NULL) return;
  write_command(machine, UINT32_C(0x100), UINT64_C(0xe900000000000000));
  machine->mi_mask = 0u;
  (void)seed_transport(machine, UINT32_C(0x100), UINT32_C(0x108));
  while (machine->queue_count != 0u)
    T64_CHECK_INT(dispatch_next_queued_event(machine), T64_STATUS_OK);
  T64_CHECK_INT(machine->mi_pending, UINT32_C(0x20));
  T64_CHECK_INT(t64_machine_bus_write(machine, MI_INTR_MASK, 4u,
                                      UINT32_C(0x0800)), T64_STATUS_OK);
  T64_CHECK_INT(machine->mi_pending, UINT32_C(0x20));
  T64_CHECK(machine->mi_irq_line);
  T64_CHECK_INT(t64_machine_bus_write(machine, MI_INTR_MASK, 4u,
                                      UINT32_C(0x0400)), T64_STATUS_OK);
  T64_CHECK_INT(machine->mi_pending, UINT32_C(0x20));
  T64_CHECK_INT(t64_machine_bus_read(machine, MI_INTR_MASK, 4u, &mask),
                T64_STATUS_OK);
  T64_CHECK((mask & UINT32_C(0x20)) == 0u);
  T64_CHECK(!machine->mi_irq_line);
  T64_CHECK_INT(t64_machine_bus_write(machine, MI_MODE, 4u,
                                      UINT32_C(0x0800)), T64_STATUS_OK);
  T64_CHECK_INT(machine->mi_pending, 0u);
  T64_CHECK_INT(t64_machine_bus_read(machine, MI_INTR_MASK, 4u, &mask),
                T64_STATUS_OK);
  T64_CHECK((mask & UINT32_C(0x20)) == 0u);
  t64_machine_destroy(machine);
}

static void case_repeated_fullsync_fences(void) {
  t64_machine *machine = create_test_machine();
  unsigned steps;

  if (machine == NULL) return;
  write_command(machine, UINT32_C(0x100), UINT64_C(0xe900000000000000));
  write_command(machine, UINT32_C(0x108), UINT64_C(0xe900000000000000));
  (void)seed_transport(machine, UINT32_C(0x100), UINT32_C(0x110));
  for (steps = 0u; steps < 16u && machine->rdp.fullsync_count < 2u;
       steps++) {
    if (machine->queue_count == 0u) break;
    T64_CHECK_INT(dispatch_next_queued_event(machine), T64_STATUS_OK);
  }
  T64_CHECK_INT(machine->rdp.fullsync_count, 2u);
  T64_CHECK_INT(machine->rdp.command_count, 2u);
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT(machine->mi_pending, UINT32_C(0x20));
  T64_CHECK(!machine->rdp.busy);
  t64_machine_destroy(machine);
}

static void case_freeze_flush_controls(void) {
  t64_machine *machine = create_test_machine();
  uint32_t status = 0u;

  if (machine == NULL) return;
  clear_queued_events(machine);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_START, 4u,
                                      UINT32_C(0x100)), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_STATUS, 4u,
                                      UINT32_C(0x0008)), T64_STATUS_OK);
  write_command(machine, UINT32_C(0x100), UINT64_C(0xe900000000000000));
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_END, 4u,
                                      UINT32_C(0x108)), T64_STATUS_OK);
  T64_CHECK_INT(machine->queue_count, 0u);
  T64_CHECK_INT(machine->rdp.dpc_current, UINT32_C(0x100));
  T64_CHECK_INT(t64_machine_bus_read(machine, DP_STATUS, 4u, &status),
                T64_STATUS_OK);
  T64_CHECK((status & DPC_FREEZE) != 0u);
  T64_CHECK((status & (DPC_START_VALID | DPC_END_VALID)) ==
            (DPC_START_VALID | DPC_END_VALID));
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_STATUS, 4u,
                                      UINT32_C(0x0004)), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_STATUS, 4u,
                                      UINT32_C(0x0020)), T64_STATUS_OK);
  T64_CHECK_INT(dispatch_next_queued_event(machine), T64_STATUS_OK);
  T64_CHECK_INT(machine->rdp.dpc_current, UINT32_C(0x100));
  T64_CHECK_INT(t64_machine_bus_read(machine, DP_STATUS, 4u, &status),
                T64_STATUS_OK);
  T64_CHECK((status & DPC_FLUSH) != 0u);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_STATUS, 4u,
                                      UINT32_C(0x0010)), T64_STATUS_OK);
  T64_CHECK_INT(dispatch_next_queued_event(machine), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_read(machine, DP_STATUS, 4u, &status),
                T64_STATUS_OK);
  T64_CHECK((status & DPC_FLUSH) == 0u);
  T64_CHECK((status & (DPC_START_VALID | DPC_END_VALID)) == 0u);
  t64_machine_destroy(machine);
}

static void case_xbus_dmem_bounded(void) {
  t64_machine *machine = create_test_machine();

  if (machine == NULL) return;
  clear_queued_events(machine);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_STATUS, 4u, DPC_SET_XBUS),
                T64_STATUS_OK);
  machine->sp_dmem[0] = UINT8_C(0xe6);
  (void)memset(machine->sp_dmem + 1u, 0, 7u);
  {
    uint32_t status = 0u;
    T64_CHECK_INT(t64_machine_bus_read(machine, DP_STATUS, 4u, &status),
                  T64_STATUS_OK);
    T64_CHECK((status & DPC_XBUS_READ) != 0u);
  }
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_START, 4u, 0u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_END, 4u, 8u),
                T64_STATUS_OK);
  T64_CHECK_INT(dispatch_next_queued_event(machine), T64_STATUS_OK);
  T64_CHECK(machine->rdp.command_pending);
  T64_CHECK_INT(dispatch_next_queued_event(machine), T64_STATUS_OK);
  T64_CHECK_INT(machine->rdp.last_opcode_field, UINT32_C(0x26));
  T64_CHECK_INT(machine->rdp.command_count, 1u);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_START, 4u,
                                      UINT32_C(0x1000)),
                T64_STATUS_BUS_DEVICE_REFUSED);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_END, 4u,
                                      UINT32_C(0x1008)),
                T64_STATUS_BUS_DEVICE_REFUSED);
  t64_machine_destroy(machine);
}

static void case_loadsync_raw_26(void) {
  t64_machine *machine = create_test_machine();

  if (machine == NULL) return;
  write_command(machine, UINT32_C(0x100), UINT64_C(0xe600000000000000));
  (void)seed_transport(machine, UINT32_C(0x100), UINT32_C(0x108));
  while (machine->queue_count != 0u)
    T64_CHECK_INT(dispatch_next_queued_event(machine), T64_STATUS_OK);
  T64_CHECK_INT(machine->rdp.last_command_byte, UINT32_C(0xe6));
  T64_CHECK_INT(machine->rdp.last_opcode_field, UINT32_C(0x26));
  T64_CHECK_INT(machine->rdp.command_count, 1u);
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  t64_machine_destroy(machine);
}

static void case_scheduler_time_sequence_exhaustion_atomic(void) {
  t64_machine *machine = create_test_machine();
  uint32_t end_before;
  size_t queue_before;

  if (machine == NULL) return;
  clear_queued_events(machine);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_START, 4u,
                                      UINT32_C(0x100)), T64_STATUS_OK);
  end_before = machine->rdp.dpc_end;
  queue_before = machine->queue_count;
  machine->tick = UINT64_MAX;
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_END, 4u,
                                      UINT32_C(0x108)),
                T64_STATUS_TICK_OVERFLOW);
  T64_CHECK_INT(machine->rdp.dpc_end, end_before);
  T64_CHECK_INT(machine->queue_count, queue_before);
  machine->tick = 0u;
  machine->profile.sequence_width_bits = 8u;
  machine->next_sequence = UINT64_C(256);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_END, 4u,
                                      UINT32_C(0x108)),
                T64_STATUS_SEQUENCE_EXHAUSTED);
  T64_CHECK_INT(machine->rdp.dpc_end, end_before);
  T64_CHECK_INT(machine->queue_count, queue_before);
  t64_machine_destroy(machine);
}

static void case_reset_clears_pending_work(void) {
  t64_machine *machine = create_test_machine();
  uint8_t rom[TEST_ROM_CAPACITY];
  size_t rom_length = 0u;
  size_t i;
  bool queued_rdp_event = false;
  unsigned steps;

  if (machine == NULL) return;
  write_command(machine, UINT32_C(0x100), UINT64_C(0xe900000000000000));
  (void)seed_transport(machine, UINT32_C(0x100), UINT32_C(0x108));
  for (steps = 0u; steps < 4u && !machine->rdp.fence_pending; steps++) {
    T64_CHECK_INT(dispatch_next_queued_event(machine), T64_STATUS_OK);
  }
  T64_CHECK(machine->rdp.fence_pending);
  T64_CHECK(machine->queue_count != 0u);
  T64_CHECK(load_authored_rom(rom, sizeof rom, &rom_length));
  T64_CHECK_INT(t64_machine_admit_rom(machine, rom, rom_length),
                T64_STATUS_OK);
  for (i = 0u; i < machine->queue_count; i++) {
    size_t event_index = (machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY;
    if (machine->events[event_index].device == T64_RDP_DEVICE)
      queued_rdp_event = true;
  }
  T64_CHECK(!queued_rdp_event);
  T64_CHECK(!machine->rdp.fence_pending);
  T64_CHECK(!machine->rdp.busy);
  T64_CHECK_INT(machine->rdp.dpc_start, 0u);
  T64_CHECK_INT(machine->mi_pending, 0u);
  t64_machine_destroy(machine);
}

static void case_counter_registers_read_only_and_clearable(void) {
  t64_machine *machine = create_test_machine();
  const uint32_t counters[] = {DPC_CLOCK_COUNTER, DPC_BUFFER_COUNTER,
                               DPC_PIPE_COUNTER, DPC_TMEM_COUNTER};
  uint32_t value = 0u;
  size_t i;

  if (machine == NULL) return;
  for (i = 0u; i < sizeof counters / sizeof counters[0]; i++) {
    uint32_t counter_value = UINT32_C(0xffffffff);
    T64_CHECK_INT(t64_machine_bus_read(machine, counters[i], 4u,
                                       &counter_value),
                  T64_STATUS_OK);
    T64_CHECK_INT(counter_value, 0u);
    T64_CHECK_INT(t64_machine_bus_write(machine, counters[i], 4u, 0u),
                  T64_STATUS_BUS_DEVICE_REFUSED);
  }
  seed_fill_state_only(machine);
  write_command(machine, UINT32_C(0x100), UINT64_C(0x3600c00800000000));
  (void)seed_transport(machine, UINT32_C(0x100), UINT32_C(0x108));
  while (machine->queue_count != 0u)
    T64_CHECK_INT(dispatch_next_queued_event(machine), T64_STATUS_OK);
  T64_CHECK(machine->rdp.dpc_clock_counter > 0u);
  T64_CHECK(machine->rdp.dpc_buffer_busy_counter > 0u);
  T64_CHECK(machine->rdp.dpc_pipe_busy_counter > 0u);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_STATUS, 4u,
                                      UINT32_C(0x03c0)), T64_STATUS_OK);
  for (i = 0u; i < sizeof counters / sizeof counters[0]; i++) {
    T64_CHECK_INT(t64_machine_bus_read(machine, counters[i], 4u, &value),
                  T64_STATUS_OK);
    T64_CHECK_INT(value, 0u);
  }
  t64_machine_destroy(machine);
}

static void case_fetched_rsp_dp_fill(void) {
  const uint32_t program[] = {
      encode_rsp_cop0(true, 1u, 8u),
      encode_rsp_cop0(true, 2u, 9u),
      0u,
      0u,
      0u,
      encode_rsp_cop0(false, 3u, 10u),
      UINT32_C(0x0000000d)};
  t64_machine *machine = create_test_machine();

  if (machine == NULL) return;
  T64_CHECK(t64_has_rsp_functional_profile(machine));
  seed_fill_state_only(machine);
  write_command(machine, UINT32_C(0x100), UINT64_C(0x3600c00800000000));
  write_command(machine, UINT32_C(0x108), UINT64_C(0xe900000000000000));
  machine->rsp.gpr[1] = UINT32_C(0x100);
  machine->rsp.gpr[2] = UINT32_C(0x110);
  T64_CHECK(install_rsp_program(machine, program,
                                sizeof program / sizeof program[0]));
  T64_CHECK_INT(t64_rsp_start(machine), T64_STATUS_OK);
  T64_CHECK(advance_to_tick(machine, UINT64_C(1)));
  T64_CHECK_INT(machine->rsp_observed_outcome, T64_RSP_ATTEMPT_RETIRED);
  T64_CHECK_INT(machine->rsp_observed_opcode, program[0]);
  T64_CHECK(advance_to_tick(machine, UINT64_C(32)));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT(machine->rdp.dpc_start, UINT32_C(0x100));
  T64_CHECK_INT(machine->rdp.dpc_end, UINT32_C(0x110));
  T64_CHECK_INT(machine->rdp.dpc_current, UINT32_C(0x110));
  T64_CHECK_INT(machine->rdp.command_count, 2u);
  T64_CHECK_INT(machine->rdp.fill_count, 1u);
  T64_CHECK_INT(machine->rdp.fullsync_count, 1u);
  T64_CHECK_INT(machine->mi_pending, UINT32_C(0x20));
  T64_CHECK_INT(machine->rsp.gpr[3], UINT32_C(0x108));
  T64_CHECK_INT(machine->ram[UINT32_C(0x3000)], UINT8_C(0xf8));
  T64_CHECK_INT(machine->ram[UINT32_C(0x3001)], UINT8_C(0x01));
  T64_CHECK_INT(machine->ram[UINT32_C(0x3002)], UINT8_C(0x07));
  T64_CHECK_INT(machine->ram[UINT32_C(0x3003)], UINT8_C(0xc1));
  t64_machine_destroy(machine);
}

static void case_older_profile_dp_refuses(void) {
  const uint32_t program[] = {encode_rsp_cop0(false, 3u, 8u),
                              UINT32_C(0x0000000d)};
  t64_machine *machine = create_rsp_only_test_machine();

  if (machine == NULL) return;
  T64_CHECK(!t64_has_rdp_vi_profile(machine));
  T64_CHECK(t64_has_rsp_functional_profile(machine));
  T64_CHECK(install_rsp_program(machine, program,
                                sizeof program / sizeof program[0]));
  T64_CHECK_INT(t64_rsp_start(machine), T64_STATUS_OK);
  T64_CHECK(advance_to_tick(machine, UINT64_C(1)));
  T64_CHECK_INT(machine->rsp_observed_outcome, T64_RSP_ATTEMPT_REFUSED);
  T64_CHECK_INT(machine->rsp_observed_refusal_reason,
                T64_RSP_REFUSAL_DP_UNAVAILABLE);
  T64_CHECK_INT(machine->rsp.pc, 0u);
  T64_CHECK_INT(machine->rsp.retired_instructions, 0u);
  T64_CHECK_INT(machine->rdp.dpc_start, 0u);
  T64_CHECK_INT(machine->rdp.dpc_end, 0u);
  T64_CHECK_INT(machine->rdp.dpc_current, 0u);
  t64_machine_destroy(machine);
}

static void case_append_end_resume(void) {
  const uint32_t program[] = {
      encode_rsp_cop0(true, 1u, 8u),
      encode_rsp_cop0(true, 2u, 9u),
      0u,
      encode_rsp_cop0(true, 2u, 9u),
      encode_rsp_cop0(false, 3u, 10u),
      UINT32_C(0x0000000d)};
  t64_machine *machine = create_test_machine();

  if (machine == NULL) return;
  seed_fill_state_only(machine);
  write_command(machine, UINT32_C(0x100), UINT64_C(0x3600c00800000000));
  write_command(machine, UINT32_C(0x108), UINT64_C(0xe900000000000000));
  machine->rsp.gpr[1] = UINT32_C(0x100);
  machine->rsp.gpr[2] = UINT32_C(0x110);
  T64_CHECK(install_rsp_program(machine, program,
                                sizeof program / sizeof program[0]));
  T64_CHECK_INT(t64_rsp_start(machine), T64_STATUS_OK);
  T64_CHECK(advance_to_tick(machine, UINT64_C(5)));
  T64_CHECK_INT(machine->rdp.dpc_current, UINT32_C(0x108));
  T64_CHECK(machine->rdp.command_pending);
  T64_CHECK(machine->rdp.busy);
  T64_CHECK(advance_to_tick(machine, UINT64_C(32)));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK_INT(machine->rdp.dpc_end, UINT32_C(0x110));
  T64_CHECK_INT(machine->rdp.dpc_current, UINT32_C(0x110));
  T64_CHECK_INT(machine->rdp.command_count, 2u);
  T64_CHECK_INT(machine->rdp.fill_count, 1u);
  T64_CHECK_INT(machine->rdp.fullsync_count, 1u);
  T64_CHECK_INT(machine->mi_pending, UINT32_C(0x20));
  T64_CHECK_INT(machine->rsp.gpr[3], UINT32_C(0x108));
  t64_machine_destroy(machine);
}

static void case_truncated_packet_reached(void) {
  const uint32_t program[] = {encode_rsp_cop0(true, 1u, 8u),
                              encode_rsp_cop0(true, 2u, 9u), 0u};
  t64_machine *machine = create_test_machine();
  uint64_t rsp_sequence = 0u;
  uint64_t rdp_sequence = 0u;

  if (machine == NULL) return;
  machine->rsp.gpr[1] = UINT32_C(0x100);
  machine->rsp.gpr[2] = UINT32_C(0x108);
  T64_CHECK(install_rsp_program(machine, program,
                                sizeof program / sizeof program[0]));
  T64_CHECK_INT(t64_rsp_start(machine), T64_STATUS_OK);
  T64_CHECK(advance_to_tick(machine, UINT64_C(4)));
  T64_CHECK_INT(machine->rsp_observed_opcode, program[1]);
  T64_CHECK_INT(machine->rsp_observed_outcome, T64_RSP_ATTEMPT_RETIRED);
  T64_CHECK_INT(machine->rdp.dpc_end, UINT32_C(0x108));
  T64_CHECK(queued_sequence(machine, UINT64_C(5), T64_RDP_DEVICE,
                            T64_RDP_EVENT_WORK, &rdp_sequence));
  T64_CHECK(queued_sequence(machine, UINT64_C(5), T64_DEVICE_SP,
                            T64_SP_EVENT_RSP_ISSUE, &rsp_sequence));
  /* A half-command bound cannot be written through the aligned DPC END port;
     inject it after accepted RSP setup to exercise the fetch-time guard. */
  machine->rdp.dpc_end = UINT32_C(0x104);
  T64_CHECK(advance_to_tick(machine, UINT64_C(5)));
  T64_CHECK_INT(machine->rdp.refusal_reason,
                T64_RDP_REFUSAL_TRUNCATED_WORD);
  T64_CHECK_INT(machine->rdp.dpc_current, UINT32_C(0x100));
  T64_CHECK_INT(machine->rdp.command_count, 0u);
  T64_CHECK_INT(machine->rdp.fill_count, 0u);
  T64_CHECK(!machine->rdp.busy);
  T64_CHECK(!machine->rdp.command_pending);
  T64_CHECK(rsp_sequence < rdp_sequence);
  t64_machine_destroy(machine);
}

static void case_unsupported_preserves_prior_work(void) {
  const uint32_t program[] = {encode_rsp_cop0(true, 1u, 8u),
                              encode_rsp_cop0(true, 2u, 9u),
                              UINT32_C(0x0000000d)};
  t64_machine *machine = create_test_machine();

  if (machine == NULL) return;
  seed_fill_state_only(machine);
  write_command(machine, UINT32_C(0x100), UINT64_C(0x3600c00800000000));
  write_command(machine, UINT32_C(0x108), UINT64_C(0x3e00000000000000));
  machine->rsp.gpr[1] = UINT32_C(0x100);
  machine->rsp.gpr[2] = UINT32_C(0x110);
  T64_CHECK(install_rsp_program(machine, program,
                                sizeof program / sizeof program[0]));
  T64_CHECK_INT(t64_rsp_start(machine), T64_STATUS_OK);
  T64_CHECK(advance_to_tick(machine, UINT64_C(32)));
  T64_CHECK_INT(machine->rdp.refusal_reason,
                T64_RDP_REFUSAL_UNSUPPORTED_OPCODE);
  T64_CHECK_INT(machine->rdp.dpc_current, UINT32_C(0x108));
  T64_CHECK_INT(machine->rdp.fill_count, 1u);
  T64_CHECK_INT(machine->rdp.fullsync_count, 0u);
  T64_CHECK_INT(machine->mi_pending, 0u);
  T64_CHECK_INT(machine->ram[UINT32_C(0x3000)], UINT8_C(0xf8));
  T64_CHECK_INT(machine->ram[UINT32_C(0x3001)], UINT8_C(0x01));
  T64_CHECK_INT(machine->ram[UINT32_C(0x3002)], UINT8_C(0x07));
  T64_CHECK_INT(machine->ram[UINT32_C(0x3003)], UINT8_C(0xc1));
  t64_machine_destroy(machine);
}

static void case_same_tick_sequence(void) {
  const uint32_t program[] = {encode_rsp_cop0(true, 1u, 8u),
                              encode_rsp_cop0(true, 2u, 9u), 0u};
  t64_machine *machine = create_test_machine();
  uint64_t rsp_sequence = 0u;
  uint64_t rdp_sequence = 0u;

  if (machine == NULL) return;
  write_command(machine, UINT32_C(0x100), UINT64_C(0xe600000000000000));
  machine->rsp.gpr[1] = UINT32_C(0x100);
  machine->rsp.gpr[2] = UINT32_C(0x108);
  T64_CHECK(install_rsp_program(machine, program,
                                sizeof program / sizeof program[0]));
  T64_CHECK_INT(t64_rsp_start(machine), T64_STATUS_OK);
  T64_CHECK(advance_to_tick(machine, UINT64_C(4)));
  T64_CHECK_INT(machine->rsp_observed_opcode, program[1]);
  T64_CHECK_INT(machine->rsp_observed_outcome, T64_RSP_ATTEMPT_RETIRED);
  T64_CHECK(queued_sequence(machine, UINT64_C(5), T64_DEVICE_SP,
                            T64_SP_EVENT_RSP_ISSUE, &rsp_sequence));
  T64_CHECK(queued_sequence(machine, UINT64_C(5), T64_RDP_DEVICE,
                            T64_RDP_EVENT_WORK, &rdp_sequence));
  T64_CHECK(rsp_sequence < rdp_sequence);
  T64_CHECK(advance_to_tick(machine, UINT64_C(5)));
  T64_CHECK_INT(machine->rsp_observed_opcode, program[2]);
  T64_CHECK_INT(machine->rdp.dpc_current, UINT32_C(0x108));
  T64_CHECK(machine->rdp.command_pending);
  T64_CHECK_INT(machine->rdp.last_command_byte, 0u);
  t64_machine_destroy(machine);
}
#endif

typedef struct {
  const char *name;
  void (*run)(void);
} test_case;

static const test_case cases[] = {
#ifdef T64_TEST_DP_TRANSPORT
    {"current-end-retains-busy-for-pipeline",
     case_current_end_retains_busy_for_pipeline},
    {"fullsync-after-prior-render", case_fullsync_after_prior_render},
    {"dp-pending-mask-clear-independent",
     case_dp_pending_mask_clear_independent},
    {"repeated-fullsync-fences", case_repeated_fullsync_fences},
    {"freeze-flush-controls", case_freeze_flush_controls},
    {"xbus-dmem-bounded", case_xbus_dmem_bounded},
    {"loadsync-raw-26", case_loadsync_raw_26},
    {"scheduler-time-sequence-exhaustion-atomic",
     case_scheduler_time_sequence_exhaustion_atomic},
    {"reset-clears-pending-work", case_reset_clears_pending_work},
    {"counter-registers-read-only-and-clearable",
     case_counter_registers_read_only_and_clearable},
    {"fetched-rsp-dp-fill", case_fetched_rsp_dp_fill},
    {"older-profile-dp-refuses", case_older_profile_dp_refuses},
    {"append-end-resume", case_append_end_resume},
    {"truncated-packet-reached", case_truncated_packet_reached},
    {"unsupported-preserves-prior-work",
     case_unsupported_preserves_prior_work},
    {"same-tick-sequence", case_same_tick_sequence},
#else
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
#endif
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
#ifdef T64_TEST_DP_TRANSPORT
  return T64_TEST_FINISH("unit.core.dp.transport");
#else
  return T64_TEST_FINISH("unit.core.rdp.fill");
#endif
}
