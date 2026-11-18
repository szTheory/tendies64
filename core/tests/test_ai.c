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
#define AI_STATUS_BUSY UINT32_C(0x40000000)
#define AI_STATUS_FULL UINT32_C(0x80000000)
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

static bool configure_ai_rate_pair(t64_machine *machine) {
  t64_status status;

  status = t64_machine_bus_write(machine, AI_DACRATE, 4u, 1103u);
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) return false;
  status = t64_machine_bus_write(machine, AI_BITRATE, 4u, 15u);
  T64_CHECK_INT(status, T64_STATUS_OK);
  return status == T64_STATUS_OK;
}

static bool find_next_ai_sample_tick(const t64_machine *machine,
                                     uint64_t *tick_out) {
  size_t offset;

  if (machine == NULL || tick_out == NULL) return false;
  for (offset = 0u; offset < machine->queue_count; offset++) {
    const size_t index =
        (machine->queue_head + offset) % T64_EVENT_QUEUE_CAPACITY;
    const t64_event *event = &machine->events[index];
    if (event->device == T64_AI_DEVICE &&
        event->kind == T64_AI_EVENT_SAMPLE &&
        event->token == machine->ai.event_token) {
      *tick_out = event->tick;
      return true;
    }
  }
  return false;
}

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

static bool has_ai_sample_event(const t64_machine *machine) {
  uint64_t ignored_tick;

  return find_next_ai_sample_tick(machine, &ignored_tick);
}

static void start_ai_spin_loop(t64_machine *machine) {
  machine->ram[UINT32_C(0x1000)] = 0x08u;
  machine->ram[UINT32_C(0x1001)] = 0x00u;
  machine->ram[UINT32_C(0x1002)] = 0x04u;
  machine->ram[UINT32_C(0x1003)] = 0x00u;
  (void)memset(machine->ram + UINT32_C(0x1004), 0, 4u);
  machine->cpu.pc = UINT32_C(0x80001000);
  machine->cpu.pc_full = t64_cpu_canonical_address(machine->cpu.pc);
}

static bool submit_ai_dma(t64_machine *machine, uint32_t address,
                          uint32_t length) {
  t64_status status;

  status = t64_machine_bus_write(machine, AI_DRAM_ADDR, 4u, address);
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) return false;
  status = t64_machine_bus_write(machine, AI_LEN, 4u, length);
  T64_CHECK_INT(status, T64_STATUS_OK);
  return status == T64_STATUS_OK;
}

static bool advance_ai_sample(t64_machine *machine) {
  uint64_t sample_tick = UINT64_MAX;

  T64_CHECK(find_next_ai_sample_tick(machine, &sample_tick));
  if (!find_next_ai_sample_tick(machine, &sample_tick)) return false;
  return advance_guest_to(machine, sample_tick);
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

static void case_register_fields_and_readback(void) {
  uint8_t ipl3_asset[IPL3_ASSET_BYTES];
  t64_machine *machine = create_admitted_profile(AI_PROFILE_ID, ipl3_asset);
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  uint32_t value = UINT32_MAX;

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_DACRATE, 4u, 1103u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_BITRATE, 4u, 15u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_DACRATE, 4u, &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, 1103u);
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_BITRATE, 4u, &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, 15u);
  value = UINT32_MAX;
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_DRAM_ADDR, 4u, &value),
                T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(value, UINT32_MAX);
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_CONTROL, 4u, &value),
                T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(value, UINT32_MAX);
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_DACRATE, 2u, &value),
                T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_DACRATE + 2u, 4u, &value),
                T64_STATUS_BUS_ALIGNMENT);
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_STATUS + 0x18u, 4u, &value),
                T64_STATUS_BUS_UNSUPPORTED);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_STATUS + 0x18u, 4u, 0u),
                T64_STATUS_BUS_UNSUPPORTED);

  T64_CHECK_INT(t64_machine_bus_write(machine, AI_DACRATE, 4u,
                                      UINT32_C(0xffff044f)),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_DACRATE, 4u, &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, 1103u);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_BITRATE, 4u,
                                      UINT32_C(0xffff000f)),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_BITRATE, 4u, &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, 15u);
  T64_CHECK_INT(t64_machine_digest(machine, digest_before), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_DACRATE, 4u, 1054u),
                T64_STATUS_BUS_DEVICE_REFUSED);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) == 0);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_DACRATE, 4u, 1055u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_DACRATE, 4u, &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, 1055u);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_DACRATE, 4u, 1103u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(machine, digest_before), T64_STATUS_OK);

  /* A zero BITRATE write is refused even though reset starts at zero. */
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_BITRATE, 4u, 0u),
                T64_STATUS_BUS_DEVICE_REFUSED);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) == 0);
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_BITRATE, 4u, &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, 15u);
  t64_machine_destroy(machine);
}

