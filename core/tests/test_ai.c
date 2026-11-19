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

static bool cancel_boot_cpu_event(t64_machine *machine);

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
  uint32_t value = UINT32_MAX;
  t64_status status;

  T64_CHECK_INT(UINT32_C(1103) & UINT32_C(0x3fff), UINT32_C(1103));
  T64_CHECK_INT(UINT32_C(15) & UINT32_C(0x000f), UINT32_C(15));
  status = t64_machine_bus_write(machine, AI_DACRATE, 4u, 1103u);
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) return false;
  status = t64_machine_bus_write(machine, AI_BITRATE, 4u, 15u);
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) return false;
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_DACRATE, 4u, &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, 1103u);
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_BITRATE, 4u, &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, 15u);
  return true;
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

static bool advance_guest_to_in_slices(t64_machine *machine, uint64_t tick) {
  while (machine->tick < tick) {
    uint64_t next_tick = machine->tick + UINT64_C(2048);
    if (next_tick < machine->tick || next_tick > tick) next_tick = tick;
    if (!advance_guest_to(machine, next_tick)) return false;
  }
  return machine->tick == tick;
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

static bool submit_ai_frames(t64_machine *machine, uint32_t address,
                             uint32_t frame_count) {
  uint64_t wide_length;
  uint32_t length;

  if (frame_count == 0u || frame_count > UINT32_MAX / UINT32_C(4)) {
    T64_CHECK(false);
    return false;
  }
  wide_length = (uint64_t)frame_count * UINT64_C(4);
  if (wide_length > UINT32_MAX) {
    T64_CHECK(false);
    return false;
  }
  length = (uint32_t)wide_length;
  T64_CHECK_INT(length & UINT32_C(0x0003fff8), length);
  T64_CHECK_INT(length & UINT32_C(7), 0u);
  if ((length & UINT32_C(0x0003fff8)) != length ||
      (length & UINT32_C(7)) != 0u) {
    return false;
  }
  return submit_ai_dma(machine, address, length);
}

static bool advance_ai_sample(t64_machine *machine) {
  uint64_t sample_tick = UINT64_MAX;

  T64_CHECK(find_next_ai_sample_tick(machine, &sample_tick));
  if (!find_next_ai_sample_tick(machine, &sample_tick)) return false;
  return advance_guest_to(machine, sample_tick);
}

static uint64_t expected_audio_elapsed_ticks(uint64_t samples,
                                             uint32_t dacrate) {
  const uint64_t numerator = UINT64_C(62500000) * samples *
                             ((uint64_t)dacrate + UINT64_C(1));
  const uint64_t source_clock = UINT64_C(48681812);

  T64_CHECK_INT(dacrate & UINT32_C(0x3fff), dacrate);
  return numerator / source_clock +
         (numerator % source_clock == 0u ? 0u : 1u);
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
  /* Acknowledge the 2 -> 1 interrupt, then prove that completing the last
   * single descriptor (1 -> 0) neither raises nor re-raises MI AI. */
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_STATUS, 4u, 0u),
                T64_STATUS_OK);
  T64_CHECK_INT(machine->mi_pending & T64_MI_INTERRUPT_AI, 0u);
  T64_CHECK(advance_ai_sample(machine));
  T64_CHECK_INT(machine->ai.descriptor_count, 1u);
  T64_CHECK_INT(machine->mi_pending & T64_MI_INTERRUPT_AI, 0u);
  T64_CHECK(advance_ai_sample(machine));
  T64_CHECK_INT(machine->ai.descriptor_count, 0u);
  T64_CHECK_INT(machine->mi_pending & T64_MI_INTERRUPT_AI, 0u);
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

static void run_rational_boundary_case(uint32_t dacrate) {
  uint8_t ipl3_asset[IPL3_ASSET_BYTES];
  t64_machine *machine = create_admitted_profile(AI_PROFILE_ID, ipl3_asset);
  t64_audio_packet_info info;
  uint64_t origin_tick;
  uint64_t first_tick;
  uint64_t second_tick;

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  if (!configure_ai_rate_pair(machine)) {
    t64_machine_destroy(machine);
    return;
  }
  if (dacrate != 1103u) {
    T64_CHECK_INT(t64_machine_bus_write(machine, AI_DACRATE, 4u, dacrate),
                  T64_STATUS_OK);
  }
  origin_tick = machine->tick;
  start_ai_spin_loop(machine);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_OK);
  T64_CHECK(submit_ai_frames(machine, UINT32_C(0x100), 2u));
  first_tick = origin_tick + expected_audio_elapsed_ticks(1u, dacrate);
  second_tick = origin_tick + expected_audio_elapsed_ticks(2u, dacrate);

  T64_CHECK(advance_guest_to_in_slices(machine, first_tick - 1u));
  T64_CHECK_INT(machine->ai.sample_index, 0u);
  T64_CHECK(advance_guest_to_in_slices(machine, first_tick));
  T64_CHECK_INT(machine->ai.sample_index, 1u);
  T64_CHECK(advance_guest_to_in_slices(machine, first_tick + 1u));
  T64_CHECK_INT(machine->ai.sample_index, 1u);
  T64_CHECK(advance_guest_to_in_slices(machine, second_tick - 1u));
  T64_CHECK_INT(machine->ai.sample_index, 1u);
  T64_CHECK(advance_guest_to_in_slices(machine, second_tick));
  T64_CHECK_INT(machine->ai.sample_index, 2u);
  T64_CHECK(advance_guest_to_in_slices(machine, second_tick + 1u));
  T64_CHECK_INT(machine->ai.sample_index, 2u);
  T64_CHECK_INT(t64_audio_info(machine, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.first_tick, first_tick);
  T64_CHECK_INT(info.last_tick, second_tick);
  T64_CHECK_INT(info.rate_denominator, (uint64_t)dacrate + UINT64_C(1));
  t64_machine_destroy(machine);
}

static void case_rational_before_on_after(void) {
  run_rational_boundary_case(1103u);
  run_rational_boundary_case(1104u);
}

static void case_partial_length_readback(void) {
  uint8_t ipl3_asset[IPL3_ASSET_BYTES];
  t64_machine *machine = create_admitted_profile(AI_PROFILE_ID, ipl3_asset);
  uint32_t remaining = 0u;
  uint32_t expected[] = {16u, 12u, 8u, 4u, 0u};
  size_t i;

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  if (!configure_ai_rate_pair(machine)) {
    t64_machine_destroy(machine);
    return;
  }
  start_ai_spin_loop(machine);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_OK);
  T64_CHECK(submit_ai_frames(machine, UINT32_C(0x100), 4u));
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_LEN, 4u, &remaining),
                T64_STATUS_OK);
  T64_CHECK_INT(remaining, expected[0]);
  for (i = 1u; i < sizeof expected / sizeof expected[0]; i++) {
    T64_CHECK(advance_ai_sample(machine));
    T64_CHECK_INT(t64_machine_bus_read(machine, AI_LEN, 4u, &remaining),
                  T64_STATUS_OK);
    T64_CHECK_INT(remaining, expected[i]);
  }
  T64_CHECK_INT(machine->ai.descriptor_count, 0u);
  t64_machine_destroy(machine);
}

static void case_inflight_source_mutation(void) {
  const uint8_t original[8] = {0x10u, 0x21u, 0x32u, 0x43u,
                               0x54u, 0x65u, 0x76u, 0x87u};
  const uint8_t changed_tail[4] = {0x98u, 0xa9u, 0xbau, 0xcbu};
  const uint8_t expected[8] = {0x10u, 0x21u, 0x32u, 0x43u,
                               0x98u, 0xa9u, 0xbau, 0xcbu};
  uint8_t ipl3_asset[IPL3_ASSET_BYTES];
  uint8_t actual[sizeof expected];
  t64_machine *machine = create_admitted_profile(AI_PROFILE_ID, ipl3_asset);
  size_t copied = 0u;

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  if (!configure_ai_rate_pair(machine)) {
    t64_machine_destroy(machine);
    return;
  }
  (void)memcpy(machine->ram + UINT32_C(0x100), original, sizeof original);
  start_ai_spin_loop(machine);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_OK);
  T64_CHECK(submit_ai_frames(machine, UINT32_C(0x100), 2u));
  T64_CHECK(advance_ai_sample(machine));
  (void)memcpy(machine->ram + UINT32_C(0x104), changed_tail,
               sizeof changed_tail);
  T64_CHECK(advance_ai_sample(machine));
  T64_CHECK_INT(t64_audio_copy(machine, actual, sizeof actual, &copied),
                T64_STATUS_OK);
  T64_CHECK_INT(copied, sizeof expected);
  T64_CHECK(memcmp(actual, expected, sizeof expected) == 0);
  t64_machine_destroy(machine);
}

static void case_active_rate_latch(void) {
  uint8_t ipl3_asset[IPL3_ASSET_BYTES];
  uint8_t packet[8];
  t64_machine *machine = create_admitted_profile(AI_PROFILE_ID, ipl3_asset);
  t64_audio_packet_info info;
  size_t copied = 0u;
  uint64_t origin_tick;
  uint64_t first_sample_tick;
  uint64_t first_descriptor_done_tick;
  uint64_t second_descriptor_first_tick;
  uint64_t second_descriptor_done_tick;
  uint64_t scheduled_tick = UINT64_MAX;
  uint32_t register_value = UINT32_MAX;

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  if (!configure_ai_rate_pair(machine)) {
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK_INT(UINT32_C(7) & UINT32_C(0x000f), UINT32_C(7));
  T64_CHECK_INT(UINT32_C(15) & UINT32_C(0x000f), UINT32_C(15));
  T64_CHECK_INT(UINT32_C(2207) & UINT32_C(0x3fff), UINT32_C(2207));
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_BITRATE, 4u, 7u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_BITRATE, 4u,
                                     &register_value),
                T64_STATUS_OK);
  T64_CHECK_INT(register_value, 7u);
  origin_tick = machine->tick;
  start_ai_spin_loop(machine);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_OK);
  T64_CHECK(submit_ai_frames(machine, UINT32_C(0x100), 2u));
  T64_CHECK(submit_ai_frames(machine, UINT32_C(0x108), 2u));

  first_sample_tick = origin_tick + expected_audio_elapsed_ticks(1u, 1103u);
  first_descriptor_done_tick =
      origin_tick + expected_audio_elapsed_ticks(2u, 1103u);
  T64_CHECK(find_next_ai_sample_tick(machine, &scheduled_tick));
  T64_CHECK_INT(scheduled_tick, first_sample_tick);
  if (!advance_guest_to_in_slices(machine, first_sample_tick)) {
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK_INT(machine->ai.active_byte_cursor, 4u);

  T64_CHECK_INT(t64_machine_bus_write(machine, AI_DACRATE, 4u, 2207u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_DACRATE, 4u,
                                     &register_value),
                T64_STATUS_OK);
  T64_CHECK_INT(register_value, 2207u);
  T64_CHECK_INT(machine->ai.latched_dacrate, 1103u);
  T64_CHECK_INT(machine->ai.latched_bitrate, 7u);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_BITRATE, 4u, 15u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_BITRATE, 4u,
                                     &register_value),
                T64_STATUS_OK);
  T64_CHECK_INT(register_value, 15u);
  T64_CHECK(find_next_ai_sample_tick(machine, &scheduled_tick));
  T64_CHECK_INT(scheduled_tick, first_descriptor_done_tick);
  if (!advance_guest_to_in_slices(machine, first_descriptor_done_tick)) {
    t64_machine_destroy(machine);
    return;
  }

  T64_CHECK_INT(machine->ai.descriptor_count, 1u);
  T64_CHECK_INT(machine->ai.latched_dacrate, 2207u);
  T64_CHECK_INT(machine->ai.latched_bitrate, 15u);
  second_descriptor_first_tick =
      first_descriptor_done_tick + expected_audio_elapsed_ticks(1u, 2207u);
  second_descriptor_done_tick =
      first_descriptor_done_tick + expected_audio_elapsed_ticks(2u, 2207u);
  T64_CHECK(find_next_ai_sample_tick(machine, &scheduled_tick));
  T64_CHECK_INT(scheduled_tick, second_descriptor_first_tick);
  if (!advance_guest_to_in_slices(machine, second_descriptor_done_tick)) {
    t64_machine_destroy(machine);
    return;
  }

  T64_CHECK_INT(t64_audio_info(machine, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.rate_denominator, UINT64_C(1104));
  T64_CHECK_INT(t64_audio_copy(machine, packet, sizeof packet, &copied),
                T64_STATUS_OK);
  T64_CHECK_INT(copied, sizeof packet);
  T64_CHECK_INT(t64_audio_info(machine, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.rate_denominator, UINT64_C(2208));
  t64_machine_destroy(machine);
}

static void case_bitrate_zero_and_ratio(void) {
  uint8_t ipl3_asset[IPL3_ASSET_BYTES];
  t64_machine *machine = create_admitted_profile(AI_PROFILE_ID, ipl3_asset);
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  uint32_t value = UINT32_MAX;

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  if (!configure_ai_rate_pair(machine)) {
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK_INT(t64_machine_digest(machine, digest_before), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_DACRATE, 4u, 1054u),
                T64_STATUS_BUS_DEVICE_REFUSED);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) == 0);
  T64_CHECK_INT(UINT32_C(1055) & UINT32_C(0x3fff), UINT32_C(1055));
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_DACRATE, 4u, 1055u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_DACRATE, 4u, &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, 1055u);

  T64_CHECK_INT(t64_machine_digest(machine, digest_before), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_BITRATE, 4u, 0u),
                T64_STATUS_BUS_DEVICE_REFUSED);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) == 0);
  T64_CHECK_INT(UINT32_C(0xffff000f) & UINT32_C(0x000f), UINT32_C(15));
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_BITRATE, 4u,
                                      UINT32_C(0xffff000f)),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_BITRATE, 4u, &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, 15u);
  T64_CHECK_INT(t64_machine_digest(machine, digest_before), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_BITRATE, 4u,
                                      UINT32_C(0xffff0010)),
                T64_STATUS_BUS_DEVICE_REFUSED);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) == 0);
  t64_machine_destroy(machine);
}

