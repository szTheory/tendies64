#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_test.h"
#include "../t64_internal.h"
#include "tendies64/t64_machine.h"

#define AI_PROFILE_ID \
  "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1"

#define AI_DRAM_ADDR UINT32_C(0x04500000)
#define AI_LEN UINT32_C(0x04500004)
#define AI_CONTROL UINT32_C(0x04500008)
#define AI_STATUS UINT32_C(0x0450000c)
#define AI_DACRATE UINT32_C(0x04500010)
#define AI_BITRATE UINT32_C(0x04500014)
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

static const t64_host_allocator allocator = {
    test_allocate, test_release, NULL};

static t64_machine *create_admitted_profile(const char *profile_id,
                                             uint8_t ipl3_asset[IPL3_ASSET_BYTES]) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  T64_CHECK_INT(t64_profile_lookup(profile_id, &profile), T64_STATUS_OK);
  if (profile == NULL) return NULL;
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  if (machine == NULL) return NULL;
  if (!read_openipl3_asset(ipl3_asset)) {
    T64_CHECK(false);
    t64_machine_destroy(machine);
    return NULL;
  }
  T64_CHECK_INT(t64_machine_admit_rom(machine, ipl3_asset,
                                      IPL3_ASSET_BYTES), T64_STATUS_OK);
  return machine;
}

static void case_admitted_profile_dma_queue(void) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  t64_stop_result stop = {0};
  t64_audio_packet_info info;
  t64_stop_request request = {T64_STOP_GUEST_TICK, 0u};
  uint8_t first_packet[16];
  uint8_t second_packet[16];
  uint8_t ipl3_asset[IPL3_ASSET_BYTES];
  uint64_t first_ai_tick = UINT64_MAX;
  size_t copied = 0u;
  size_t i;
  uint32_t value = 0u;
  const uint8_t literal_pcm[32] = {
      0x00u, 0x00u, 0x7fu, 0xffu, 0x80u, 0x00u, 0xffu, 0xffu,
      0x12u, 0x34u, 0xedu, 0xccu, 0x00u, 0x01u, 0xffu, 0xfeu,
      0x80u, 0x00u, 0x7fu, 0xffu, 0xffu, 0xffu, 0x00u, 0x00u,
      0x55u, 0xaau, 0xaau, 0x55u, 0x00u, 0x00u, 0x80u, 0x00u};
  t64_status status = t64_profile_lookup(AI_PROFILE_ID, &profile);

  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK(profile != NULL);
  if (profile == NULL) return;
  T64_CHECK_STR(profile->profile_id, AI_PROFILE_ID);
  T64_CHECK_INT(profile->descriptor_version, 11u);
  T64_CHECK_INT(profile->machine_model_version, 19u);
  T64_CHECK_INT(profile->state_serialization_version, 24u);
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;

  if (!read_openipl3_asset(ipl3_asset)) {
    T64_CHECK(false);
    t64_machine_destroy(machine);
    return;
  }
  status = t64_machine_admit_rom(machine, ipl3_asset, sizeof ipl3_asset);
  if (status != T64_STATUS_OK) {
    T64_CHECK_INT(status, T64_STATUS_OK);
    t64_machine_destroy(machine);
    return;
  }
  (void)memcpy(machine->ram + UINT32_C(0x100), literal_pcm,
               sizeof literal_pcm);
  value = UINT32_MAX;
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_DRAM_ADDR, 4u, &value),
                T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(value, UINT32_MAX);
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_CONTROL, 4u, &value),
                T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(value, UINT32_MAX);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_OK);
  status = t64_machine_bus_write(machine, AI_DACRATE, 4u, 1103u);
  if (status != T64_STATUS_OK) {
    T64_CHECK_INT(status, T64_STATUS_OK);
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_BITRATE, 4u, 15u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_DRAM_ADDR, 4u, 0x100u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_LEN, 4u, 16u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_DRAM_ADDR, 4u, 0x110u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_LEN, 4u, 16u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_STATUS, 4u, &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, UINT32_C(0xc0000000));
  for (i = 0u; i < machine->queue_count; i++) {
    const size_t index = (machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY;
    const t64_event *event = &machine->events[index];
    if (event->device == T64_AI_DEVICE &&
        event->kind == T64_AI_EVENT_SAMPLE) {
      first_ai_tick = event->tick;
      break;
    }
  }
  machine->ram[UINT32_C(0x1000)] = 0x08u;
  machine->ram[UINT32_C(0x1001)] = 0x00u;
  machine->ram[UINT32_C(0x1002)] = 0x04u;
  machine->ram[UINT32_C(0x1003)] = 0x00u;
  (void)memset(machine->ram + UINT32_C(0x1004), 0, 4u);
  machine->cpu.pc = UINT32_C(0x80001000);
  machine->cpu.pc_full = t64_cpu_canonical_address(machine->cpu.pc);
  do {
    request.value = machine->tick + UINT64_C(3000);
    if (request.value > UINT64_C(12000)) request.value = UINT64_C(12000);
    status = t64_machine_advance(machine, &request, &stop);
    T64_CHECK_INT(status, T64_STATUS_OK);
    if (status != T64_STATUS_OK || stop.reason != T64_STOP_REASON_BOUND) break;
  } while (machine->tick < UINT64_C(12000) &&
           machine->ai.descriptor_count > 0u);
  T64_CHECK(first_ai_tick != UINT64_MAX);
  T64_CHECK(machine->tick >= first_ai_tick);
  T64_CHECK_INT(machine->ai.sample_index, 8u);
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_STATUS, 4u, &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, 0u);
  if (value != 0u) {
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04300008), 4u,
                                    &value),
                T64_STATUS_OK);
  T64_CHECK((value & T64_MI_INTERRUPT_AI) != 0u);
  T64_CHECK_INT(t64_audio_info(machine, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.frame_count, 4u);
  T64_CHECK_INT(info.byte_length, sizeof first_packet);
  T64_CHECK_INT(t64_audio_copy(machine, first_packet, sizeof first_packet,
                               &copied),
                T64_STATUS_OK);
  T64_CHECK_INT(copied, sizeof first_packet);
  T64_CHECK(memcmp(first_packet, literal_pcm, sizeof first_packet) == 0);
  T64_CHECK_INT(t64_audio_copy(machine, second_packet, sizeof second_packet,
                               &copied),
                T64_STATUS_OK);
  T64_CHECK_INT(copied, sizeof second_packet);
  T64_CHECK(memcmp(second_packet, literal_pcm + sizeof first_packet,
                   sizeof second_packet) == 0);
  t64_machine_destroy(machine);
}