static void case_address_before_length(void) {
  uint8_t ipl3_asset[IPL3_ASSET_BYTES];
  t64_machine *machine = create_admitted_profile(AI_PROFILE_ID, ipl3_asset);
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  if (!configure_ai_rate_pair(machine)) {
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK_INT(t64_machine_digest(machine, digest_before), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_LEN, 4u, 8u),
                T64_STATUS_BUS_DEVICE_REFUSED);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) == 0);
  T64_CHECK(!machine->ai.address_staged);
  T64_CHECK_INT(machine->ai.descriptor_count, 0u);
  T64_CHECK(!has_ai_sample_event(machine));

  /* High address bits and the ignored low three bits are masked. */
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_DRAM_ADDR, 4u,
                                      UINT32_C(0xff000103)),
                T64_STATUS_OK);
  T64_CHECK(machine->ai.address_staged);
  T64_CHECK_INT(machine->ai.staged_address, UINT32_C(0x100));
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_LEN, 4u,
                                      UINT32_C(0x00040000)),
                T64_STATUS_OK);
  T64_CHECK(machine->ai.address_staged);
  T64_CHECK_INT(machine->ai.descriptor_count, 0u);
  T64_CHECK(!has_ai_sample_event(machine));
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_LEN, 4u, 8u),
                T64_STATUS_OK);
  T64_CHECK(!machine->ai.address_staged);
  T64_CHECK_INT(machine->ai.descriptor_count, 1u);
  T64_CHECK(!machine->ai.active);
  T64_CHECK(!has_ai_sample_event(machine));
  t64_machine_destroy(machine);
}

static void case_length_zero_and_maximum(void) {
  uint8_t ipl3_asset[IPL3_ASSET_BYTES];
  t64_machine *machine = create_admitted_profile(AI_PROFILE_ID, ipl3_asset);
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  if (!configure_ai_rate_pair(machine)) {
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK_INT(t64_machine_digest(machine, digest_before), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_LEN, 4u, 0u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) == 0);
  T64_CHECK_INT(machine->ai.descriptor_count, 0u);
  T64_CHECK(!machine->ai.address_staged);
  T64_CHECK(!has_ai_sample_event(machine));

  T64_CHECK_INT(t64_machine_bus_write(machine, AI_DRAM_ADDR, 4u,
                                      UINT32_C(0x200)),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(machine, digest_before), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_LEN, 4u,
                                      UINT32_C(0x00040000)),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) == 0);
  T64_CHECK(machine->ai.address_staged);
  T64_CHECK_INT(machine->ai.descriptor_count, 0u);
  T64_CHECK(!has_ai_sample_event(machine));

  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_LEN, 4u,
                                      UINT32_C(0x0003fff8)),
                T64_STATUS_OK);
  T64_CHECK(!machine->ai.address_staged);
  T64_CHECK_INT(machine->ai.descriptor_count, 1u);
  T64_CHECK_INT(machine->ai.descriptors[machine->ai.descriptor_head].length,
                UINT32_C(0x0003fff8));
  T64_CHECK(has_ai_sample_event(machine));
  start_ai_spin_loop(machine);
  T64_CHECK(advance_guest_to(machine, machine->tick + UINT64_C(1)));
  T64_CHECK_INT(machine->ai.active_byte_cursor, 0u);
  T64_CHECK_INT(machine->ai.sample_index, 0u);
  t64_machine_destroy(machine);
}