static void case_pause_phase_resume(void) {
  uint8_t ipl3_asset[IPL3_ASSET_BYTES];
  t64_machine *machine = create_admitted_profile(AI_PROFILE_ID, ipl3_asset);
  uint64_t next_tick = UINT64_MAX;
  uint64_t pause_tick;
  uint64_t resumed_tick;
  uint64_t phase_before;
  const uint64_t remaining_ticks = UINT64_C(17);

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  if (!configure_ai_rate_pair(machine)) {
    t64_machine_destroy(machine);
    return;
  }
  start_ai_spin_loop(machine);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_OK);
  T64_CHECK(submit_ai_frames(machine, UINT32_C(0x100), 4u));
  T64_CHECK(advance_ai_sample(machine));
  T64_CHECK_INT(machine->ai.sample_index, 1u);
  T64_CHECK(find_next_ai_sample_tick(machine, &next_tick));
  T64_CHECK(next_tick > machine->tick + remaining_ticks);
  if (next_tick <= machine->tick + remaining_ticks) {
    t64_machine_destroy(machine);
    return;
  }
  pause_tick = next_tick - remaining_ticks;
  T64_CHECK(advance_guest_to_in_slices(machine, pause_tick));
  phase_before = machine->ai.phase_remainder;
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 0u),
                T64_STATUS_OK);
  T64_CHECK(machine->ai.paused);
  T64_CHECK_INT(machine->ai.paused_remaining_ticks, remaining_ticks);
  T64_CHECK(!has_ai_sample_event(machine));
  T64_CHECK_INT(machine->ai.phase_remainder, phase_before);
  T64_CHECK(advance_guest_to_in_slices(machine,
                                       machine->tick + UINT64_C(200)));
  T64_CHECK_INT(machine->ai.sample_index, 1u);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_OK);
  resumed_tick = machine->tick + remaining_ticks;
  T64_CHECK(find_next_ai_sample_tick(machine, &next_tick));
  T64_CHECK_INT(next_tick, resumed_tick);
  T64_CHECK(advance_guest_to_in_slices(machine, resumed_tick - 1u));
  T64_CHECK_INT(machine->ai.sample_index, 1u);
  T64_CHECK(advance_guest_to_in_slices(machine, resumed_tick));
  T64_CHECK_INT(machine->ai.sample_index, 2u);
  T64_CHECK_INT(machine->ai.active_byte_cursor, 8u);
  t64_machine_destroy(machine);
}

static void case_reset_owned_events(void) {
  uint8_t ipl3_asset[IPL3_ASSET_BYTES];
  t64_machine *machine = create_admitted_profile(AI_PROFILE_ID, ipl3_asset);
  size_t queue_count_before;
  size_t offset;
  bool found_unrelated = false;

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  if (!configure_ai_rate_pair(machine)) {
    t64_machine_destroy(machine);
    return;
  }
  start_ai_spin_loop(machine);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_OK);
  T64_CHECK(submit_ai_frames(machine, UINT32_C(0x100), 2u));
  T64_CHECK_INT(t64_test_schedule_synthetic(
                    machine, machine->tick + UINT64_C(1000),
                    T64_TEST_EVENT_NOOP, UINT64_C(55)),
                T64_STATUS_OK);
  t64_mi_set_pending(machine, UINT32_C(0x34));
  queue_count_before = machine->queue_count;
  T64_CHECK(has_ai_sample_event(machine));
  t64_ai_reset_machine_state(machine);
  T64_CHECK_INT(machine->queue_count, queue_count_before - 1u);
  T64_CHECK(!has_ai_sample_event(machine));
  T64_CHECK_INT(machine->ai.descriptor_count, 0u);
  T64_CHECK(!machine->ai.active);
  T64_CHECK(!machine->ai.address_staged);
  T64_CHECK_INT(machine->mi_pending, UINT32_C(0x30));
  for (offset = 0u; offset < machine->queue_count; offset++) {
    const size_t index =
        (machine->queue_head + offset) % T64_EVENT_QUEUE_CAPACITY;
    const t64_event *event = &machine->events[index];
    if (event->device == T64_DEVICE_SYNTHETIC_TEST &&
        event->kind == T64_TEST_EVENT_NOOP && event->value == 55u) {
      found_unrelated = true;
    }
    T64_CHECK(event->device != T64_AI_DEVICE);
  }
  T64_CHECK(found_unrelated);
  t64_machine_destroy(machine);
}

static bool cancel_boot_cpu_event(t64_machine *machine) {
  uint64_t cancelled_tick = UINT64_MAX;
  t64_status status = t64_schedule_cancel(
      machine, T64_DEVICE_CPU, T64_CPU_EVENT_STEP, 0u, &cancelled_tick);

  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(cancelled_tick, 1u);
  return status == T64_STATUS_OK && cancelled_tick == 1u;
}

static bool schedule_cpu_step_at(t64_machine *machine, uint64_t tick) {
  t64_event_reservation reservation;
  t64_status status = t64_schedule_reserve(
      machine, tick, T64_DEVICE_CPU, T64_CPU_EVENT_STEP, 0u, 0u,
      t64_cpu_step, &reservation);

  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) return false;
  status = t64_schedule_commit(machine, &reservation);
  T64_CHECK_INT(status, T64_STATUS_OK);
  return status == T64_STATUS_OK;
}

static void prepare_cpu_sample_store(t64_machine *machine) {
  const uint32_t instruction = UINT32_C(0xad090100);

  machine->ram[UINT32_C(0x1000)] = (uint8_t)(instruction >> 24);
  machine->ram[UINT32_C(0x1001)] = (uint8_t)(instruction >> 16);
  machine->ram[UINT32_C(0x1002)] = (uint8_t)(instruction >> 8);
  machine->ram[UINT32_C(0x1003)] = (uint8_t)instruction;
  machine->cpu.pc = UINT32_C(0x80001000);
  machine->cpu.pc_full = t64_cpu_canonical_address(machine->cpu.pc);
  machine->cpu.gpr[8] = t64_cpu_canonical_address(UINT32_C(0xa0000000));
  machine->cpu.gpr[9] = UINT64_C(0x55667788);
}

static void run_cpu_store_sample_order(bool cpu_store_first) {
  const uint8_t original[4] = {0x11u, 0x22u, 0x33u, 0x44u};
  const uint8_t changed[4] = {0x55u, 0x66u, 0x77u, 0x88u};
  const uint8_t *expected = cpu_store_first ? changed : original;
  uint8_t ipl3_asset[IPL3_ASSET_BYTES];
  t64_machine *machine = create_admitted_profile(AI_PROFILE_ID, ipl3_asset);
  uint64_t sample_tick;

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  if (!configure_ai_rate_pair(machine)) {
    t64_machine_destroy(machine);
    return;
  }
  (void)memcpy(machine->ram + UINT32_C(0x100), original, sizeof original);
  if (!cancel_boot_cpu_event(machine)) {
    t64_machine_destroy(machine);
    return;
  }
  prepare_cpu_sample_store(machine);
  sample_tick = machine->tick + expected_audio_elapsed_ticks(1u, 1103u);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_OK);
  if (cpu_store_first) {
    T64_CHECK(schedule_cpu_step_at(machine, sample_tick));
    T64_CHECK(submit_ai_frames(machine, UINT32_C(0x100), 2u));
  } else {
    T64_CHECK(submit_ai_frames(machine, UINT32_C(0x100), 2u));
    T64_CHECK(schedule_cpu_step_at(machine, sample_tick));
  }
  T64_CHECK(advance_guest_to_in_slices(machine, sample_tick));
  T64_CHECK_INT(machine->cpu.retired_instructions, 1u);
  T64_CHECK_INT(machine->ai.sample_index, 1u);
  T64_CHECK_INT(machine->ai.packet_length, 4u);
  T64_CHECK(memcmp(machine->audio_packets, expected, sizeof original) == 0);
  T64_CHECK(memcmp(machine->ram + UINT32_C(0x100), changed, sizeof changed) ==
            0);
  t64_machine_destroy(machine);
}

static bool ai_profile_vi_write(t64_machine *machine, uint32_t offset,
                                uint32_t value) {
  t64_status status = t64_machine_bus_write(
      machine, UINT32_C(0x04400000) + offset, 4u, value);

  T64_CHECK_INT(status, T64_STATUS_OK);
  return status == T64_STATUS_OK;
}

