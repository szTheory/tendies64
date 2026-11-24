/*
 * K1 characterization of Z refusal on the shipped composite ids (plan 10-02).
 *
 * Phase 10 adds depth only under new registered ids. These cases pin, before
 * any edit of core/t64_internal.h, that the shipped descriptor 11 id and the
 * four descriptor 12 ids refuse every depth command exactly as they do today:
 *
 *   dispatch-refusal: SetZImage (0x3e) and SetPrimDepth (0x2e) are refused
 *       when the command is dispatched (supported_opcode is false).
 *   apply-refusal:    the four Z triangle opcodes (0x09, 0x0b, 0x0d, 0x0f)
 *       pass dispatch and are refused when the command is applied.
 *
 * Each refusal records the status register, the refusal reason, the last
 * opcode field, both DPC counters, the untouched depth members and an
 * all-zero hidden depth table. The command words are copied from the bound
 * core/tests/test_rdp_depth.c; that file is never edited.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_internal.h"
#include "t64_sha256.h"
#include "t64_test.h"

#define IPL3_ASSET_BYTES 4096u
#define COMMAND_ADDRESS UINT32_C(0x100)
#define DP_START UINT32_C(0x04100000)
#define DP_END (DP_START + UINT32_C(4))
#define SI_STEM "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1"

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

/* Create the machine for an id and admit the cleared openIPL3 asset. */
static t64_machine *create_admitted(const char *profile_id) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t asset[IPL3_ASSET_BYTES];
  t64_status status;

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
  status = t64_machine_admit_rom(machine, asset, sizeof asset);
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) {
    t64_machine_destroy(machine);
    return NULL;
  }
  return machine;
}

static void write_be64(uint8_t *bytes, uint64_t value) {
  uint32_t i;
  for (i = 0u; i < 8u; i++)
    bytes[i] = (uint8_t)(value >> (56u - i * 8u));
}

/* Same event pump as core/tests/test_rdp_depth.c dispatch_all. */
static bool dispatch_all(t64_machine *machine) {
  size_t step;

  for (step = 0u; step < 4096u && machine->queue_count != 0u; step++) {
    t64_event event = machine->events[machine->queue_head];
    machine->queue_head = (machine->queue_head + 1u) %
                          T64_EVENT_QUEUE_CAPACITY;
    machine->queue_count--;
    machine->tick = event.tick;
    T64_CHECK(event.handler != NULL);
    if (event.handler != NULL)
      T64_CHECK_INT(event.handler(machine, &event), T64_STATUS_OK);
  }
  T64_CHECK_INT(machine->queue_count, 0u);
  return machine->queue_count == 0u;
}

/* Same submission as core/tests/test_rdp_depth.c run_commands. */
static bool run_commands(t64_machine *machine, const uint64_t *words,
                         size_t count) {
  size_t i;
  uint32_t end;

  if (machine == NULL || words == NULL || count == 0u ||
      count > (UINT32_MAX - COMMAND_ADDRESS) / 8u)
    return false;
  machine->queue_head = 0u;
  machine->queue_count = 0u;
  for (i = 0u; i < count; i++)
    write_be64(machine->ram + COMMAND_ADDRESS + i * 8u, words[i]);
  end = COMMAND_ADDRESS + (uint32_t)(count * 8u);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_START, 4u,
                                      COMMAND_ADDRESS), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_END, 4u, end),
                T64_STATUS_OK);
  return dispatch_all(machine);
}

/* Words common to every K1 submission, copied from test_rdp_depth.c. */
static const uint64_t setup_words[] = {
    UINT64_C(0x3f10000700005000), /* RGBA16 color image, width 8 */
    UINT64_C(0x2d00000000020020), /* scissor [0,8) x [0,8) */
    UINT64_C(0x2f00000000000034), /* primitive Z, compare, update */
};

/* Increments the dispatch path makes for one refused command: one clock count
 * and one buffer-busy count (core/t64_rdp.c, the unsupported opcode branch of
 * t64_rdp_event). */