static void case_rdram_span_endpoints(void) {
  uint8_t first_asset[IPL3_ASSET_BYTES];
  uint8_t second_asset[IPL3_ASSET_BYTES];
  t64_machine *machine =
      create_admitted_profile(AI_PROFILE_ID, first_asset);
  t64_machine *overrun_machine;
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  t64_ai_state ai_before;

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  if (!configure_ai_rate_pair(machine)) {
    t64_machine_destroy(machine);
    return;
  }
  start_ai_spin_loop(machine);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_OK);
  T64_CHECK(submit_ai_dma(machine, 0u, 8u));
  T64_CHECK(submit_ai_dma(machine, UINT32_C(0x003ffff8), 8u));
  T64_CHECK_INT(machine->ai.descriptor_count, T64_AUDIO_QUEUE_CAPACITY);
  ai_before = machine->ai;
  T64_CHECK_INT(t64_machine_digest(machine, digest_before), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_DRAM_ADDR, 4u,
                                      UINT32_C(0x00400000)),
                T64_STATUS_BUS_DEVICE_REFUSED);
  T64_CHECK(memcmp(&machine->ai, &ai_before, sizeof ai_before) == 0);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) == 0);
  t64_machine_destroy(machine);

  overrun_machine = create_admitted_profile(AI_PROFILE_ID, second_asset);
  T64_CHECK(overrun_machine != NULL);
  if (overrun_machine == NULL) return;
  if (!configure_ai_rate_pair(overrun_machine)) {
    t64_machine_destroy(overrun_machine);
    return;
  }
  T64_CHECK_INT(t64_machine_bus_write(overrun_machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(overrun_machine, AI_DRAM_ADDR, 4u,
                                      UINT32_C(0x003ffff8)),
                T64_STATUS_OK);
  ai_before = overrun_machine->ai;
  T64_CHECK_INT(t64_machine_digest(overrun_machine, digest_before),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(overrun_machine, AI_LEN, 4u, 16u),
                T64_STATUS_BUS_DEVICE_REFUSED);
  T64_CHECK(memcmp(&overrun_machine->ai, &ai_before, sizeof ai_before) == 0);
  T64_CHECK_INT(t64_machine_digest(overrun_machine, digest_after),
                T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) == 0);
  T64_CHECK(overrun_machine->ai.address_staged);
  t64_machine_destroy(overrun_machine);
}

static void case_dma_two_entry_full(void) {
  uint8_t ipl3_asset[IPL3_ASSET_BYTES];
  t64_machine *machine = create_admitted_profile(AI_PROFILE_ID, ipl3_asset);
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  t64_ai_state ai_before;
  t64_event events_before[T64_EVENT_QUEUE_CAPACITY];
  uint32_t pending_before;
  uint32_t status = 0u;
  size_t queue_count_before;
  uint64_t next_sequence_before;

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  if (!configure_ai_rate_pair(machine)) {
    t64_machine_destroy(machine);
    return;
  }
  start_ai_spin_loop(machine);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_OK);
  T64_CHECK(submit_ai_dma(machine, UINT32_C(0x100), 8u));
  T64_CHECK(submit_ai_dma(machine, UINT32_C(0x110), 8u));
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_STATUS, 4u, &status),
                T64_STATUS_OK);
  T64_CHECK_INT(status, UINT32_C(0xc0000000));
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_DRAM_ADDR, 4u,
                                      UINT32_C(0x120)),
                T64_STATUS_OK);
  ai_before = machine->ai;
  (void)memcpy(events_before, machine->events, sizeof events_before);
  pending_before = machine->mi_pending;
  queue_count_before = machine->queue_count;
  next_sequence_before = machine->next_sequence;
  T64_CHECK_INT(t64_machine_digest(machine, digest_before), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_LEN, 4u, 8u),
                T64_STATUS_QUEUE_FULL);
  T64_CHECK(memcmp(&machine->ai, &ai_before, sizeof ai_before) == 0);
  T64_CHECK(memcmp(events_before, machine->events, sizeof events_before) == 0);
  T64_CHECK_INT(machine->mi_pending, pending_before);
  T64_CHECK_INT(machine->queue_count, queue_count_before);
  T64_CHECK_INT(machine->next_sequence, next_sequence_before);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) == 0);
  T64_CHECK(machine->ai.address_staged);
  T64_CHECK_INT(machine->ai.staged_address, UINT32_C(0x120));
  t64_machine_destroy(machine);
}