static bool configure_ai_profile_vi_sync(t64_machine *machine,
                                        uint32_t v_sync,
                                        uint32_t h_sync) {
  size_t pixel;

  for (pixel = 0u; pixel < 12u; pixel++) {
    t64_status status = t64_machine_bus_write(
        machine, UINT32_C(0x3000) + (uint32_t)(pixel * 2u), 2u,
        UINT32_C(0xf801));
    T64_CHECK_INT(status, T64_STATUS_OK);
    if (status != T64_STATUS_OK) return false;
  }
  return ai_profile_vi_write(machine, UINT32_C(0x04), UINT32_C(0x3000)) &&
         ai_profile_vi_write(machine, UINT32_C(0x08), UINT32_C(4)) &&
         ai_profile_vi_write(machine, UINT32_C(0x0c), UINT32_C(2)) &&
         ai_profile_vi_write(machine, UINT32_C(0x14), UINT32_C(0)) &&
         ai_profile_vi_write(machine, UINT32_C(0x18), v_sync) &&
         ai_profile_vi_write(machine, UINT32_C(0x1c), h_sync) &&
         ai_profile_vi_write(machine, UINT32_C(0x24), UINT32_C(4)) &&
         ai_profile_vi_write(machine, UINT32_C(0x28), UINT32_C(0x00020008)) &&
         ai_profile_vi_write(machine, UINT32_C(0x30), UINT32_C(0x00000400)) &&
         ai_profile_vi_write(machine, UINT32_C(0x34), UINT32_C(0x00000400)) &&
         ai_profile_vi_write(machine, UINT32_C(0x00), UINT32_C(2));
}

static bool configure_ai_profile_vi(t64_machine *machine) {
  return configure_ai_profile_vi_sync(machine, UINT32_C(8), UINT32_C(2207));
}

static bool find_next_vi_field_tick(const t64_machine *machine,
                                    uint64_t *tick_out) {
  size_t offset;

  if (machine == NULL || tick_out == NULL) return false;
  for (offset = 0u; offset < machine->queue_count; offset++) {
    const size_t index =
        (machine->queue_head + offset) % T64_EVENT_QUEUE_CAPACITY;
    const t64_event *event = &machine->events[index];
    if (event->device == T64_VI_DEVICE &&
        event->kind == T64_VI_EVENT_FIELD &&
        event->token == machine->vi.event_generation) {
      *tick_out = event->tick;
      return true;
    }
  }
  return false;
}

static void run_vi_ai_tie_order(bool ai_first) {
  uint8_t ipl3_asset[IPL3_ASSET_BYTES];
  t64_machine *machine = create_admitted_profile(AI_PROFILE_ID, ipl3_asset);
  t64_trace_record records[16];
  size_t count = 0u;
  size_t i;
  size_t ai_index = SIZE_MAX;
  size_t vi_index = SIZE_MAX;
  uint64_t dropped = 0u;
  uint64_t sample_tick;
  uint64_t vi_tick = UINT64_MAX;
  bool saturated = false;

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  if (!configure_ai_rate_pair(machine)) {
    t64_machine_destroy(machine);
    return;
  }
  if (!cancel_boot_cpu_event(machine)) {
    t64_machine_destroy(machine);
    return;
  }
  sample_tick = machine->tick + expected_audio_elapsed_ticks(1u, 1103u);
  T64_CHECK_INT(UINT32_C(2207) & UINT32_C(0x0fff), UINT32_C(2207));
  if (ai_first) {
    T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                  T64_STATUS_OK);
    T64_CHECK(submit_ai_frames(machine, UINT32_C(0x100), 2u));
    T64_CHECK(configure_ai_profile_vi(machine));
  } else {
    T64_CHECK(configure_ai_profile_vi(machine));
    T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                  T64_STATUS_OK);
    T64_CHECK(submit_ai_frames(machine, UINT32_C(0x100), 2u));
  }
  T64_CHECK(find_next_ai_sample_tick(machine, &vi_tick));
  T64_CHECK_INT(vi_tick, sample_tick);
  vi_tick = UINT64_MAX;
  T64_CHECK(find_next_vi_field_tick(machine, &vi_tick));
  T64_CHECK_INT(vi_tick, sample_tick);
  T64_CHECK_INT(t64_machine_trace_set_enabled(machine, true), T64_STATUS_OK);
  T64_CHECK(advance_guest_to_in_slices(machine, sample_tick));
  T64_CHECK_INT(t64_machine_trace_drain(machine, records,
                                        sizeof records / sizeof records[0],
                                        &count, &dropped, &saturated),
                T64_STATUS_OK);
  T64_CHECK_INT(dropped, 0u);
  T64_CHECK(!saturated);
  for (i = 0u; i < count; i++) {
    if (records[i].tick != sample_tick) continue;
    if (records[i].device == T64_AI_DEVICE && ai_index == SIZE_MAX)
      ai_index = i;
    if (records[i].device == T64_VI_DEVICE && vi_index == SIZE_MAX)
      vi_index = i;
  }
  T64_CHECK(ai_index != SIZE_MAX);
  T64_CHECK(vi_index != SIZE_MAX);
  if (ai_first) {
    T64_CHECK(ai_index < vi_index);
  } else {
    T64_CHECK(vi_index < ai_index);
  }
  t64_machine_destroy(machine);
}

static void case_same_tick_store_sample(void) {
  run_cpu_store_sample_order(true);
  run_cpu_store_sample_order(false);
  run_vi_ai_tie_order(true);
  run_vi_ai_tie_order(false);
}

static void assert_ai_length_refusal(t64_machine *machine,
                                     t64_status expected_status) {
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  t64_ai_state ai_before = machine->ai;
  size_t queue_count_before = machine->queue_count;
  uint64_t next_sequence_before = machine->next_sequence;

  T64_CHECK_INT(t64_machine_digest(machine, digest_before), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_LEN, 4u, 8u),
                expected_status);
  T64_CHECK(memcmp(&machine->ai, &ai_before, sizeof ai_before) == 0);
  T64_CHECK_INT(machine->queue_count, queue_count_before);
  T64_CHECK_INT(machine->next_sequence, next_sequence_before);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) == 0);
}

static void assert_ai_event_refusal_held(t64_machine *machine,
                                         uint64_t event_tick) {
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  t64_ai_state ai_before = machine->ai;
  t64_stop_request request = {T64_STOP_GUEST_TICK, event_tick};
  t64_stop_result result = {UINT32_MAX, UINT64_MAX};
  size_t queue_count_before = machine->queue_count;
  uint64_t next_sequence_before = machine->next_sequence;
  uint64_t tick_before = machine->tick;

  T64_CHECK_INT(t64_machine_digest(machine, digest_before), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_advance(machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK_INT(result.reason, T64_STOP_REASON_RESOURCE_LIMIT);
  T64_CHECK_INT(result.tick, tick_before);
  T64_CHECK_INT(machine->tick, tick_before);
  T64_CHECK_INT(machine->queue_count, queue_count_before);
  T64_CHECK_INT(machine->next_sequence, next_sequence_before);
  T64_CHECK(memcmp(&machine->ai, &ai_before, sizeof ai_before) == 0);
  T64_CHECK(has_ai_sample_event(machine));
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) == 0);
}

static void case_arithmetic_overflow_refusal(void) {
  uint8_t ipl3_asset[IPL3_ASSET_BYTES];
  t64_machine *machine = create_admitted_profile(AI_PROFILE_ID, ipl3_asset);
  uint64_t event_tick = UINT64_MAX;

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  if (!configure_ai_rate_pair(machine)) {
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_DRAM_ADDR, 4u,
                                      UINT32_C(0x100)),
                T64_STATUS_OK);
  /* Defensive injected guest-time boundary; ordinary register writes remain
   * bounded and are covered by the adjacent accepted/refused pair cases. */
  machine->tick = UINT64_MAX - UINT64_C(100);
  assert_ai_length_refusal(machine, T64_STATUS_TICK_OVERFLOW);
  t64_machine_destroy(machine);

  machine = create_admitted_profile(AI_PROFILE_ID, ipl3_asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  if (!configure_ai_rate_pair(machine)) {
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_DRAM_ADDR, 4u,
                                      UINT32_C(0x100)),
                T64_STATUS_OK);
  machine->ai.descriptor_sequence = UINT64_MAX;
  assert_ai_length_refusal(machine, T64_STATUS_SEQUENCE_EXHAUSTED);
  t64_machine_destroy(machine);

  machine = create_admitted_profile(AI_PROFILE_ID, ipl3_asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  if (!configure_ai_rate_pair(machine)) {
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_DRAM_ADDR, 4u,
                                      UINT32_C(0x100)),
                T64_STATUS_OK);
  machine->ai.event_generation = UINT64_MAX;
  assert_ai_length_refusal(machine, T64_STATUS_SEQUENCE_EXHAUSTED);
  t64_machine_destroy(machine);

  machine = create_admitted_profile(AI_PROFILE_ID, ipl3_asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  if (!configure_ai_rate_pair(machine)) {
    t64_machine_destroy(machine);
    return;
  }
  start_ai_spin_loop(machine);
  T64_CHECK(cancel_boot_cpu_event(machine));
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_OK);
  T64_CHECK(submit_ai_frames(machine, UINT32_C(0x100), 2u));
  T64_CHECK(find_next_ai_sample_tick(machine, &event_tick));
  machine->ai.sample_index = UINT64_MAX;
  assert_ai_event_refusal_held(machine, event_tick);
  t64_machine_destroy(machine);

  machine = create_admitted_profile(AI_PROFILE_ID, ipl3_asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  if (!configure_ai_rate_pair(machine)) {
    t64_machine_destroy(machine);
    return;
  }
  start_ai_spin_loop(machine);
  T64_CHECK(cancel_boot_cpu_event(machine));
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_OK);
  T64_CHECK(submit_ai_frames(machine, UINT32_C(0x100), 2u));
  T64_CHECK(advance_ai_sample(machine));
  T64_CHECK(find_next_ai_sample_tick(machine, &event_tick));
  machine->ai.packet_history_count = UINT64_MAX;
  assert_ai_event_refusal_held(machine, event_tick);
  t64_machine_destroy(machine);

  machine = create_admitted_profile(AI_PROFILE_ID, ipl3_asset);
  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  if (!configure_ai_rate_pair(machine)) {
    t64_machine_destroy(machine);
    return;
  }
  start_ai_spin_loop(machine);
  T64_CHECK(cancel_boot_cpu_event(machine));
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_OK);
  T64_CHECK(submit_ai_frames(machine, UINT32_C(0x100), 2u));
  T64_CHECK(submit_ai_frames(machine, UINT32_C(0x108), 2u));
  T64_CHECK(advance_ai_sample(machine));
  T64_CHECK(find_next_ai_sample_tick(machine, &event_tick));
  machine->ai.event_generation = UINT64_MAX;
  assert_ai_event_refusal_held(machine, event_tick);
  t64_machine_destroy(machine);
}