#define DISPATCH_REFUSAL_CLOCK_INCREMENT 1u
#define DISPATCH_REFUSAL_BUSY_INCREMENT 1u

/* Every depth member is as a machine that never saw a depth command. */
static void check_depth_untouched(const t64_machine *machine) {
  size_t i;
  size_t bytes = T64_RDP_DEPTH_HIDDEN_BYTES(machine->profile.rdram_bytes);

  T64_CHECK(!machine->rdp.depth_image_valid);
  T64_CHECK(!machine->rdp.primitive_depth_valid);
  T64_CHECK_INT(machine->rdp.depth_image_address, 0u);
  T64_CHECK_INT(machine->rdp.primitive_z, 0u);
  T64_CHECK_INT(machine->rdp.primitive_dz, 0u);
  T64_CHECK(machine->rdp_depth_hidden != NULL);
  if (machine->rdp_depth_hidden == NULL) return;
  for (i = 0u; i < bytes; i++) {
    if (machine->rdp_depth_hidden[i] != 0u) {
      T64_CHECK_INT(machine->rdp_depth_hidden[i], 0u);
      break;
    }
  }
}

static unsigned refusals_checked;

/* One SetZImage or SetPrimDepth word refused at dispatch on one id. */
static void check_dispatch_refusal(const char *profile_id, uint64_t word) {
  t64_machine *machine = create_admitted(profile_id);
  uint32_t opcode = (uint32_t)((word >> 56) & UINT64_C(0x3f));
  uint32_t clock_before;
  uint32_t busy_before;
  uint64_t commands_before;

  if (machine == NULL) return;
  T64_CHECK(run_commands(machine, setup_words,
                         sizeof setup_words / sizeof setup_words[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  clock_before = machine->rdp.dpc_clock_counter;
  busy_before = machine->rdp.dpc_buffer_busy_counter;
  commands_before = machine->rdp.command_count;
  check_depth_untouched(machine);

  T64_CHECK(run_commands(machine, &word, 1u));
  T64_CHECK_INT(machine->rdp.refusal_reason,
                T64_RDP_REFUSAL_UNSUPPORTED_OPCODE);
  T64_CHECK_INT(machine->rdp.last_opcode_field, opcode);
  T64_CHECK_INT(machine->rdp.last_original_word, word);
  T64_CHECK((machine->rdp.dpc_status & T64_RDP_STATUS_ERROR) != 0u);
  T64_CHECK((machine->rdp.dpc_status & T64_RDP_STATUS_BUSY) == 0u);
  T64_CHECK(!machine->rdp.busy);
  T64_CHECK(!machine->rdp.command_pending);
  T64_CHECK_INT(machine->rdp.dpc_clock_counter,
                clock_before + DISPATCH_REFUSAL_CLOCK_INCREMENT);
  T64_CHECK_INT(machine->rdp.dpc_buffer_busy_counter,
                busy_before + DISPATCH_REFUSAL_BUSY_INCREMENT);
  T64_CHECK_INT(machine->rdp.command_count, commands_before);
  check_depth_untouched(machine);
  refusals_checked++;
  t64_machine_destroy(machine);
}

static void case_dispatch_refusal(void) {
  /* Tracer: one shipped id, SetZImage only. */
  check_dispatch_refusal(SI_STEM, UINT64_C(0x3e00000000006000));
  printf("k1: dispatch-refusal refusals=%u\n", refusals_checked);
}

int main(int argc, char **argv) {
  static const char *const cases[] = {"dispatch-refusal"};

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    size_t index;

    for (index = 0u; index < sizeof cases / sizeof cases[0]; index++) {
      printf("%s\n", cases[index]);
    }
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    if (strcmp(argv[2], "dispatch-refusal") == 0) {
      case_dispatch_refusal();
      return T64_TEST_FINISH("unit.core.k1.dispatch-refusal");
    }
    fprintf(stderr, "unknown case: %s\n", argv[2]);
    return 2;
  }
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--list-cases|--case NAME]\n", argv[0]);
    return 2;
  }
  case_dispatch_refusal();
  return T64_TEST_FINISH("unit.core.k1");
}