static void case_mi_full_falling_edge(void) {
  uint8_t ipl3_asset[IPL3_ASSET_BYTES];
  t64_machine *machine = create_admitted_profile(AI_PROFILE_ID, ipl3_asset);
  uint32_t value = 0u;

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  if (!configure_ai_rate_pair(machine)) {
    t64_machine_destroy(machine);
    return;
  }
  start_ai_spin_loop(machine);
  T64_CHECK_INT(t64_machine_bus_write(machine, UINT32_C(0x0430000c), 4u,
                                      UINT32_C(0x20)),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_OK);
  T64_CHECK(submit_ai_dma(machine, UINT32_C(0x100), 8u));
  T64_CHECK(submit_ai_dma(machine, UINT32_C(0x110), 8u));
  T64_CHECK_INT(machine->mi_pending & T64_MI_INTERRUPT_AI, 0u);
  T64_CHECK(advance_ai_sample(machine));
  T64_CHECK_INT(machine->ai.descriptor_count, T64_AUDIO_QUEUE_CAPACITY);
  T64_CHECK_INT(machine->mi_pending & T64_MI_INTERRUPT_AI, 0u);
  T64_CHECK(advance_ai_sample(machine));
  T64_CHECK_INT(machine->ai.descriptor_count, 1u);
  T64_CHECK_INT(machine->mi_pending & T64_MI_INTERRUPT_AI,
                T64_MI_INTERRUPT_AI);
  T64_CHECK(machine->mi_irq_line);
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_STATUS, 4u, &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, AI_STATUS_BUSY);
  T64_CHECK(advance_ai_sample(machine));
  T64_CHECK(advance_ai_sample(machine));
  T64_CHECK_INT(machine->ai.descriptor_count, 0u);
  T64_CHECK_INT(machine->mi_pending & T64_MI_INTERRUPT_AI,
                T64_MI_INTERRUPT_AI);
  t64_machine_destroy(machine);
}

static void case_status_ack_isolation(void) {
  uint8_t ipl3_asset[IPL3_ASSET_BYTES];
  t64_machine *machine = create_admitted_profile(AI_PROFILE_ID, ipl3_asset);
  uint32_t value = 0u;

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  t64_mi_set_pending(machine, UINT32_C(0x34));
  T64_CHECK_INT(machine->mi_pending, UINT32_C(0x34));
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_STATUS, 4u,
                                      UINT32_C(0xdeadbeef)),
                T64_STATUS_OK);
  T64_CHECK_INT(machine->mi_pending, UINT32_C(0x30));
  T64_CHECK_INT(t64_machine_bus_read(machine, UINT32_C(0x04300008), 4u,
                                    &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, UINT32_C(0x30));
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_STATUS, 4u, 0u),
                T64_STATUS_OK);
  T64_CHECK_INT(machine->mi_pending, UINT32_C(0x30));
  t64_machine_destroy(machine);
}