static void case_audio_info_copy_atomic(void) {
  static const uint8_t expected[8] = {
      0x80u, 0x01u, 0xffu, 0xfeu, 0x12u, 0x34u, 0xedu, 0xccu};
  uint8_t ipl3_asset[IPL3_ASSET_BYTES];
  uint8_t short_output[8];
  uint8_t actual[8];
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  t64_machine *machine = create_admitted_profile(AI_PROFILE_ID, ipl3_asset);
  t64_audio_packet_info info;
  size_t required = SIZE_MAX;
  size_t copied = 0u;
  uint32_t remaining = UINT32_MAX;

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  if (!configure_ai_rate_pair(machine)) {
    t64_machine_destroy(machine);
    return;
  }
  (void)memcpy(machine->ram + UINT32_C(0x100), expected, sizeof expected);
  start_ai_spin_loop(machine);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_OK);
  T64_CHECK(submit_ai_frames(machine, UINT32_C(0x100), 2u));
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_LEN, 4u, &remaining),
                T64_STATUS_OK);
  T64_CHECK_INT(remaining, 8u);
  if (remaining != 8u) {
    t64_machine_destroy(machine);
    return;
  }
  T64_CHECK(advance_ai_sample(machine));
  T64_CHECK(advance_ai_sample(machine));

  T64_CHECK_INT(t64_audio_info(machine, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.sequence, 1u);
  T64_CHECK_INT(info.first_sample_index, 0u);
  T64_CHECK_INT(info.frame_count, 2u);
  T64_CHECK_INT(info.rate_denominator, 1104u);
  T64_CHECK_INT(info.byte_length, sizeof expected);
  T64_CHECK_INT(machine->ai.packet_count, 1u);
  T64_CHECK_INT(t64_machine_digest(machine, digest_before), T64_STATUS_OK);

  (void)memset(short_output, 0xa5, sizeof short_output);
  T64_CHECK_INT(t64_audio_copy(machine, NULL, 0u, &required),
                T64_STATUS_BUFFER_TOO_SMALL);
  T64_CHECK_INT(required, sizeof expected);
  T64_CHECK_INT(machine->ai.packet_count, 1u);
  required = SIZE_MAX;
  T64_CHECK_INT(t64_audio_copy(machine, short_output,
                               sizeof short_output - 1u, &required),
                T64_STATUS_BUFFER_TOO_SMALL);
  T64_CHECK_INT(required, sizeof expected);
  T64_CHECK_INT(short_output[0], 0xa5u);
  T64_CHECK_INT(short_output[sizeof short_output - 1u], 0xa5u);
  T64_CHECK_INT(machine->ai.packet_count, 1u);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) == 0);

  T64_CHECK_INT(t64_audio_copy(machine, actual, sizeof actual, &copied),
                T64_STATUS_OK);
  T64_CHECK_INT(copied, sizeof expected);
  T64_CHECK(memcmp(actual, expected, sizeof expected) == 0);
  T64_CHECK_INT(machine->ai.packet_count, 0u);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) == 0);
  t64_machine_destroy(machine);
}

static bool prepare_two_completed_audio_packets(t64_machine *machine) {
  static const uint8_t pcm[24] = {
      0x00u, 0x01u, 0x00u, 0x02u, 0x00u, 0x03u, 0x00u, 0x04u,
      0x10u, 0x01u, 0x10u, 0x02u, 0x10u, 0x03u, 0x10u, 0x04u,
      0x20u, 0x01u, 0x20u, 0x02u, 0x20u, 0x03u, 0x20u, 0x04u};
  uint32_t remaining = UINT32_MAX;
  size_t i;

  if (machine == NULL || !configure_ai_rate_pair(machine)) return false;
  (void)memcpy(machine->ram + UINT32_C(0x100), pcm, sizeof pcm);
  start_ai_spin_loop(machine);
  if (t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u) != T64_STATUS_OK ||
      !submit_ai_frames(machine, UINT32_C(0x100), 2u) ||
      !submit_ai_frames(machine, UINT32_C(0x108), 2u) ||
      t64_machine_bus_read(machine, AI_LEN, 4u, &remaining) != T64_STATUS_OK) {
    T64_CHECK(false);
    return false;
  }
  T64_CHECK_INT(remaining, 8u);
  if (remaining != 8u) return false;
  for (i = 0u; i < 4u; i++) {
    if (!advance_ai_sample(machine)) return false;
  }
  T64_CHECK_INT(machine->ai.packet_count, T64_AUDIO_QUEUE_CAPACITY);
  T64_CHECK_INT(machine->ai.descriptor_count, 0u);
  return machine->ai.packet_count == T64_AUDIO_QUEUE_CAPACITY &&
         machine->ai.descriptor_count == 0u;
}

static bool prepare_blocked_ai_event(t64_machine *machine,
                                     t64_event *event_out) {
  static const uint8_t third_packet[8] = {
      0x20u, 0x01u, 0x20u, 0x02u, 0x20u, 0x03u, 0x20u, 0x04u};
  size_t offset;
  uint32_t remaining = UINT32_MAX;

  if (machine == NULL || event_out == NULL ||
      !prepare_two_completed_audio_packets(machine)) {
    return false;
  }
  (void)memcpy(machine->ram + UINT32_C(0x110), third_packet,
               sizeof third_packet);
  T64_CHECK(submit_ai_frames(machine, UINT32_C(0x110), 2u));
  if (t64_machine_bus_read(machine, AI_LEN, 4u, &remaining) != T64_STATUS_OK) {
    T64_CHECK(false);
    return false;
  }
  T64_CHECK_INT(remaining, 8u);
  if (remaining != 8u) return false;
  for (offset = 0u; offset < machine->queue_count; offset++) {
    const size_t index =
        (machine->queue_head + offset) % T64_EVENT_QUEUE_CAPACITY;
    const t64_event *event = &machine->events[index];
    if (event->device == T64_AI_DEVICE &&
        event->kind == T64_AI_EVENT_SAMPLE &&
        event->token == machine->ai.event_token) {
      *event_out = *event;
      return true;
    }
  }
  T64_CHECK(false);
  return false;
}

static void case_output_full_same_event(void) {
  uint8_t ipl3_asset[IPL3_ASSET_BYTES];
  t64_machine *machine = create_admitted_profile(AI_PROFILE_ID, ipl3_asset);
  t64_event expected_event;
  t64_stop_request request;
  t64_stop_result stop = {0u, 0u};
  uint64_t sample_before;
  size_t cursor_before;
  uint64_t phase_before;
  uint64_t blocked_tick;

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  if (!prepare_blocked_ai_event(machine, &expected_event)) {
    t64_machine_destroy(machine);
    return;
  }
  sample_before = machine->ai.sample_index;
  cursor_before = machine->ai.active_byte_cursor;
  phase_before = machine->ai.phase_remainder;
  request.kind = T64_STOP_GUEST_TICK;
  request.value = expected_event.tick;
  T64_CHECK_INT(t64_machine_advance(machine, &request, &stop), T64_STATUS_OK);
  T64_CHECK_INT(stop.reason, T64_STOP_REASON_RESOURCE_LIMIT);
  blocked_tick = machine->tick;
  T64_CHECK_INT(stop.tick, blocked_tick);
  T64_CHECK(blocked_tick < expected_event.tick);
  T64_CHECK_INT(machine->ai.sample_index, sample_before);
  T64_CHECK_INT(machine->ai.active_byte_cursor, cursor_before);
  T64_CHECK_INT(machine->ai.phase_remainder, phase_before);
  T64_CHECK_INT(machine->ai.packet_count, T64_AUDIO_QUEUE_CAPACITY);
  T64_CHECK(machine->ai.packet_reserved == false);
  T64_CHECK(machine->queue_count > 0u);
  if (machine->queue_count > 0u) {
    const t64_event *front = &machine->events[machine->queue_head];
    T64_CHECK_INT(front->device, T64_AI_DEVICE);
    T64_CHECK_INT(front->kind, T64_AI_EVENT_SAMPLE);
    T64_CHECK_INT(front->tick, expected_event.tick);
    T64_CHECK_INT(front->sequence, expected_event.sequence);
    T64_CHECK_INT(front->token, expected_event.token);
  }
  stop.reason = 0u;
  stop.tick = 0u;
  T64_CHECK_INT(t64_machine_advance(machine, &request, &stop), T64_STATUS_OK);
  T64_CHECK_INT(stop.reason, T64_STOP_REASON_RESOURCE_LIMIT);
  T64_CHECK_INT(machine->tick, blocked_tick);
  T64_CHECK_INT(machine->ai.sample_index, sample_before);
  T64_CHECK_INT(machine->ai.active_byte_cursor, cursor_before);
  T64_CHECK_INT(machine->ai.phase_remainder, phase_before);
  T64_CHECK_INT(machine->ai.packet_count, T64_AUDIO_QUEUE_CAPACITY);
  T64_CHECK_INT(machine->queue_count > 0u, 1u);
  if (machine->queue_count > 0u) {
    const t64_event *front = &machine->events[machine->queue_head];
    T64_CHECK_INT(front->tick, expected_event.tick);
    T64_CHECK_INT(front->sequence, expected_event.sequence);
    T64_CHECK_INT(front->token, expected_event.token);
  }
  t64_machine_destroy(machine);
}

