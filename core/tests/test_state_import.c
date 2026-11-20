/*
 * State import tests (plan 09-09).
 *
 * The importer is exercised on authored machines of the composite family only:
 * every blob is produced in memory by the exporter of plan 09-08 from authored
 * inputs or the already cleared openIPL3 asset, and no test reads or writes a
 * file. The container offsets used here are literal copies of the reference
 * decoder of core/tests/test_state_export.c, so a layout mistake in the
 * production code cannot be hidden by a matching mistake in these tests. The
 * hashes inside a container are integrity checks, not authentication; the
 * contract under test is memory safety and liveness, and no case claims that an
 * imported state is reachable or hardware accurate.
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
#define SI_STEM_ID "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1"

/* ---- test allocator with counters (the shape of core/tests/test_profile.c) */

static size_t allocate_calls;
static size_t release_calls;

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
  allocate_calls++;
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = alignment;
  return true;
}

static void test_release(void *context, t64_host_storage storage) {
  (void)context;
  release_calls++;
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

#define ADVANCE_SLICE_TICKS 2000u

/* One absolute guest-tick bound; a long distance is walked in slices because
 * every CPU step is one dispatched event and the profile caps the events of one
 * advance call. */
static bool advance_to(t64_machine *machine, uint64_t tick) {
  while (machine->tick < tick) {
    const uint64_t step = tick - machine->tick > ADVANCE_SLICE_TICKS
                              ? machine->tick + ADVANCE_SLICE_TICKS
                              : tick;
    t64_stop_request request = {T64_STOP_GUEST_TICK, step};
    t64_stop_result stop = {0u, 0u};
    const t64_status status = t64_machine_advance(machine, &request, &stop);

    T64_CHECK_INT(status, T64_STATUS_OK);
    T64_CHECK_INT(stop.reason, T64_STOP_REASON_BOUND);
    if (status != T64_STATUS_OK || stop.reason != T64_STOP_REASON_BOUND) {
      return false;
    }
  }
  return machine->tick == tick;
}

static bool digest_of(const t64_machine *machine,
                      uint8_t digest[T64_DIGEST_SIZE]) {
  const t64_status status = t64_machine_digest(machine, digest);

  T64_CHECK_INT(status, T64_STATUS_OK);
  return status == T64_STATUS_OK;
}

/* ---- the S1 state: an SI RD64B in flight with its execute still pending --- */

#define SI_DRAM_ADDR UINT32_C(0x04800000)
#define SI_RD64B UINT32_C(0x04800004)
#define SI_WR64B UINT32_C(0x04800010)
#define PIF_ADDRESS UINT32_C(0x1FC007C0)

/* The 09-03 tracer-poll frame: channel 0 and channel 1 each carry command 0x01
 * expecting four bytes, then the end escape, and byte 63 requests a parse. */
static const uint8_t poll_frame[64] = {
    0x01, 0x04, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
    0x01, 0x04, 0x01, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFE, [63] = 0x01};

static void bus_write_bytes(t64_machine *machine, uint32_t address,
                            const uint8_t *bytes, size_t count) {
  size_t index;

  for (index = 0u; index < count; index++) {
    T64_CHECK_INT(t64_machine_bus_write(machine, address + (uint32_t)index,
                                        1u, bytes[index]),
                  T64_STATUS_OK);
  }
}

static size_t count_events(const t64_machine *machine, uint32_t device,
                           uint32_t kind) {
  size_t count = 0u;
  size_t i;

  for (i = 0u; i < machine->queue_count; i++) {
    const t64_event *event =
        &machine->events[(machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY];

    if (event->device == device && event->kind == kind) count++;
  }
  return count;
}

/* Admit, run the boot code to tick 2,000, write the poll frame, start WR64B,
 * run to 5,000, start RD64B at 0x3900 and run 8,000 more ticks: the read is
 * active and its execute event is still pending. */
static t64_machine *build_s1(const char *profile_id) {
  t64_machine *machine = create_admitted(profile_id);

  if (machine == NULL) return NULL;
  T64_CHECK(advance_to(machine, 2000u));
  bus_write_bytes(machine, UINT32_C(0x3800), poll_frame, 64u);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_DRAM_ADDR, 4u, 0x3800u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_WR64B, 4u, PIF_ADDRESS),
                T64_STATUS_OK);
  T64_CHECK(advance_to(machine, 5000u));
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_DRAM_ADDR, 4u, 0x3900u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, SI_RD64B, 4u, PIF_ADDRESS),
                T64_STATUS_OK);
  T64_CHECK(advance_to(machine, 13000u));
  /* Reached: the read is in flight with the execute event pending, both SI
   * events and one CPU step event are queued. */
  T64_CHECK(machine->si.active_valid);
  T64_CHECK(machine->si.execute_pending);
  T64_CHECK_INT(machine->si.active_direction, T64_SI_DIRECTION_RD64B);
  T64_CHECK_INT(count_events(machine, T64_SI_DEVICE, T64_SI_EVENT_EXECUTE),
                1u);
  T64_CHECK_INT(count_events(machine, T64_SI_DEVICE, T64_SI_EVENT_COMPLETE),
                1u);
  T64_CHECK_INT(count_events(machine, T64_DEVICE_CPU, T64_CPU_EVENT_STEP), 1u);
  return machine;
}