static void case_control_disable_enable(void) {
  uint8_t ipl3_asset[IPL3_ASSET_BYTES];
  t64_machine *machine = create_admitted_profile(AI_PROFILE_ID, ipl3_asset);
  uint32_t remaining = 0u;
  uint64_t next_tick = UINT64_MAX;
  uint64_t remaining_ticks;
  uint64_t resumed_tick = UINT64_MAX;
  uint64_t expected_resume_tick;
  size_t before_cursor;
  bool found_event;

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  if (!configure_ai_rate_pair(machine)) {
    t64_machine_destroy(machine);
    return;
  }
  start_ai_spin_loop(machine);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_OK);
  T64_CHECK(submit_ai_dma(machine, UINT32_C(0x100), 16u));
  T64_CHECK(advance_ai_sample(machine));
  T64_CHECK_INT(machine->ai.sample_index, 1u);
  T64_CHECK_INT(machine->ai.active_byte_cursor, 4u);
  found_event = find_next_ai_sample_tick(machine, &next_tick);
  T64_CHECK(found_event);
  if (!found_event) {
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK(next_tick > machine->tick);
  if (next_tick <= machine->tick) {
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK(advance_guest_to(machine, next_tick - UINT64_C(1)));
  remaining_ticks = next_tick - machine->tick;
  before_cursor = machine->ai.active_byte_cursor;
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_LEN, 4u, &remaining),
                T64_STATUS_OK);
  T64_CHECK_INT(remaining, 12u);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 0u),
                T64_STATUS_OK);
  T64_CHECK(!has_ai_sample_event(machine));
  if (has_ai_sample_event(machine)) {
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 0u),
                T64_STATUS_OK);
  T64_CHECK(advance_guest_to(machine, machine->tick + UINT64_C(8)));
  T64_CHECK_INT(machine->ai.sample_index, 1u);
  T64_CHECK_INT(machine->ai.active_byte_cursor, before_cursor);
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_LEN, 4u, &remaining),
                T64_STATUS_OK);
  T64_CHECK_INT(remaining, 12u);
  expected_resume_tick = machine->tick + remaining_ticks;
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_OK);
  found_event = find_next_ai_sample_tick(machine, &resumed_tick);
  T64_CHECK(found_event);
  if (!found_event) {
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK_INT(resumed_tick, expected_resume_tick);
  T64_CHECK(advance_guest_to(machine, resumed_tick - UINT64_C(1)));
  T64_CHECK_INT(machine->ai.sample_index, 1u);
  T64_CHECK(advance_guest_to(machine, resumed_tick));
  T64_CHECK_INT(machine->ai.sample_index, 2u);
  T64_CHECK_INT(machine->ai.active_byte_cursor, 8u);
  t64_machine_destroy(machine);
}

static bool assert_boundary_audio(uint32_t address, uint32_t length,
                                  const uint8_t *expected) {
  uint8_t ipl3_asset[IPL3_ASSET_BYTES];
  uint8_t actual[16];
  t64_machine *machine = create_admitted_profile(AI_PROFILE_ID, ipl3_asset);
  t64_audio_packet_info info;
  size_t copied = 0u;
  uint32_t attempts = 0u;

  T64_CHECK(machine != NULL);
  if (machine == NULL) return false;
  if (!configure_ai_rate_pair(machine)) {
    t64_machine_destroy(machine);
    return false;
  }
  start_ai_spin_loop(machine);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_OK);
  (void)memcpy(machine->ram + address, expected, length);
  T64_CHECK(submit_ai_dma(machine, address, length));
  while (machine->ai.descriptor_count > 0u && attempts < 8u) {
    if (!advance_ai_sample(machine)) break;
    attempts++;
  }
  T64_CHECK_INT(machine->ai.descriptor_count, 0u);
  T64_CHECK_INT(t64_audio_info(machine, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.frame_count, length / 4u);
  T64_CHECK_INT(info.byte_length, length);
  T64_CHECK_INT(t64_audio_copy(machine, actual, sizeof actual, &copied),
                T64_STATUS_OK);
  T64_CHECK_INT(copied, length);
  T64_CHECK(memcmp(actual, expected, length) == 0);
  t64_machine_destroy(machine);
  return true;
}