static void case_copy_alias_and_overflow(void) {
  static const uint8_t pcm[8] = {
      0x80u, 0x01u, 0xffu, 0xfeu, 0x12u, 0x34u, 0xedu, 0xccu};
  uint8_t ipl3_asset[IPL3_ASSET_BYTES];
  uint8_t image_before[8];
  uint8_t overlap[16];
  uint8_t ram_before[8];
  t64_machine *machine = create_admitted_profile(AI_PROFILE_ID, ipl3_asset);
  t64_audio_packet_info info;
  size_t required = (size_t)UINT32_C(0x12345678);
  size_t queue_before;
  size_t i;

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  if (!configure_ai_rate_pair(machine)) {
    t64_machine_destroy(machine);
    return;
  }
  (void)memcpy(machine->ram + UINT32_C(0x100), pcm, sizeof pcm);
  start_ai_spin_loop(machine);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_OK);
  T64_CHECK(submit_ai_frames(machine, UINT32_C(0x100), 2u));
  T64_CHECK(advance_ai_sample(machine));
  T64_CHECK(advance_ai_sample(machine));
  T64_CHECK_INT(machine->ai.packet_count, 1u);
  queue_before = machine->ai.packet_count;
  T64_CHECK_INT(t64_audio_info(machine, &info), T64_STATUS_OK);
  (void)memcpy(ram_before, machine->ram + UINT32_C(0x200),
               sizeof ram_before);
  T64_CHECK_INT(t64_audio_copy(machine, machine->ram + UINT32_C(0x200),
                               sizeof pcm, &required),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK_INT(required, (size_t)UINT32_C(0x12345678));
  T64_CHECK(memcmp(ram_before, machine->ram + UINT32_C(0x200),
                   sizeof ram_before) == 0);
  T64_CHECK_INT(machine->ai.packet_count, queue_before);

  (void)memcpy(image_before, machine->image_storage.data,
               sizeof image_before);
  T64_CHECK_INT(t64_audio_copy(machine, machine->image_storage.data,
                               sizeof image_before, &required),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK(memcmp(image_before, machine->image_storage.data,
                   sizeof image_before) == 0);
  T64_CHECK_INT(required, (size_t)UINT32_C(0x12345678));

  for (i = 0u; i < sizeof overlap; i++) overlap[i] = 0xa5u;
  T64_CHECK_INT(t64_audio_copy(machine, overlap, 8u,
                               (size_t *)(void *)(overlap + 1u)),
                T64_STATUS_INVALID_ARGUMENT);
  for (i = 0u; i < sizeof overlap; i++) T64_CHECK_INT(overlap[i], 0xa5u);
  T64_CHECK_INT(required, (size_t)UINT32_C(0x12345678));

  T64_CHECK_INT(t64_audio_copy(
                    machine,
                    (uint8_t *)(uintptr_t)(UINTPTR_MAX - (uintptr_t)3u),
                    8u, &required),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK_INT(required, (size_t)UINT32_C(0x12345678));
  T64_CHECK_INT(t64_audio_copy(
                    machine, NULL, 0u,
                    (size_t *)(uintptr_t)(UINTPTR_MAX - (uintptr_t)3u)),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK_INT(t64_audio_info(
                    machine,
                    (t64_audio_packet_info *)(uintptr_t)(
                        UINTPTR_MAX - (uintptr_t)3u)),
                T64_STATUS_INVALID_ARGUMENT);
  T64_CHECK_INT(machine->ai.packet_count, queue_before);
  t64_machine_destroy(machine);
}

static void case_drain_resume_exactly_once(void) {
  static const uint8_t first_packet[8] = {
      0x00u, 0x01u, 0x00u, 0x02u, 0x00u, 0x03u, 0x00u, 0x04u};
  static const uint8_t second_packet[8] = {
      0x10u, 0x01u, 0x10u, 0x02u, 0x10u, 0x03u, 0x10u, 0x04u};
  static const uint8_t third_packet[8] = {
      0x20u, 0x01u, 0x20u, 0x02u, 0x20u, 0x03u, 0x20u, 0x04u};
  uint8_t ipl3_asset[IPL3_ASSET_BYTES];
  uint8_t copied_packet[8];
  uint8_t digest_before[T64_DIGEST_SIZE];
  uint8_t digest_after[T64_DIGEST_SIZE];
  t64_machine *machine = create_admitted_profile(AI_PROFILE_ID, ipl3_asset);
  t64_event blocked_event;
  t64_event retained_event;
  t64_audio_packet_info info;
  t64_stop_request request;
  t64_stop_result stop = {0u, 0u};
  size_t copied = 0u;
  size_t offset;
  size_t cursor_before;
  size_t packet_length_before;
  uint64_t tick_at_block;
  uint64_t sample_before;
  uint64_t phase_before;
  uint32_t mi_before;
  bool found_event = false;
  bool retained = false;
  uint32_t remaining = UINT32_MAX;

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  if (!configure_ai_rate_pair(machine)) {
    t64_machine_destroy(machine);
    return;
  }
  (void)memcpy(machine->ram + UINT32_C(0x100), first_packet,
               sizeof first_packet);
  (void)memcpy(machine->ram + UINT32_C(0x108), second_packet,
               sizeof second_packet);
  (void)memcpy(machine->ram + UINT32_C(0x110), third_packet,
               sizeof third_packet);
  start_ai_spin_loop(machine);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_OK);
  T64_CHECK(submit_ai_frames(machine, UINT32_C(0x100), 2u));
  T64_CHECK(submit_ai_frames(machine, UINT32_C(0x108), 2u));
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_LEN, 4u, &remaining),
                T64_STATUS_OK);
  T64_CHECK_INT(remaining, 8u);
  T64_CHECK(advance_ai_sample(machine));
  T64_CHECK(advance_ai_sample(machine));
  T64_CHECK(advance_ai_sample(machine));
  T64_CHECK(advance_ai_sample(machine));
  T64_CHECK_INT(machine->ai.packet_count, T64_AUDIO_QUEUE_CAPACITY);
  T64_CHECK_INT(machine->ai.descriptor_count, 0u);

  T64_CHECK(submit_ai_frames(machine, UINT32_C(0x110), 2u));
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_LEN, 4u, &remaining),
                T64_STATUS_OK);
  T64_CHECK_INT(remaining, 8u);
  for (offset = 0u; offset < machine->queue_count; offset++) {
    const size_t index =
        (machine->queue_head + offset) % T64_EVENT_QUEUE_CAPACITY;
    const t64_event *event = &machine->events[index];
    if (event->device == T64_AI_DEVICE &&
        event->kind == T64_AI_EVENT_SAMPLE &&
        event->token == machine->ai.event_token) {
      blocked_event = *event;
      found_event = true;
      break;
    }
  }
  T64_CHECK(found_event);
  if (!found_event) {
    t64_machine_destroy(machine);
    return;
  }

  sample_before = machine->ai.sample_index;
  phase_before = machine->ai.phase_remainder;
  cursor_before = machine->ai.active_byte_cursor;
  packet_length_before = machine->ai.packet_length;
  mi_before = machine->mi_pending;
  request.kind = T64_STOP_GUEST_TICK;
  request.value = blocked_event.tick;
  T64_CHECK_INT(t64_machine_advance(machine, &request, &stop), T64_STATUS_OK);
  T64_CHECK_INT(stop.reason, T64_STOP_REASON_RESOURCE_LIMIT);
  T64_CHECK_INT(stop.tick, machine->tick);
  T64_CHECK(machine->tick < blocked_event.tick);
  tick_at_block = machine->tick;
  T64_CHECK_INT(machine->ai.sample_index, sample_before);
  T64_CHECK_INT(machine->ai.phase_remainder, phase_before);
  T64_CHECK_INT(machine->ai.active_byte_cursor, cursor_before);
  T64_CHECK_INT(machine->ai.packet_length, packet_length_before);
  T64_CHECK_INT(machine->mi_pending, mi_before);
  T64_CHECK(machine->ai.packet_reserved == false);
  T64_CHECK(machine->queue_count > 0u);
  if (machine->queue_count > 0u) {
    const t64_event *front_event = &machine->events[machine->queue_head];
    T64_CHECK_INT(front_event->device, T64_AI_DEVICE);
    T64_CHECK_INT(front_event->kind, T64_AI_EVENT_SAMPLE);
    T64_CHECK_INT(front_event->tick, blocked_event.tick);
    T64_CHECK_INT(front_event->sequence, blocked_event.sequence);
    T64_CHECK_INT(front_event->token, blocked_event.token);
  }
  for (offset = 0u; offset < machine->queue_count; offset++) {
    const size_t index =
        (machine->queue_head + offset) % T64_EVENT_QUEUE_CAPACITY;
    const t64_event *event = &machine->events[index];
    if (event->device == T64_AI_DEVICE &&
        event->kind == T64_AI_EVENT_SAMPLE &&
        event->token == blocked_event.token &&
        event->tick == blocked_event.tick &&
        event->sequence == blocked_event.sequence) {
      retained_event = *event;
      retained = true;
      break;
    }
  }
  T64_CHECK(retained);
  if (retained) {
    T64_CHECK_INT(retained_event.tick, blocked_event.tick);
    T64_CHECK_INT(retained_event.sequence, blocked_event.sequence);
    T64_CHECK_INT(retained_event.token, blocked_event.token);
  }
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  (void)memcpy(digest_before, digest_after, sizeof digest_before);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) == 0);

  T64_CHECK_INT(t64_audio_info(machine, &info), T64_STATUS_OK);
  T64_CHECK_INT(t64_audio_copy(machine, copied_packet, sizeof copied_packet,
                               &copied),
                T64_STATUS_OK);
  T64_CHECK_INT(copied, sizeof first_packet);
  T64_CHECK(memcmp(copied_packet, first_packet, sizeof first_packet) == 0);
  T64_CHECK_INT(machine->tick, tick_at_block);
  T64_CHECK_INT(machine->ai.packet_count, 1u);
  T64_CHECK_INT(t64_machine_digest(machine, digest_after), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_before, digest_after, sizeof digest_before) == 0);

  stop.reason = 0u;
  stop.tick = 0u;
  T64_CHECK_INT(t64_machine_advance(machine, &request, &stop), T64_STATUS_OK);
  T64_CHECK_INT(stop.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(machine->tick, blocked_event.tick);
  T64_CHECK_INT(machine->ai.sample_index, sample_before + 1u);
  T64_CHECK_INT(machine->ai.active_byte_cursor, 4u);
  T64_CHECK_INT(machine->ai.packet_length, 4u);
  T64_CHECK(machine->ai.packet_reserved);
  T64_CHECK_INT(machine->ai.packet_count, 1u);

  stop.reason = 0u;
  stop.tick = 0u;
  T64_CHECK_INT(t64_machine_advance(machine, &request, &stop), T64_STATUS_OK);
  T64_CHECK_INT(stop.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(machine->ai.sample_index, sample_before + 1u);
  T64_CHECK(advance_ai_sample(machine));
  T64_CHECK_INT(machine->ai.packet_count, T64_AUDIO_QUEUE_CAPACITY);
  T64_CHECK_INT(t64_audio_info(machine, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.sequence, 2u);
  T64_CHECK_INT(t64_audio_copy(machine, copied_packet, sizeof copied_packet,
                               &copied),
                T64_STATUS_OK);
  T64_CHECK_INT(copied, sizeof second_packet);
  T64_CHECK(memcmp(copied_packet, second_packet, sizeof second_packet) == 0);
  T64_CHECK_INT(t64_audio_info(machine, &info), T64_STATUS_OK);
  T64_CHECK_INT(info.sequence, 3u);
  T64_CHECK_INT(t64_audio_copy(machine, copied_packet, sizeof copied_packet,
                               &copied),
                T64_STATUS_OK);
  T64_CHECK_INT(copied, sizeof third_packet);
  T64_CHECK(memcmp(copied_packet, third_packet, sizeof third_packet) == 0);
  t64_machine_destroy(machine);
}

static void case_reserve_before_first_sample(void) {
  static const uint8_t first_packet[8] = {
      0x00u, 0x01u, 0x00u, 0x02u, 0x00u, 0x03u, 0x00u, 0x04u};
  static const uint8_t second_packet[8] = {
      0x10u, 0x01u, 0x10u, 0x02u, 0x10u, 0x03u, 0x10u, 0x04u};
  uint8_t ipl3_asset[IPL3_ASSET_BYTES];
  t64_machine *machine = create_admitted_profile(AI_PROFILE_ID, ipl3_asset);
  t64_event event;
  t64_stop_request request;
  t64_stop_result stop = {0u, 0u};
  size_t sample_slot = SIZE_MAX;
  size_t i;
  uint64_t sample_before;
  uint64_t phase_before;
  size_t cursor_before;
  uint32_t mi_before;

  T64_CHECK(machine != NULL);
  if (machine == NULL) return;
  if (!prepare_blocked_ai_event(machine, &event)) {
    t64_machine_destroy(machine);
    return;
  }
  sample_before = machine->ai.sample_index;
  phase_before = machine->ai.phase_remainder;
  cursor_before = machine->ai.active_byte_cursor;
  mi_before = machine->mi_pending;
  request.kind = T64_STOP_GUEST_TICK;
  request.value = event.tick;
  T64_CHECK_INT(t64_machine_advance(machine, &request, &stop), T64_STATUS_OK);
  T64_CHECK_INT(stop.reason, T64_STOP_REASON_RESOURCE_LIMIT);
  T64_CHECK_INT(machine->ai.packet_count, T64_AUDIO_QUEUE_CAPACITY);
  T64_CHECK_INT(machine->ai.sample_index, sample_before);
  T64_CHECK_INT(machine->ai.phase_remainder, phase_before);
  T64_CHECK_INT(machine->ai.active_byte_cursor, cursor_before);
  T64_CHECK_INT(machine->ai.packet_length, 0u);
  T64_CHECK(machine->ai.packet_reserved == false);
  T64_CHECK_INT(machine->mi_pending, mi_before);
  T64_CHECK(machine->queue_count > 0u);
  if (machine->queue_count > 0u) {
    const t64_event *front = &machine->events[machine->queue_head];
    T64_CHECK_INT(front->device, T64_AI_DEVICE);
    T64_CHECK_INT(front->kind, T64_AI_EVENT_SAMPLE);
    T64_CHECK_INT(front->tick, event.tick);
    T64_CHECK_INT(front->sequence, event.sequence);
    T64_CHECK_INT(front->token, event.token);
  }
  for (i = 0u; i < T64_AUDIO_QUEUE_CAPACITY; i++) {
    if (machine->ai.packet_info[i].sequence == 1u) sample_slot = i;
  }
  T64_CHECK(sample_slot < T64_AUDIO_QUEUE_CAPACITY);
  if (sample_slot < T64_AUDIO_QUEUE_CAPACITY) {
    T64_CHECK(memcmp(machine->audio_packets +
                         sample_slot * T64_AUDIO_MAX_PACKET_BYTES,
                     first_packet, sizeof first_packet) == 0);
  }
  sample_slot = SIZE_MAX;
  for (i = 0u; i < T64_AUDIO_QUEUE_CAPACITY; i++) {
    if (machine->ai.packet_info[i].sequence == 2u) sample_slot = i;
  }
  T64_CHECK(sample_slot < T64_AUDIO_QUEUE_CAPACITY);
  if (sample_slot < T64_AUDIO_QUEUE_CAPACITY) {
    T64_CHECK(memcmp(machine->audio_packets +
                         sample_slot * T64_AUDIO_MAX_PACKET_BYTES,
                     second_packet, sizeof second_packet) == 0);
  }
  t64_machine_destroy(machine);
}

static void case_canonical_producer_history(void) {
  uint8_t asset_a[IPL3_ASSET_BYTES];
  uint8_t asset_b[IPL3_ASSET_BYTES];
  uint8_t packet[8];
  uint8_t digest_a[T64_DIGEST_SIZE];
  uint8_t digest_b[T64_DIGEST_SIZE];
  uint8_t digest_changed[T64_DIGEST_SIZE];
  t64_machine *machine_a = create_admitted_profile(AI_PROFILE_ID, asset_a);
  t64_machine *machine_b = create_admitted_profile(AI_PROFILE_ID, asset_b);
  t64_audio_packet_info info;
  size_t copied = 0u;
  size_t i;
  uint64_t tick_before;
  uint64_t phase_before;
  uint32_t latch_before;
  uint8_t history_before;

  T64_CHECK(machine_a != NULL);
  T64_CHECK(machine_b != NULL);
  if (machine_a == NULL || machine_b == NULL) {
    if (machine_a != NULL) t64_machine_destroy(machine_a);
    if (machine_b != NULL) t64_machine_destroy(machine_b);
    return;
  }
  if (!prepare_two_completed_audio_packets(machine_a) ||
      !prepare_two_completed_audio_packets(machine_b)) {
    t64_machine_destroy(machine_a);
    t64_machine_destroy(machine_b);
    return;
  }
  T64_CHECK_INT(t64_machine_digest(machine_a, digest_a), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(machine_b, digest_b), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_a, digest_b, sizeof digest_a) == 0);
  tick_before = machine_b->tick;
  for (i = 0u; i < 2u; i++) {
    T64_CHECK_INT(t64_audio_info(machine_b, &info), T64_STATUS_OK);
    T64_CHECK_INT(info.sequence, i + 1u);
    T64_CHECK_INT(t64_audio_copy(machine_b, packet, sizeof packet, &copied),
                  T64_STATUS_OK);
    T64_CHECK_INT(copied, sizeof packet);
  }
  T64_CHECK_INT(machine_b->tick, tick_before);
  T64_CHECK_INT(machine_b->ai.packet_count, 0u);
  T64_CHECK_INT(machine_b->ai.packet_history_count, 2u);
  T64_CHECK_INT(t64_machine_digest(machine_b, digest_b), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_a, digest_b, sizeof digest_a) == 0);

  phase_before = machine_b->ai.phase_remainder;
  machine_b->ai.phase_remainder++;
  T64_CHECK_INT(t64_machine_digest(machine_b, digest_changed), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_a, digest_changed, sizeof digest_a) != 0);
  machine_b->ai.phase_remainder = phase_before;
  latch_before = machine_b->ai.latched_dacrate;
  machine_b->ai.latched_dacrate ^= UINT32_C(1);
  T64_CHECK_INT(t64_machine_digest(machine_b, digest_changed), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_a, digest_changed, sizeof digest_a) != 0);
  machine_b->ai.latched_dacrate = latch_before;
  history_before = machine_b->ai.packet_history_sha256[0];
  machine_b->ai.packet_history_sha256[0] ^= UINT8_C(1);
  T64_CHECK_INT(t64_machine_digest(machine_b, digest_changed), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_a, digest_changed, sizeof digest_a) != 0);
  machine_b->ai.packet_history_sha256[0] = history_before;

  T64_CHECK(submit_ai_frames(machine_b, UINT32_C(0x110), 2u));
  T64_CHECK_INT(t64_machine_digest(machine_b, digest_changed), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_a, digest_changed, sizeof digest_a) != 0);
  T64_CHECK(advance_ai_sample(machine_b));
  T64_CHECK_INT(machine_b->ai.active_byte_cursor, 4u);
  T64_CHECK_INT(machine_b->ai.packet_length, 4u);
  T64_CHECK(machine_b->ai.packet_reserved);
  T64_CHECK_INT(t64_machine_digest(machine_b, digest_changed), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_a, digest_changed, sizeof digest_a) != 0);
  t64_machine_destroy(machine_a);
  t64_machine_destroy(machine_b);
}