/* ---- container access with literal offsets (reference decoder values) ---- */

#define D_TOTAL 0x010u
#define D_COUNT 0x018u
#define D_TICK 0x068u
#define D_DECLARED 0x070u

static uint32_t rd32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint64_t rd64(const uint8_t *p) {
  return ((uint64_t)rd32(p) << 32) | (uint64_t)rd32(p + 4);
}

/* One export by size query then copy-out; the caller frees the blob. */
static uint8_t *export_blob(const t64_machine *machine, size_t *length_out) {
  size_t required = 0u;
  size_t written = 0u;
  uint8_t *blob;
  t64_status status;

  *length_out = 0u;
  status = t64_machine_state_export(machine, NULL, 0u, &required);
  T64_CHECK_INT(status, T64_STATUS_BUFFER_TOO_SMALL);
  T64_CHECK(required > 0u);
  if (status != T64_STATUS_BUFFER_TOO_SMALL || required == 0u) return NULL;
  blob = (uint8_t *)malloc(required);
  if (blob == NULL) {
    T64_CHECK(false);
    return NULL;
  }
  status = t64_machine_state_export(machine, blob, required, &written);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(written, required);
  if (status != T64_STATUS_OK || written != required) {
    free(blob);
    return NULL;
  }
  *length_out = written;
  return blob;
}

static bool queues_equal(const t64_machine *left, const t64_machine *right) {
  size_t i;

  if (left->queue_count != right->queue_count) return false;
  for (i = 0u; i < left->queue_count; i++) {
    const t64_event *a =
        &left->events[(left->queue_head + i) % T64_EVENT_QUEUE_CAPACITY];
    const t64_event *b =
        &right->events[(right->queue_head + i) % T64_EVENT_QUEUE_CAPACITY];

    if (a->tick != b->tick || a->sequence != b->sequence ||
        a->device != b->device || a->kind != b->kind || a->value != b->value ||
        a->token != b->token) {
      return false;
    }
  }
  return true;
}

/* The ten production handlers by (device, kind), written out independently of
 * the importer's own table. */
static t64_event_handler_fn expected_handler(uint32_t device, uint32_t kind) {
  if (device == T64_DEVICE_CPU && kind == T64_CPU_EVENT_STEP) {
    return t64_cpu_step;
  }
  if (device == T64_DEVICE_PI && kind == T64_PI_EVENT_COMPLETE) {
    return t64_pi_complete;
  }
  if (device == T64_DEVICE_SP && kind == T64_SP_EVENT_COMPLETE) {
    return t64_sp_complete;
  }
  if (device == T64_DEVICE_SP && kind == T64_SP_EVENT_RSP_ISSUE) {
    return t64_rsp_issue_event_handler();
  }
  if (device == T64_RDP_DEVICE && kind == T64_RDP_EVENT_WORK) {
    return t64_rdp_event;
  }
  if (device == T64_VI_DEVICE && kind == T64_VI_EVENT_FIELD) {
    return t64_vi_event;
  }
  if (device == T64_AI_DEVICE && kind == T64_AI_EVENT_SAMPLE) {
    return t64_ai_event;
  }
  if (device == T64_TRACE_DEVICE_INPUT &&
      kind == T64_TRACE_KIND_INPUT_SUBMITTED) {
    return t64_input_event_handler;
  }
  if (device == T64_SI_DEVICE && kind == T64_SI_EVENT_EXECUTE) {
    return t64_si_execute_event;
  }
  if (device == T64_SI_DEVICE && kind == T64_SI_EVENT_COMPLETE) {
    return t64_si_complete_event;
  }
  return NULL;
}

static bool handlers_match_production(const t64_machine *machine) {
  size_t i;

  for (i = 0u; i < machine->queue_count; i++) {
    const t64_event *event =
        &machine->events[(machine->queue_head + i) % T64_EVENT_QUEUE_CAPACITY];
    const t64_event_handler_fn want =
        expected_handler(event->device, event->kind);

    if (want == NULL || event->handler != want) return false;
  }
  return true;
}