static void case_new_profile_inherited_devices(void) {
  uint8_t ipl3_asset[IPL3_ASSET_BYTES];
  t64_machine *machine =
      create_admitted_profile(AI_PROFILE_ID, ipl3_asset);
  t64_stop_request request;
  t64_stop_result stop = {0};
  uint32_t instruction;
  size_t index;

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;

  /* Execute an actual VR4300 instruction from RDRAM under the new profile. */
  machine->ram[UINT32_C(0x1000)] = 0x24u;
  machine->ram[UINT32_C(0x1001)] = 0x08u;
  machine->ram[UINT32_C(0x1002)] = 0x00u;
  machine->ram[UINT32_C(0x1003)] = 0x07u;
  machine->cpu.pc = UINT32_C(0x80001000);
  machine->cpu.pc_full = t64_cpu_canonical_address(machine->cpu.pc);
  request = (t64_stop_request){T64_STOP_GUEST_TICK, machine->tick + 1u};
  T64_CHECK_INT(t64_machine_advance(machine, &request, &stop), T64_STATUS_OK);
  T64_CHECK_INT(machine->cpu.gpr[8], 7u);
  T64_CHECK(machine->cpu.retired_instructions > 0u);

  /* Fetch and retire one RSP scalar instruction from IMEM. */
  instruction = UINT32_C(0x24020009); /* addiu v0, zero, 9 */
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x04001000), 4u,
                                      instruction), T64_STATUS_OK);
  T64_CHECK_INT(t64_rsp_start(machine), T64_STATUS_OK);
  request.value = machine->tick + 1u;
  T64_CHECK_INT(t64_machine_advance(machine, &request, &stop), T64_STATUS_OK);
  T64_CHECK(machine->rsp.retired_instructions > 0u);
  T64_CHECK_INT(machine->rsp.gpr[2], 9u);

  /* Complete a real PI ROM-to-RDRAM DMA under the new profile. */
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x04600000), 4u,
                                      UINT32_C(0x200)), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x04600004), 4u,
                                      UINT32_C(0x10000000)), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x0460000c), 4u,
                                      7u), T64_STATUS_OK);
  request.value = machine->tick + 16u;
  T64_CHECK_INT(t64_machine_advance(machine, &request, &stop), T64_STATUS_OK);
  for (index = 0u; index < 8u; index++) {
    T64_CHECK_INT(machine->ram[UINT32_C(0x200) + index], ipl3_asset[index]);
  }
  t64_machine_destroy(machine);
}