static void case_output_suite(void) {
  puts("case audio-info-copy-atomic");
  case_audio_info_copy_atomic();
  puts("case reserve-before-first-sample");
  case_reserve_before_first_sample();
  puts("case output-full-same-event");
  case_output_full_same_event();
  puts("case drain-resume-exactly-once");
  case_drain_resume_exactly_once();
  puts("case copy-alias-and-overflow");
  case_copy_alias_and_overflow();
  puts("case canonical-producer-history");
  case_canonical_producer_history();
}

static bool configure_ai_test_rate(t64_machine *machine, uint32_t dacrate,
                                   uint32_t bitrate) {
  uint32_t value = UINT32_MAX;
  t64_status status;

  T64_CHECK_INT(dacrate & UINT32_C(0x3fff), dacrate);
  T64_CHECK_INT(bitrate & UINT32_C(0x000f), bitrate);
  status = t64_machine_bus_write(machine, AI_DACRATE, 4u, dacrate);
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) return false;
  status = t64_machine_bus_write(machine, AI_BITRATE, 4u, bitrate);
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) return false;
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_DACRATE, 4u, &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, dacrate);
  T64_CHECK_INT(t64_machine_bus_read(machine, AI_BITRATE, 4u, &value),
                T64_STATUS_OK);
  T64_CHECK_INT(value, bitrate);
  return true;
}

static bool prepare_two_descriptor_stream(t64_machine *machine,
                                          const uint8_t pcm[16],
                                          uint32_t dacrate,
                                          uint32_t bitrate,
                                          uint64_t *origin_tick_out) {
  uint32_t remaining = UINT32_MAX;
  t64_status status;

  if (machine == NULL || pcm == NULL || origin_tick_out == NULL ||
      !configure_ai_test_rate(machine, dacrate, bitrate)) {
    return false;
  }
  (void)memcpy(machine->ram + UINT32_C(0x100), pcm, 16u);
  start_ai_spin_loop(machine);
  *origin_tick_out = machine->tick;
  status = t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u);
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK ||
      !submit_ai_frames(machine, UINT32_C(0x100), 2u) ||
      !submit_ai_frames(machine, UINT32_C(0x108), 2u)) {
    return false;
  }
  status = t64_machine_bus_read(machine, AI_LEN, 4u, &remaining);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(remaining, 8u);
  return status == T64_STATUS_OK && remaining == 8u;
}

static bool copy_audio_packet(t64_machine *machine,
                              t64_audio_packet_info *info_out,
                              uint8_t bytes_out[8]) {
  size_t copied = 0u;
  t64_status status = t64_audio_info(machine, info_out);

  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) return false;
  status = t64_audio_copy(machine, bytes_out, 8u, &copied);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(copied, 8u);
  return status == T64_STATUS_OK && copied == 8u;
}

static void case_whole_versus_split(void) {
  static const uint8_t pcm[16] = {
      0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u, 0x08u,
      0x11u, 0x12u, 0x13u, 0x14u, 0x15u, 0x16u, 0x17u, 0x18u};
  uint8_t asset_whole[IPL3_ASSET_BYTES];
  uint8_t asset_split[IPL3_ASSET_BYTES];
  uint8_t whole_bytes[2][8];
  uint8_t split_bytes[2][8];
  uint8_t digest_whole[T64_DIGEST_SIZE];
  uint8_t digest_split[T64_DIGEST_SIZE];
  t64_machine *whole = create_admitted_profile(AI_PROFILE_ID, asset_whole);
  t64_machine *split = create_admitted_profile(AI_PROFILE_ID, asset_split);
  t64_audio_packet_info whole_info[2];
  t64_audio_packet_info split_info[2];
  uint64_t whole_origin = 0u;
  uint64_t split_origin = 0u;
  uint64_t first_tick;
  uint64_t second_packet_first_tick;
  uint64_t final_tick;
  uint32_t packet;

  T64_CHECK(whole != NULL);
  T64_CHECK(split != NULL);
  if (whole == NULL || split == NULL) {
    if (whole != NULL) t64_machine_destroy(whole);
    if (split != NULL) t64_machine_destroy(split);
    return;
  }
  if (!prepare_two_descriptor_stream(whole, pcm, 131u, 1u, &whole_origin) ||
      !prepare_two_descriptor_stream(split, pcm, 131u, 1u, &split_origin)) {
    t64_machine_destroy(whole);
    t64_machine_destroy(split);
    return;
  }
  T64_CHECK_INT(whole_origin, split_origin);
  T64_CHECK(find_next_ai_sample_tick(whole, &first_tick));
  second_packet_first_tick = whole_origin + expected_audio_elapsed_ticks(3u, 131u);
  final_tick = whole_origin + expected_audio_elapsed_ticks(4u, 131u);
  T64_CHECK(advance_guest_to(whole, final_tick));

  T64_CHECK(advance_guest_to(split, first_tick - 1u));
  T64_CHECK(advance_guest_to(split, first_tick));
  T64_CHECK(advance_guest_to(split, second_packet_first_tick));
  T64_CHECK_INT(split->ai.packet_count, 1u);
  T64_CHECK(copy_audio_packet(split, &split_info[0], split_bytes[0]));
  T64_CHECK_INT(split_info[0].sequence, 1u);
  T64_CHECK(advance_guest_to(split, final_tick));
  T64_CHECK_INT(whole->tick, final_tick);
  T64_CHECK_INT(split->tick, final_tick);
  T64_CHECK_INT(whole->ai.sample_index, 4u);
  T64_CHECK_INT(split->ai.sample_index, 4u);
  T64_CHECK_INT(whole->ai.packet_count, 2u);
  T64_CHECK_INT(split->ai.packet_count, 1u);
  T64_CHECK_INT(t64_machine_digest(whole, digest_whole), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(split, digest_split), T64_STATUS_OK);
  T64_CHECK(memcmp(digest_whole, digest_split, sizeof digest_whole) == 0);

  for (packet = 0u; packet < 2u; packet++) {
    T64_CHECK(copy_audio_packet(whole, &whole_info[packet],
                                whole_bytes[packet]));
  }
  T64_CHECK(copy_audio_packet(split, &split_info[1], split_bytes[1]));
  for (packet = 0u; packet < 2u; packet++) {
    T64_CHECK(memcmp(&whole_info[packet], &split_info[packet],
                     sizeof whole_info[packet]) == 0);
    T64_CHECK(memcmp(whole_bytes[packet], split_bytes[packet], 8u) == 0);
    T64_CHECK(memcmp(whole_bytes[packet], pcm + packet * 8u, 8u) == 0);
  }
  t64_machine_destroy(whole);
  t64_machine_destroy(split);
}