static void case_documented_boundary_policy(void) {
  const uint8_t before[8] = {0x01u, 0x23u, 0x45u, 0x67u,
                             0x89u, 0xabu, 0xcdu, 0xefu};
  const uint8_t exact_boundary[8] = {0x10u, 0x32u, 0x54u, 0x76u,
                                     0x98u, 0xbau, 0xdcu, 0xfeu};
  const uint8_t generic_crossing[16] = {
      0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u, 0x88u,
      0x99u, 0xaau, 0xbbu, 0xccu, 0xddu, 0xeeu, 0xf0u, 0x0fu};

  /* The second span ends at 0x2000, matching the SDK's narrow caveat. */
  T64_CHECK(assert_boundary_audio(UINT32_C(0x1ff0), 8u, before));
  T64_CHECK(assert_boundary_audio(UINT32_C(0x1ff8), 8u, exact_boundary));
  /* This crosses 8 KiB but does not end at the cited 16 KiB sub-boundary. */
  T64_CHECK(assert_boundary_audio(UINT32_C(0x1ff8), 16u, generic_crossing));
}

static void case_register_suite(void) {
  puts("case register-fields-and-readback");
  case_register_fields_and_readback();
  puts("case address-before-length");
  case_address_before_length();
  puts("case length-zero-and-maximum");
  case_length_zero_and_maximum();
  puts("case rdram-span-endpoints");
  case_rdram_span_endpoints();
  puts("case dma-two-entry-full");
  case_dma_two_entry_full();
  puts("case mi-full-falling-edge");
  case_mi_full_falling_edge();
  puts("case status-ack-isolation");
  case_status_ack_isolation();
  puts("case control-disable-enable");
  case_control_disable_enable();
  puts("case documented-boundary-policy");
  case_documented_boundary_policy();
}

int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    puts("cpu-two-dma-literal-pcm");
    puts("new-profile-inherited-devices");
    puts("old-profile-ai-refusals");
    puts("register-fields-and-readback");
    puts("address-before-length");
    puts("length-zero-and-maximum");
    puts("rdram-span-endpoints");
    puts("dma-two-entry-full");
    puts("mi-full-falling-edge");
    puts("status-ack-isolation");
    puts("control-disable-enable");
    puts("documented-boundary-policy");
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--suite") == 0 &&
      strcmp(argv[2], "registers") == 0) {
    case_register_suite();
    return T64_TEST_FINISH("unit.core.ai.registers");
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
    if (strcmp(argv[2], "register-fields-and-readback") == 0) {
      case_register_fields_and_readback();
      return T64_TEST_FINISH("unit.core.ai.registers");
    }
    if (strcmp(argv[2], "address-before-length") == 0) {
      case_address_before_length();
      return T64_TEST_FINISH("unit.core.ai.registers");
    }
    if (strcmp(argv[2], "length-zero-and-maximum") == 0) {
      case_length_zero_and_maximum();
      return T64_TEST_FINISH("unit.core.ai.registers");
    }
    if (strcmp(argv[2], "rdram-span-endpoints") == 0) {
      case_rdram_span_endpoints();
      return T64_TEST_FINISH("unit.core.ai.registers");
    }
    if (strcmp(argv[2], "dma-two-entry-full") == 0) {
      case_dma_two_entry_full();
      return T64_TEST_FINISH("unit.core.ai.registers");
    }
    if (strcmp(argv[2], "mi-full-falling-edge") == 0) {
      case_mi_full_falling_edge();
      return T64_TEST_FINISH("unit.core.ai.registers");
    }
    if (strcmp(argv[2], "status-ack-isolation") == 0) {
      case_status_ack_isolation();
      return T64_TEST_FINISH("unit.core.ai.registers");
    }
    if (strcmp(argv[2], "control-disable-enable") == 0) {
      case_control_disable_enable();
      return T64_TEST_FINISH("unit.core.ai.registers");
    }
    if (strcmp(argv[2], "documented-boundary-policy") == 0) {
      case_documented_boundary_policy();
      return T64_TEST_FINISH("unit.core.ai.registers");
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
  case_register_suite();
  return T64_TEST_FINISH("unit.core.ai.tracer");
}