static void case_old_profile_ai_refusals(void) {
  const char *const old_profile =
      "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9";
  uint8_t ipl3_asset[IPL3_ASSET_BYTES];
  t64_machine *machine = create_admitted_profile(old_profile, ipl3_asset);
  uint32_t value = UINT32_MAX;
  uint32_t pending_before;
  uint32_t mask_before;
  size_t queue_before;
  uint64_t sequence_before;
  uint64_t tick_before;
  t64_ai_state ai_before;

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  T64_CHECK_INT(machine->profile.descriptor_version, 10u);
  T64_CHECK_INT(machine->profile.machine_model_version, 18u);
  T64_CHECK_INT(machine->profile.state_serialization_version, 23u);
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_STATUS, 4u, &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, 0u);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 0u),
                T64_STATUS_OK);
  t64_mi_set_pending(machine, UINT32_C(0x24));
  pending_before = machine->mi_pending;
  mask_before = machine->mi_mask;
  queue_before = machine->queue_count;
  sequence_before = machine->next_sequence;
  tick_before = machine->tick;
  ai_before = machine->ai;

  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_BUS_DEVICE_REFUSED);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_STATUS, 4u, 1u),
                T64_STATUS_BUS_DEVICE_REFUSED);
  T64_CHECK(memcmp(&machine->ai, &ai_before, sizeof ai_before) == 0);
  T64_CHECK_INT(machine->mi_pending, pending_before);
  T64_CHECK_INT(machine->mi_mask, mask_before);
  T64_CHECK_INT(machine->queue_count, queue_before);
  T64_CHECK_INT(machine->next_sequence, sequence_before);
  T64_CHECK_INT(machine->tick, tick_before);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_STATUS, 4u, 0u),
                T64_STATUS_OK);
  T64_CHECK_INT(machine->mi_pending, UINT32_C(0x20));
  t64_machine_destroy(machine);
}

int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    puts("cpu-two-dma-literal-pcm");
    puts("new-profile-inherited-devices");
    puts("old-profile-ai-refusals");
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    if (strcmp(argv[2], "cpu-two-dma-literal-pcm") == 0) {
      case_admitted_profile_dma_queue();
      return T64_TEST_FINISH("unit.core.ai.tracer");
    }
    if (strcmp(argv[2], "new-profile-inherited-devices") == 0) {
      case_new_profile_inherited_devices();
      return T64_TEST_FINISH("unit.core.ai.profile");
    }
    if (strcmp(argv[2], "old-profile-ai-refusals") == 0) {
      case_old_profile_ai_refusals();
      return T64_TEST_FINISH("unit.core.ai.old-profile-ai-refusals");
    }
    fprintf(stderr, "unknown AI case: %s\n", argv[2]);
    return 2;
  }
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--list-cases|--case NAME]\n", argv[0]);
    return 2;
  }
  case_admitted_profile_dma_queue();
  case_new_profile_inherited_devices();
  case_old_profile_ai_refusals();
  return T64_TEST_FINISH("unit.core.ai.tracer");
}