static void case_interleaved_machine_isolation(void) {
  static const uint8_t pcm_a[16] = {
      0x10u, 0x11u, 0x12u, 0x13u, 0x14u, 0x15u, 0x16u, 0x17u,
      0x18u, 0x19u, 0x1au, 0x1bu, 0x1cu, 0x1du, 0x1eu, 0x1fu};
  static const uint8_t pcm_b[16] = {
      0xa0u, 0xa1u, 0xa2u, 0xa3u, 0xa4u, 0xa5u, 0xa6u, 0xa7u,
      0xa8u, 0xa9u, 0xaau, 0xabu, 0xacu, 0xadu, 0xaeu, 0xafu};
  uint8_t assets[4][IPL3_ASSET_BYTES];
  uint8_t interleaved_bytes[2][2][8];
  uint8_t reference_bytes[2][2][8];
  uint8_t interleaved_digest[T64_DIGEST_SIZE];
  uint8_t reference_digest[T64_DIGEST_SIZE];
  t64_audio_packet_info interleaved_info[2][2];
  t64_audio_packet_info reference_info[2][2];
  t64_machine *machines[4];
  uint64_t origins[4] = {0u, 0u, 0u, 0u};
  uint64_t *sample_origins[4] = {&origins[0], &origins[1], &origins[2],
                                 &origins[3]};
  uint32_t stream;
  uint32_t sample;

  for (stream = 0u; stream < 4u; stream++) {
    machines[stream] = create_admitted_profile(AI_PROFILE_ID, assets[stream]);
    T64_CHECK(machines[stream] != NULL);
    if (machines[stream] == NULL) {
      for (uint32_t cleanup = 0u; cleanup < stream; cleanup++)
        t64_machine_destroy(machines[cleanup]);
      return;
    }
  }
  if (!prepare_two_descriptor_stream(machines[0], pcm_a, 1103u, 15u,
                                     sample_origins[0]) ||
      !prepare_two_descriptor_stream(machines[1], pcm_b, 2207u, 15u,
                                     sample_origins[1]) ||
      !prepare_two_descriptor_stream(machines[2], pcm_a, 1103u, 15u,
                                     sample_origins[2]) ||
      !prepare_two_descriptor_stream(machines[3], pcm_b, 2207u, 15u,
                                     sample_origins[3])) {
    for (stream = 0u; stream < 4u; stream++) t64_machine_destroy(machines[stream]);
    return;
  }
  for (stream = 0u; stream < 4u; stream++)
    T64_CHECK_INT(origins[stream], origins[stream % 2u]);
  for (sample = 0u; sample < 4u; sample++) {
    T64_CHECK(advance_ai_sample(machines[0]));
    T64_CHECK(advance_ai_sample(machines[1]));
    if (sample == 1u) {
      T64_CHECK(copy_audio_packet(machines[0], &interleaved_info[0][0],
                                  interleaved_bytes[0][0]));
    }
  }
  for (sample = 0u; sample < 4u; sample++) {
    T64_CHECK(advance_ai_sample(machines[2]));
    T64_CHECK(advance_ai_sample(machines[3]));
  }
  T64_CHECK_INT(t64_machine_digest(machines[0], interleaved_digest),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(machines[2], reference_digest),
                T64_STATUS_OK);
  T64_CHECK(memcmp(interleaved_digest, reference_digest,
                   sizeof interleaved_digest) == 0);
  T64_CHECK_INT(t64_machine_digest(machines[1], interleaved_digest),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_digest(machines[3], reference_digest),
                T64_STATUS_OK);
  T64_CHECK(memcmp(interleaved_digest, reference_digest,
                   sizeof interleaved_digest) == 0);
  for (stream = 0u; stream < 2u; stream++) {
    t64_machine *interleaved = machines[stream];
    t64_machine *reference = machines[stream + 2u];
    for (sample = 0u; sample < 2u; sample++) {
      T64_CHECK(copy_audio_packet(reference,
                                  &reference_info[stream][sample],
                                  reference_bytes[stream][sample]));
      if (stream == 1u || sample == 1u) {
        T64_CHECK(copy_audio_packet(interleaved,
                                    &interleaved_info[stream][sample],
                                    interleaved_bytes[stream][sample]));
      }
      T64_CHECK(memcmp(&interleaved_info[stream][sample],
                       &reference_info[stream][sample],
                       sizeof reference_info[stream][sample]) == 0);
      T64_CHECK(memcmp(interleaved_bytes[stream][sample],
                       reference_bytes[stream][sample], 8u) == 0);
    }
  }
  for (stream = 0u; stream < 4u; stream++) t64_machine_destroy(machines[stream]);
}

static void case_equal_tick_ai_vi_order(void) {
  run_vi_ai_tie_order(true);
  run_vi_ai_tie_order(false);
}

static bool find_pending_ai_sample(const t64_machine *machine,
                                   t64_event *event_out) {
  size_t offset;
  if (machine == NULL || event_out == NULL) return false;
  for (offset = 0u; offset < machine->queue_count; offset++) {
    const size_t index =
        (machine->queue_head + offset) % T64_EVENT_QUEUE_CAPACITY;
    const t64_event *event = &machine->events[index];
    if (event->device == T64_AI_DEVICE &&
        event->kind == T64_AI_EVENT_SAMPLE &&
        event->token == machine->ai.event_token) {
      *event_out = *event;
      return true;
    }
  }
  return false;
}

static bool find_pending_vi_field(const t64_machine *machine,
                                  t64_event *event_out) {
  size_t offset;
  if (machine == NULL || event_out == NULL) return false;
  for (offset = 0u; offset < machine->queue_count; offset++) {
    const size_t index =
        (machine->queue_head + offset) % T64_EVENT_QUEUE_CAPACITY;
    const t64_event *event = &machine->events[index];
    if (event->device == T64_VI_DEVICE &&
        event->kind == T64_VI_EVENT_FIELD &&
        event->token == machine->vi.event_generation) {
      *event_out = *event;
      return true;
    }
  }
  return false;
}

static bool queue_front_matches(const t64_machine *machine,
                                const t64_event *expected) {
  const t64_event *front;
  if (machine == NULL || expected == NULL || machine->queue_count == 0u)
    return false;
  front = &machine->events[machine->queue_head];
  return front->device == expected->device &&
         front->kind == expected->kind && front->tick == expected->tick &&
         front->sequence == expected->sequence &&
         front->token == expected->token;
}