static void print_digest(const char *label, const uint8_t digest[T64_DIGEST_SIZE]) {
  size_t i;

  fprintf(stderr, "%s: ", label);
  for (i = 0u; i < T64_DIGEST_SIZE; i++) fprintf(stderr, "%02x", digest[i]);
  fputc('\n', stderr);
}

/* ---- case roundtrip (T1) --------------------------------------------- */

static void case_roundtrip(void) {
  t64_machine *source = build_s1(SI_STEM_ID);
  t64_machine *peer;
  t64_state_import_report report;
  uint8_t source_digest[T64_DIGEST_SIZE];
  uint8_t peer_digest[T64_DIGEST_SIZE];
  uint8_t *blob;
  size_t length = 0u;
  size_t allocate_before;
  size_t release_before;
  t64_status status;

  if (source == NULL) return;
  blob = export_blob(source, &length);
  if (blob == NULL) {
    t64_machine_destroy(source);
    return;
  }
  (void)digest_of(source, source_digest);
  peer = create_admitted(SI_STEM_ID);
  if (peer == NULL) {
    free(blob);
    t64_machine_destroy(source);
    return;
  }
  T64_CHECK_INT(t64_machine_trace_set_enabled(peer, true), T64_STATUS_OK);
  allocate_before = allocate_calls;
  release_before = release_calls;
  (void)memset(&report, 0xA5, sizeof report);
  status = t64_machine_state_import(peer, blob, length, 0u, &report);
  /* The RED record of plan 09-09 task 1 is the pair printed here. */
  fprintf(stderr, "roundtrip import: status=%u reason=%u section=%u rule=%u\n",
          (unsigned)status, (unsigned)report.reason,
          (unsigned)report.section_id, (unsigned)report.rule);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(report.status, T64_STATUS_OK);
  T64_CHECK_INT(report.reason, T64_STATE_REASON_NONE);
  (void)digest_of(peer, peer_digest);
  if (memcmp(source_digest, peer_digest, sizeof source_digest) != 0) {
    print_digest("source digest", source_digest);
    print_digest("peer digest  ", peer_digest);
  }
  T64_CHECK(memcmp(source_digest, peer_digest, sizeof source_digest) == 0);
  T64_CHECK(memcmp(blob + D_DECLARED, peer_digest, sizeof peer_digest) == 0);
  T64_CHECK_INT(rd64(blob + D_TOTAL), length);
  T64_CHECK_INT(rd64(blob + D_TICK), peer->tick);
  T64_CHECK(queues_equal(source, peer));
  T64_CHECK(handlers_match_production(peer));
  T64_CHECK_INT(peer->queue_head, 0u);
  T64_CHECK_INT(peer->trace_count, 0u);
  T64_CHECK(peer->trace_enabled);
  T64_CHECK(t64_state_summary_is_clear(peer));
  T64_CHECK_INT(allocate_calls, allocate_before);
  T64_CHECK_INT(release_calls, release_before);
  free(blob);
  t64_machine_destroy(peer);
  t64_machine_destroy(source);
}

/* ---- dispatch ---------------------------------------------------------- */

int main(int argc, char **argv) {
  static const char *const cases[] = {"roundtrip"};
  static const char *const suites[] = {"import"};
  static void (*const runners[])(void) = {case_roundtrip};
  const size_t count = sizeof cases / sizeof cases[0];
  size_t index;

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (index = 0u; index < count; index++) printf("%s\n", cases[index]);
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    for (index = 0u; index < count; index++) {
      if (strcmp(argv[2], cases[index]) == 0) {
        char name[96];

        runners[index]();
        (void)snprintf(name, sizeof name, "unit.core.state.import.%s",
                       cases[index]);
        return T64_TEST_FINISH(name);
      }
    }
    fprintf(stderr, "unknown case: %s\n", argv[2]);
    return 2;
  }
  if (argc == 3 && strcmp(argv[1], "--suite") == 0) {
    bool found = false;
    char name[96];

    for (index = 0u; index < count; index++) {
      if (strcmp(argv[2], suites[index]) == 0) {
        runners[index]();
        found = true;
      }
    }
    if (!found) {
      fprintf(stderr, "unknown suite: %s\n", argv[2]);
      return 2;
    }
    (void)snprintf(name, sizeof name, "unit.core.state.import.%s", argv[2]);
    return T64_TEST_FINISH(name);
  }
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--list-cases|--case NAME|--suite NAME]\n",
            argv[0]);
    return 2;
  }
  for (index = 0u; index < count; index++) runners[index]();
  return T64_TEST_FINISH("unit.core.state.import");
}