static void case_independent_audio_video_full(void) {
  static const uint8_t pcm[24] = {
      0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u, 0x08u,
      0x11u, 0x12u, 0x13u, 0x14u, 0x15u, 0x16u, 0x17u, 0x18u,
      0x21u, 0x22u, 0x23u, 0x24u, 0x25u, 0x26u, 0x27u, 0x28u};
  uint8_t assets[2][IPL3_ASSET_BYTES];
  uint8_t audio_copy[8];
  uint8_t video_copy[64];
  t64_machine *audio_full =
      create_admitted_profile(AI_PROFILE_ID, assets[0]);
  t64_machine *video_full =
      create_admitted_profile(AI_PROFILE_ID, assets[1]);
  t64_event blocked_audio_event;
  t64_event next_vi_before_audio_block;
  t64_event blocked_video_event;
  t64_event next_audio_before_video_block;
  t64_event pending;
  t64_audio_packet_info audio_info;
  t64_video_packet_info video_info;
  t64_stop_request request;
  t64_stop_result stop = {0u, 0u};
  uint64_t origin = 0u;
  uint64_t next_audio_tick = UINT64_MAX;
  uint64_t audio_sample_before = 0u;
  size_t copied = 0u;
  uint32_t remaining = UINT32_MAX;
  uint32_t video_field_before = 0u;
  uint32_t sample;
  uint32_t attempt;

  T64_CHECK(audio_full != NULL);
  T64_CHECK(video_full != NULL);
  if (audio_full == NULL || video_full == NULL) {
    if (audio_full != NULL) t64_machine_destroy(audio_full);
    if (video_full != NULL) t64_machine_destroy(video_full);
    return;
  }

  if (!configure_ai_profile_vi(audio_full)) {
    t64_machine_destroy(audio_full);
    t64_machine_destroy(video_full);
    return;
  }
  if (!prepare_two_descriptor_stream(audio_full, pcm, 131u, 1u, &origin)) {
    t64_machine_destroy(audio_full);
    t64_machine_destroy(video_full);
    return;
  }
  (void)memcpy(audio_full->ram + UINT32_C(0x110), pcm + 16u, 8u);
  for (sample = 0u; sample < 4u; sample++)
    T64_CHECK(advance_ai_sample(audio_full));
  T64_CHECK_INT(audio_full->ai.packet_count, T64_AUDIO_QUEUE_CAPACITY);
  T64_CHECK_INT(audio_full->vi.packet_count, 0u);
  audio_sample_before = audio_full->ai.sample_index;
  T64_CHECK(submit_ai_frames(audio_full, UINT32_C(0x110), 2u));
  T64_CHECK_INT(t64_machine_bus_read(audio_full, AI_LEN, 4u, &remaining),
                T64_STATUS_OK);
  T64_CHECK_INT(remaining, 8u);
  if (!find_pending_ai_sample(audio_full, &blocked_audio_event) ||
      !find_pending_vi_field(audio_full, &next_vi_before_audio_block)) {
    T64_CHECK(false);
    t64_machine_destroy(audio_full);
    t64_machine_destroy(video_full);
    return;
  }
  T64_CHECK_INT(next_vi_before_audio_block.tick > blocked_audio_event.tick,
                1);
  request = (t64_stop_request){T64_STOP_GUEST_TICK,
                               blocked_audio_event.tick};
  T64_CHECK_INT(t64_machine_advance(audio_full, &request, &stop),
                T64_STATUS_OK);
  T64_CHECK_INT(stop.reason, T64_STOP_REASON_RESOURCE_LIMIT);
  T64_CHECK(audio_full->tick < blocked_audio_event.tick);
  T64_CHECK_INT(audio_full->ai.sample_index, audio_sample_before);
  T64_CHECK_INT(audio_full->vi.packet_count, 0u);
  T64_CHECK(queue_front_matches(audio_full, &blocked_audio_event));
  if (!find_pending_ai_sample(audio_full, &pending)) {
    T64_CHECK(false);
    t64_machine_destroy(audio_full);
    t64_machine_destroy(video_full);
    return;
  }
  T64_CHECK_INT(pending.tick, blocked_audio_event.tick);
  T64_CHECK_INT(pending.sequence, blocked_audio_event.sequence);
  T64_CHECK_INT(pending.token, blocked_audio_event.token);
  if (!find_pending_vi_field(audio_full, &pending)) {
    T64_CHECK(false);
    t64_machine_destroy(audio_full);
    t64_machine_destroy(video_full);
    return;
  }
  T64_CHECK_INT(pending.tick, next_vi_before_audio_block.tick);
  T64_CHECK_INT(pending.sequence, next_vi_before_audio_block.sequence);
  T64_CHECK_INT(pending.token, next_vi_before_audio_block.token);
  T64_CHECK_INT(t64_audio_info(audio_full, &audio_info), T64_STATUS_OK);
  T64_CHECK_INT(t64_audio_copy(audio_full, audio_copy, sizeof audio_copy,
                               &copied),
                T64_STATUS_OK);
  T64_CHECK_INT(copied, sizeof audio_copy);
  T64_CHECK_INT(audio_full->tick, stop.tick);
  T64_CHECK_INT(audio_full->vi.packet_count, 0u);
  stop.reason = 0u;
  stop.tick = 0u;
  T64_CHECK_INT(t64_machine_advance(audio_full, &request, &stop),
                T64_STATUS_OK);
  T64_CHECK_INT(stop.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(audio_full->tick, blocked_audio_event.tick);
  T64_CHECK_INT(audio_full->ai.sample_index, audio_sample_before + 1u);
  T64_CHECK_INT(audio_full->vi.packet_count, 0u);
  if (!find_pending_vi_field(audio_full, &pending)) {
    T64_CHECK(false);
    t64_machine_destroy(audio_full);
    t64_machine_destroy(video_full);
    return;
  }
  T64_CHECK_INT(pending.tick, next_vi_before_audio_block.tick);
  T64_CHECK_INT(pending.sequence, next_vi_before_audio_block.sequence);
  T64_CHECK_INT(pending.token, next_vi_before_audio_block.token);

  if (!configure_ai_profile_vi_sync(video_full, 8u, 8u)) {
    T64_CHECK(false);
    t64_machine_destroy(audio_full);
    t64_machine_destroy(video_full);
    return;
  }
  if (!prepare_two_descriptor_stream(video_full, pcm, UINT32_C(16383), 1u,
                                     &origin)) {
    t64_machine_destroy(audio_full);
    t64_machine_destroy(video_full);
    return;
  }
  T64_CHECK_INT(video_full->ai.packet_count, 0u);
  if (!find_next_ai_sample_tick(video_full, &next_audio_tick) ||
      !find_pending_ai_sample(video_full, &next_audio_before_video_block)) {
    T64_CHECK(false);
    t64_machine_destroy(audio_full);
    t64_machine_destroy(video_full);
    return;
  }
  for (attempt = 0u;
       attempt < 128u &&
       video_full->vi.packet_count < T64_VIDEO_QUEUE_CAPACITY;
       attempt++) {
    uint64_t next_vi_tick = UINT64_MAX;
    if (!find_next_vi_field_tick(video_full, &next_vi_tick)) {
      T64_CHECK(false);
      break;
    }
    if (!advance_guest_to_in_slices(video_full, next_vi_tick)) break;
    T64_CHECK(video_full->vi.packet_count <= T64_VIDEO_QUEUE_CAPACITY);
  }
  T64_CHECK(video_full->vi.packet_count == T64_VIDEO_QUEUE_CAPACITY);
  if (video_full->vi.packet_count != T64_VIDEO_QUEUE_CAPACITY) {
    t64_machine_destroy(audio_full);
    t64_machine_destroy(video_full);
    return;
  }
  T64_CHECK_INT(next_audio_tick > video_full->tick, 1);
  T64_CHECK_INT(video_full->ai.packet_count, 0u);
  {
    bool blocked = false;
    for (attempt = 0u; attempt < 32u; attempt++) {
      const uint32_t before_field = video_full->vi.field;
      if (!find_pending_vi_field(video_full, &pending)) {
        T64_CHECK(false);
        break;
      }
      request = (t64_stop_request){T64_STOP_GUEST_TICK, pending.tick};
      stop.reason = 0u;
      stop.tick = 0u;
      T64_CHECK_INT(t64_machine_advance(video_full, &request, &stop),
                    T64_STATUS_OK);
      if (stop.reason == T64_STOP_REASON_RESOURCE_LIMIT &&
          video_full->vi.packet_count == T64_VIDEO_QUEUE_CAPACITY) {
        blocked_video_event = pending;
        video_field_before = before_field;
        blocked = true;
        break;
      }
      T64_CHECK_INT(stop.reason, T64_STOP_REASON_BOUND);
    }
    T64_CHECK(blocked);
    if (!blocked) {
      t64_machine_destroy(audio_full);
      t64_machine_destroy(video_full);
      return;
    }
  }
  T64_CHECK(video_full->tick < blocked_video_event.tick);
  T64_CHECK_INT(video_full->vi.field, video_field_before);
  T64_CHECK_INT(video_full->ai.packet_count, 0u);
  T64_CHECK(queue_front_matches(video_full, &blocked_video_event));
  if (!find_pending_vi_field(video_full, &pending)) {
    T64_CHECK(false);
    t64_machine_destroy(audio_full);
    t64_machine_destroy(video_full);
    return;
  }
  T64_CHECK_INT(pending.tick, blocked_video_event.tick);
  T64_CHECK_INT(pending.sequence, blocked_video_event.sequence);
  T64_CHECK_INT(pending.token, blocked_video_event.token);
  if (!find_pending_ai_sample(video_full, &pending)) {
    T64_CHECK(false);
    t64_machine_destroy(audio_full);
    t64_machine_destroy(video_full);
    return;
  }
  T64_CHECK_INT(pending.tick, next_audio_before_video_block.tick);
  T64_CHECK_INT(pending.sequence, next_audio_before_video_block.sequence);
  T64_CHECK_INT(pending.token, next_audio_before_video_block.token);
  T64_CHECK_INT(t64_video_info(video_full, &video_info), T64_STATUS_OK);
  T64_CHECK(video_info.packet_bytes > 0u);
  T64_CHECK(video_info.packet_bytes <= sizeof video_copy);
  T64_CHECK_INT(t64_video_copy(video_full, video_copy, sizeof video_copy,
                               &copied),
                T64_STATUS_OK);
  T64_CHECK_INT(copied, video_info.packet_bytes);
  T64_CHECK_INT(video_full->tick, stop.tick);
  T64_CHECK_INT(video_full->ai.packet_count, 0u);
  request.value = blocked_video_event.tick;
  stop.reason = 0u;
  stop.tick = 0u;
  T64_CHECK_INT(t64_machine_advance(video_full, &request, &stop),
                T64_STATUS_OK);
  T64_CHECK_INT(stop.reason, T64_STOP_REASON_BOUND);
  T64_CHECK_INT(video_full->tick, blocked_video_event.tick);
  T64_CHECK_INT(video_full->vi.field, video_field_before + 1u);
  T64_CHECK_INT(video_full->vi.packet_count, T64_VIDEO_QUEUE_CAPACITY);
  T64_CHECK_INT(video_full->ai.packet_count, 0u);
  if (!find_pending_ai_sample(video_full, &pending)) {
    T64_CHECK(false);
    t64_machine_destroy(audio_full);
    t64_machine_destroy(video_full);
    return;
  }
  T64_CHECK_INT(pending.tick, next_audio_before_video_block.tick);
  T64_CHECK_INT(pending.sequence, next_audio_before_video_block.sequence);
  T64_CHECK_INT(pending.token, next_audio_before_video_block.token);

  t64_machine_destroy(audio_full);
  t64_machine_destroy(video_full);
}

static void case_determinism_suite(void) {
  puts("case whole-versus-split");
  case_whole_versus_split();
  puts("case interleaved-machine-isolation");
  case_interleaved_machine_isolation();
  puts("case equal-tick-ai-vi-order");
  case_equal_tick_ai_vi_order();
  puts("case independent-audio-video-full");
  case_independent_audio_video_full();
}

static void case_cadence_suite(void) {
  puts("case rational-before-on-after");
  case_rational_before_on_after();
  puts("case partial-length-readback");
  case_partial_length_readback();
  puts("case inflight-source-mutation");
  case_inflight_source_mutation();
  puts("case bitrate-zero-and-ratio");
  case_bitrate_zero_and_ratio();
  puts("case active-rate-latch");
  case_active_rate_latch();
  puts("case pause-phase-resume");
  case_pause_phase_resume();
  puts("case reset-owned-events");
  case_reset_owned_events();
  puts("case same-tick-store-sample");
  case_same_tick_store_sample();
  puts("case arithmetic-overflow-refusal");
  case_arithmetic_overflow_refusal();
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
    puts("rational-before-on-after");
    puts("partial-length-readback");
    puts("inflight-source-mutation");
    puts("bitrate-zero-and-ratio");
    puts("active-rate-latch");
    puts("pause-phase-resume");
    puts("reset-owned-events");
    puts("same-tick-store-sample");
    puts("arithmetic-overflow-refusal");
    puts("audio-info-copy-atomic");
    puts("reserve-before-first-sample");
    puts("output-full-same-event");
    puts("drain-resume-exactly-once");
    puts("copy-alias-and-overflow");
    puts("canonical-producer-history");
    puts("whole-versus-split");
    puts("interleaved-machine-isolation");
    puts("equal-tick-ai-vi-order");
    puts("independent-audio-video-full");
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--suite") == 0 &&
      strcmp(argv[2], "registers") == 0) {
    case_register_suite();
    return T64_TEST_FINISH("unit.core.ai.registers");
  }
  if (argc == 3 && strcmp(argv[1], "--suite") == 0 &&
      strcmp(argv[2], "cadence") == 0) {
    case_cadence_suite();
    return T64_TEST_FINISH("unit.core.ai.cadence");
  }
  if (argc == 3 && strcmp(argv[1], "--suite") == 0 &&
      strcmp(argv[2], "output") == 0) {
    case_output_suite();
    return T64_TEST_FINISH("unit.core.ai.output");
  }
  if (argc == 3 && strcmp(argv[1], "--suite") == 0 &&
      strcmp(argv[2], "determinism") == 0) {
    case_determinism_suite();
    return T64_TEST_FINISH("unit.core.ai.determinism");
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
    if (strcmp(argv[2], "active-rate-latch") == 0) {
      case_active_rate_latch();
      return T64_TEST_FINISH("unit.core.ai.cadence");
    }
    if (strcmp(argv[2], "rational-before-on-after") == 0) {
      case_rational_before_on_after();
      return T64_TEST_FINISH("unit.core.ai.cadence");
    }
    if (strcmp(argv[2], "partial-length-readback") == 0) {
      case_partial_length_readback();
      return T64_TEST_FINISH("unit.core.ai.cadence");
    }
    if (strcmp(argv[2], "inflight-source-mutation") == 0) {
      case_inflight_source_mutation();
      return T64_TEST_FINISH("unit.core.ai.cadence");
    }
    if (strcmp(argv[2], "bitrate-zero-and-ratio") == 0) {
      case_bitrate_zero_and_ratio();
      return T64_TEST_FINISH("unit.core.ai.cadence");
    }
    if (strcmp(argv[2], "pause-phase-resume") == 0) {
      case_pause_phase_resume();
      return T64_TEST_FINISH("unit.core.ai.cadence");
    }
    if (strcmp(argv[2], "reset-owned-events") == 0) {
      case_reset_owned_events();
      return T64_TEST_FINISH("unit.core.ai.cadence");
    }
    if (strcmp(argv[2], "same-tick-store-sample") == 0) {
      case_same_tick_store_sample();
      return T64_TEST_FINISH("unit.core.ai.cadence");
    }
    if (strcmp(argv[2], "arithmetic-overflow-refusal") == 0) {
      case_arithmetic_overflow_refusal();
      return T64_TEST_FINISH("unit.core.ai.cadence");
    }
    if (strcmp(argv[2], "audio-info-copy-atomic") == 0) {
      case_audio_info_copy_atomic();
      return T64_TEST_FINISH("unit.core.ai.output");
    }
    if (strcmp(argv[2], "reserve-before-first-sample") == 0) {
      case_reserve_before_first_sample();
      return T64_TEST_FINISH("unit.core.ai.output");
    }
    if (strcmp(argv[2], "output-full-same-event") == 0) {
      case_output_full_same_event();
      return T64_TEST_FINISH("unit.core.ai.output");
    }
    if (strcmp(argv[2], "drain-resume-exactly-once") == 0) {
      case_drain_resume_exactly_once();
      return T64_TEST_FINISH("unit.core.ai.output");
    }
    if (strcmp(argv[2], "copy-alias-and-overflow") == 0) {
      case_copy_alias_and_overflow();
      return T64_TEST_FINISH("unit.core.ai.output");
    }
    if (strcmp(argv[2], "canonical-producer-history") == 0) {
      case_canonical_producer_history();
      return T64_TEST_FINISH("unit.core.ai.output");
    }
    if (strcmp(argv[2], "whole-versus-split") == 0) {
      case_whole_versus_split();
      return T64_TEST_FINISH("unit.core.ai.determinism");
    }
    if (strcmp(argv[2], "independent-audio-video-full") == 0) {
      case_independent_audio_video_full();
      return T64_TEST_FINISH("unit.core.ai.determinism");
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
