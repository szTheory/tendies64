/*
 * One accepted case and one refused case per public operation, through the
 * installed library (Phase 11, plan 11-18, API-01, VER-05). Every case prints
 * "accepted: ..." and "refused: ..." lines; a refusal asserts the exact status,
 * its class in the consumer's own class table (t64c_classes.h) and, when a
 * machine exists, an unchanged machine digest and untouched sentinel outputs.
 * The cases mirror the documented check orders of the header; none includes a
 * header of the repository's core or tests.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tendies64/t64_machine.h>

#include "t64c_alloc.h"
#include "t64c_cases.h"
#include "t64c_check.h"
#include "t64c_classes.h"

#define T64C_SENTINEL 0xa5u

/* ---- helpers --------------------------------------------------------------- */

static const char *t64c_class_name(t64c_class cls) {
  switch (cls) {
    case T64C_CLASS_OK: return "ok";
    case T64C_CLASS_REFUSED: return "refused";
    case T64C_CLASS_GUEST_STOP: return "guest-stop";
    case T64C_CLASS_DEFECT: return "defect";
    case T64C_CLASS_RESERVED: return "reserved";
    default: return "unknown";
  }
}

/* An accepted call: one line and one check. */
static void accepted(t64c_tally *tally, bool ok, const char *label) {
  tally->checks++;
  if (!ok) {
    tally->failures++;
  }
  (void)printf("accepted: %s%s\n", label, ok ? "" : ": FAILED");
}

/* A refused call: the exact status, its class "refused" in the consumer's own
 * table, and (when a machine is given) an unchanged machine digest; with a NULL
 * `before` the machine has no image and must still have none. */
static void refused(t64c_tally *tally, t64_status actual, t64_status expected,
                    const char *label, t64_machine *machine,
                    const uint8_t before[T64_DIGEST_SIZE]) {
  const t64c_class cls = t64c_status_class(actual);
  bool unchanged = true;
  bool ok = actual == expected && cls == T64C_CLASS_REFUSED;

  if (machine != NULL) {
    uint8_t after[T64_DIGEST_SIZE];

    if (before != NULL) {
      unchanged = t64_machine_digest(machine, after) == T64_STATUS_OK &&
                  memcmp(before, after, sizeof after) == 0;
    } else {
      /* A machine without an image has no digest: it must still answer
       * NOT_READY, which is how "no image was installed" shows. */
      unchanged = t64_machine_digest(machine, after) == T64_STATUS_NOT_READY;
    }
    ok = ok && unchanged;
  }
  tally->checks++;
  if (!ok) {
    tally->failures++;
  }
  (void)printf("refused: %s: status=%u (expected %u) class=%s%s%s\n", label,
               (unsigned)actual, (unsigned)expected, t64c_class_name(cls),
               machine != NULL
                   ? (unchanged ? (before != NULL ? " digest-unchanged" : " still-no-image")
                                : " STATE-CHANGED")
                   : "",
               ok ? "" : " FAILED");
}

/* A refusal whose output bytes must stay as the caller left them. */
static void untouched(t64c_tally *tally, bool same, const char *label) {
  tally->checks++;
  if (!same) {
    tally->failures++;
  }
  (void)printf("refused: %s: outputs untouched%s\n", label, same ? "" : " FAILED");
}

static void poison(void *object, size_t size) {
  (void)memset(object, (int)T64C_SENTINEL, size);
}

static bool all_sentinel(const uint8_t *bytes, size_t length) {
  size_t i;

  for (i = 0u; i < length; i++) {
    if (bytes[i] != T64C_SENTINEL) {
      return false;
    }
  }
  return true;
}

static bool digest_of(t64_machine *machine, uint8_t digest[T64_DIGEST_SIZE]) {
  return t64_machine_digest(machine, digest) == T64_STATUS_OK;
}

/* A fixture with the composite ROM admitted (tick 0). */
static t64c_fixture *open_admitted(const t64c_args *args) {
  t64c_fixture *fixture = t64c_fixture_new(args->rom_path);

  if (fixture != NULL && t64c_fixture_admit(fixture) != T64_STATUS_OK) {
    (void)fprintf(stderr, "t64c: admission of the composite ROM failed\n");
    t64c_fixture_free(fixture);
    return NULL;
  }
  return fixture;
}

/* Destroy the machine of a fixture and report whether the ledger balances. */
static bool close_balanced(t64c_fixture *fixture) {
  bool ok;

  t64_machine_destroy(fixture->machine);
  fixture->machine = NULL;
  ok = fixture->ledger.outstanding == 0u && fixture->ledger.bad_releases == 0u;
  t64c_fixture_free(fixture);
  return ok;
}

/* Advance to a tick through RESOURCE_LIMIT stops; the final stop is BOUND. */
static bool advance_to(t64_machine *machine, uint64_t tick, t64_stop_result *last) {
  unsigned guard;

  for (guard = 0u; guard < 64u; guard++) {
    t64_stop_result stop = T64_STOP_RESULT_INIT;
    t64_status status = t64_machine_advance(
        machine, &(t64_stop_request)T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK, tick),
        &stop);

    if (status != T64_STATUS_OK) {
      return false;
    }
    if (last != NULL) {
      *last = stop;
    }
    if (stop.reason == T64_STOP_REASON_BOUND) {
      return stop.tick == tick;
    }
    if (stop.reason != T64_STOP_REASON_RESOURCE_LIMIT) {
      return false;
    }
  }
  return false;
}

/* ---- ops-lifetime ---------------------------------------------------------- */

/* A host allocator that calls back into the library from inside the callback:
 * the machine is busy, so the call must answer REENTRANT. */
typedef struct {
  t64c_ledger ledger;
  t64_machine *machine;
  t64_status seen;
  unsigned probes;
} t64c_reentrant_context;

static bool t64c_reentrant_allocate(void *context, size_t size, size_t alignment,
                                    t64_host_storage *storage_out) {
  t64c_reentrant_context *ctx = (t64c_reentrant_context *)context;

  if (ctx->machine != NULL && ctx->probes == 0u) {
    uint8_t digest[T64_DIGEST_SIZE];

    ctx->probes++;
    ctx->seen = t64_machine_digest(ctx->machine, digest);
  }
  return t64c_allocate(&ctx->ledger, size, alignment, storage_out);
}

static void t64c_reentrant_release(void *context, t64_host_storage storage) {
  t64c_release(&((t64c_reentrant_context *)context)->ledger, storage);
}

static int case_lifetime(const t64c_args *args) {
  t64c_tally tally = {0ul, 0ul};
  t64c_ledger *ledger = (t64c_ledger *)calloc(1u, sizeof *ledger);
  t64c_reentrant_context *reentrant =
      (t64c_reentrant_context *)calloc(1u, sizeof *reentrant);
  t64c_fixture *fixture = NULL;
  const t64_profile_descriptor *profile = NULL;
  t64_host_allocator allocator =
      T64_HOST_ALLOCATOR_INIT(t64c_allocate, t64c_release, NULL);
  t64_machine *machine = NULL;
  t64_machine *sentinel_machine;
  size_t created_allocations;
  size_t k;
  bool sweep_ok = true;
  bool sweep_reached_ok = false;

  if (ledger == NULL || reentrant == NULL) {
    free(ledger);
    free(reentrant);
    return 2;
  }
  allocator.context = ledger;
  T64C_CHECK_U32(&tally, t64_profile_lookup(T64C_PROFILE_ID, &profile), T64_STATUS_OK);
  if (profile == NULL) {
    free(ledger);
    free(reentrant);
    return 2;
  }

  /* accepted: create and destroy balance every allocation. */
  accepted(&tally,
           t64_machine_create(&(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(
                                  &allocator, profile),
                              &machine) == T64_STATUS_OK &&
               machine != NULL,
           "create returns OK and a machine");
  created_allocations = ledger->allocations;
  accepted(&tally,
           created_allocations > 0u && ledger->outstanding == created_allocations,
           "create allocated through the host and holds every block");
  t64_machine_destroy(machine);
  accepted(&tally,
           ledger->releases == created_allocations && ledger->outstanding == 0u &&
               ledger->bad_releases == 0u,
           "destroy released every block exactly once");
  t64_machine_destroy(NULL);
  accepted(&tally, ledger->bad_releases == 0u && ledger->outstanding == 0u,
           "destroy(NULL) does nothing");

  /* accepted: an admitted image is released by destroy through the host. */
  fixture = open_admitted(args);
  if (fixture != NULL) {
    accepted(&tally,
             fixture->ledger.outstanding == fixture->ledger.allocations &&
                 fixture->ledger.allocations > created_allocations,
             "admission allocated the image storage through the host");
    accepted(&tally, close_balanced(fixture),
             "destroy after admission released the image and every block");
    fixture = NULL;
  } else {
    T64C_CHECK(&tally, false);
  }

  /* refused: caller faults of create allocate nothing and leave the output NULL. */
  {
    const size_t calls_before = ledger->calls;
    t64_machine_create_info info = T64_MACHINE_CREATE_INFO_INIT(&allocator, profile);

    sentinel_machine = (t64_machine *)(void *)&tally;
    refused(&tally, t64_machine_create(&info, NULL), T64_STATUS_INVALID_ARGUMENT,
            "create with a NULL output slot", NULL, NULL);
    machine = sentinel_machine;
    refused(&tally, t64_machine_create(NULL, &machine), T64_STATUS_INVALID_ARGUMENT,
            "create with a NULL create-info", NULL, NULL);
    untouched(&tally, machine == NULL, "create(NULL info) leaves *machine_out NULL");
    info.allocator = NULL;
    machine = sentinel_machine;
    refused(&tally, t64_machine_create(&info, &machine), T64_STATUS_INVALID_ARGUMENT,
            "create with a NULL allocator", NULL, NULL);
    untouched(&tally, machine == NULL, "create(NULL allocator) leaves *machine_out NULL");
    info.allocator = &allocator;
    info.profile = NULL;
    machine = sentinel_machine;
    refused(&tally, t64_machine_create(&info, &machine), T64_STATUS_INVALID_ARGUMENT,
            "create with a NULL profile", NULL, NULL);
    untouched(&tally, machine == NULL, "create(NULL profile) leaves *machine_out NULL");
    accepted(&tally, ledger->calls == calls_before && ledger->outstanding == 0u,
             "no refused create called the host allocator");
  }

  /* refused: the allocator refusing the k-th allocation, for every k, ends in
   * ALLOCATION_FAILED with nothing outstanding; the first k that succeeds is
   * the number of allocations of a normal create plus one. */
  for (k = 1u; k <= created_allocations + 1u && !sweep_reached_ok; k++) {
    t64_status status;

    ledger->fail_in = k;
    machine = sentinel_machine;
    status = t64_machine_create(
        &(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(&allocator, profile),
        &machine);
    ledger->fail_in = 0u;
    if (status == T64_STATUS_OK) {
      sweep_reached_ok = true;
      t64_machine_destroy(machine);
    } else {
      sweep_ok = sweep_ok && status == T64_STATUS_ALLOCATION_FAILED &&
                 t64c_status_class(status) == T64C_CLASS_REFUSED &&
                 machine == NULL && ledger->outstanding == 0u &&
                 ledger->bad_releases == 0u;
    }
  }
  tally.checks++;
  if (!sweep_ok || !sweep_reached_ok) {
    tally.failures++;
  }
  (void)printf("refused: create with the allocator refusing allocation 1..%u: "
               "ALLOCATION_FAILED, *machine_out NULL, nothing outstanding%s\n",
               (unsigned)created_allocations, sweep_ok && sweep_reached_ok ? "" : " FAILED");

  /* refused: a call made from inside an allocator callback is REENTRANT. */
  reentrant->seen = T64_STATUS_OK;
  allocator = (t64_host_allocator)T64_HOST_ALLOCATOR_INIT(
      t64c_reentrant_allocate, t64c_reentrant_release, reentrant);
  machine = NULL;
  if (t64_machine_create(
          &(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(&allocator, profile),
          &machine) == T64_STATUS_OK &&
      machine != NULL) {
    reentrant->machine = machine;
    fixture = t64c_fixture_new(args->rom_path);
    if (fixture != NULL) {
      const t64_status admitted =
          t64_machine_admit_rom(machine, fixture->rom, fixture->rom_length);
      accepted(&tally, admitted == T64_STATUS_OK && reentrant->probes == 1u,
               "admission ran the host callback with the probe armed");
      refused(&tally, reentrant->seen, T64_STATUS_REENTRANT,
              "a library call from inside the allocator callback", NULL, NULL);
      t64c_fixture_free(fixture);
    } else {
      T64C_CHECK(&tally, false);
    }
    t64_machine_destroy(machine);
    accepted(&tally,
             reentrant->ledger.outstanding == 0u && reentrant->ledger.bad_releases == 0u,
             "destroy balanced the ledger after the reentrant probe");
  } else {
    T64C_CHECK(&tally, false);
  }

  free(ledger);
  free(reentrant);
  return t64c_report("ops-lifetime", &tally);
}

/* ---- ops-handshake --------------------------------------------------------- */

static int case_handshake(const t64c_args *args) {
  t64c_tally tally = {0ul, 0ul};
  t64c_ledger *ledger = (t64c_ledger *)calloc(1u, sizeof *ledger);
  const t64_profile_descriptor *profile = NULL;
  t64_host_allocator allocator =
      T64_HOST_ALLOCATOR_INIT(t64c_allocate, t64c_release, NULL);
  t64_machine *machine;
  t64_machine *sentinel_machine = (t64_machine *)(void *)&tally;
  size_t calls_before;

  (void)args;
  if (ledger == NULL) {
    return 2;
  }
  allocator.context = ledger;
  T64C_CHECK_U32(&tally, t64_profile_lookup(T64C_PROFILE_ID, &profile), T64_STATUS_OK);
  if (profile == NULL) {
    free(ledger);
    return 2;
  }

  accepted(&tally, t64_abi_version() == T64_ABI_VERSION,
           "t64_abi_version() equals the header's T64_ABI_VERSION");
  accepted(&tally,
           t64_build_identity() != NULL && t64_build_identity()[0] != '\0' &&
               t64_source_revision() != NULL && t64_source_revision()[0] != '\0',
           "build identity and source revision are non-empty strings");
  {
    /* A create-info larger than v1 is accepted: the library reads exactly the v1
     * members and ignores the bytes after them. */
    struct {
      t64_machine_create_info info;
      uint8_t tail[16];
    } big;

    big.info = (t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(&allocator, profile);
    big.info.struct_size = (uint32_t)sizeof big;
    poison(big.tail, sizeof big.tail);
    machine = NULL;
    accepted(&tally,
             t64_machine_create(&big.info, &machine) == T64_STATUS_OK && machine != NULL &&
                 all_sentinel(big.tail, sizeof big.tail),
             "create with a larger create-info (trailing bytes untouched)");
    t64_machine_destroy(machine);
  }

  calls_before = ledger->calls;
  {
    t64_machine_create_info info = T64_MACHINE_CREATE_INFO_INIT(&allocator, profile);
    t64_host_allocator small_allocator = T64_HOST_ALLOCATOR_INIT(t64c_allocate, t64c_release, ledger);
    t64_profile_descriptor small_profile = *profile;

    info.abi_version = T64_ABI_VERSION + 1u;
    machine = sentinel_machine;
    refused(&tally, t64_machine_create(&info, &machine), T64_STATUS_ABI_MISMATCH,
            "create with the minor version one above the library's", NULL, NULL);
    untouched(&tally, machine == NULL, "ABI_MISMATCH leaves *machine_out NULL");
    info.abi_version = T64_ABI_VERSION - 1u;
    machine = sentinel_machine;
    refused(&tally, t64_machine_create(&info, &machine), T64_STATUS_ABI_MISMATCH,
            "create with the minor version one below the library's", NULL, NULL);
    info.abi_version = T64_ABI_VERSION + (UINT32_C(1) << 16);
    machine = sentinel_machine;
    refused(&tally, t64_machine_create(&info, &machine), T64_STATUS_ABI_MISMATCH,
            "create with another major version", NULL, NULL);

    info.abi_version = T64_ABI_VERSION;
    info.struct_size = (uint32_t)sizeof info - 1u;
    machine = sentinel_machine;
    refused(&tally, t64_machine_create(&info, &machine), T64_STATUS_STRUCT_TOO_SMALL,
            "create with a create-info struct_size one byte below v1", NULL, NULL);
    untouched(&tally, machine == NULL, "STRUCT_TOO_SMALL leaves *machine_out NULL");
    info.struct_size = 0u;
    machine = sentinel_machine;
    refused(&tally, t64_machine_create(&info, &machine), T64_STATUS_STRUCT_TOO_SMALL,
            "create with a create-info struct_size of 0", NULL, NULL);
    info.struct_size = (uint32_t)sizeof info;

    small_allocator.struct_size = (uint32_t)sizeof small_allocator - 1u;
    info.allocator = &small_allocator;
    machine = sentinel_machine;
    refused(&tally, t64_machine_create(&info, &machine), T64_STATUS_STRUCT_TOO_SMALL,
            "create with an allocator struct_size one byte below v1", NULL, NULL);
    info.allocator = &allocator;

    small_profile.struct_size = (uint32_t)sizeof small_profile - 1u;
    info.profile = &small_profile;
    machine = sentinel_machine;
    refused(&tally, t64_machine_create(&info, &machine), T64_STATUS_STRUCT_TOO_SMALL,
            "create with a profile struct_size one byte below v1", NULL, NULL);
  }
  accepted(&tally, ledger->calls == calls_before && ledger->outstanding == 0u,
           "no handshake refusal called the host allocator");

  free(ledger);
  return t64c_report("ops-handshake", &tally);
}

/* ---- ops-struct-size ------------------------------------------------------- */

/* A caller-filled out-struct one byte below its v1 size: the call answers
 * STRUCT_TOO_SMALL, the machine digest and every byte of the struct stay. */
#define T64C_SMALL_OUT(label, type, init_macro, call)                          \
  do {                                                                         \
    type out = init_macro;                                                     \
    uint8_t raw[sizeof out];                                                   \
    t64_status small_status;                                                   \
    poison(&out, sizeof out);                                                  \
    out.struct_size = (uint32_t)sizeof out - 1u;                               \
    (void)memcpy(raw, &out, sizeof out);                                       \
    small_status = (call);                                                     \
    refused(&tally, small_status, T64_STATUS_STRUCT_TOO_SMALL, label, machine, \
            before);                                                           \
    untouched(&tally, memcmp(raw, &out, sizeof out) == 0, label);              \
  } while (0)

/* A caller struct larger than v1 with sixteen sentinel bytes behind it: the
 * call is accepted, the library neither writes struct_size nor any byte past
 * the v1 members. `ok_expr` may use `big.value`. */
#define T64C_LARGE_OUT(label, type, init_macro, call, ok_expr)                 \
  do {                                                                         \
    struct {                                                                   \
      type value;                                                              \
      uint8_t tail[16];                                                        \
    } big;                                                                     \
    t64_status large_status;                                                   \
    big.value = (type)init_macro;                                              \
    big.value.struct_size = (uint32_t)sizeof big;                              \
    poison(big.tail, sizeof big.tail);                                         \
    large_status = (call);                                                     \
    accepted(&tally,                                                           \
             large_status == T64_STATUS_OK && (ok_expr) &&                     \
                 big.value.struct_size == (uint32_t)sizeof big &&              \
                 all_sentinel(big.tail, sizeof big.tail),                      \
             label);                                                           \
  } while (0)

static int case_struct_size(const t64c_args *args) {
  t64c_tally tally = {0ul, 0ul};
  t64c_fixture *fixture = open_admitted(args);
  t64_machine *machine;
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t blob[16];
  const t64_profile_descriptor *profile = NULL;

  if (fixture == NULL) {
    return 2;
  }
  machine = fixture->machine;
  profile = fixture->profile;
  T64C_CHECK(&tally, digest_of(machine, before));
  poison(blob, sizeof blob);

  /* Below v1: every in/out struct of every entry. */
  {
    t64_stop_request request =
        T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK, UINT64_C(100));
    t64_stop_result result = T64_STOP_RESULT_INIT;
    uint8_t raw[sizeof result];

    request.struct_size = (uint32_t)sizeof request - 1u;
    poison(&result, sizeof result);
    result.struct_size = (uint32_t)sizeof result;
    (void)memcpy(raw, &result, sizeof result);
    refused(&tally, t64_machine_advance(machine, &request, &result),
            T64_STATUS_STRUCT_TOO_SMALL,
            "advance with a request struct_size one byte below v1", machine, before);
    untouched(&tally, memcmp(raw, &result, sizeof result) == 0,
              "advance(small request) result");
    request.struct_size = (uint32_t)sizeof request;
    result.struct_size = (uint32_t)sizeof result - 1u;
    (void)memcpy(raw, &result, sizeof result);
    refused(&tally, t64_machine_advance(machine, &request, &result),
            T64_STATUS_STRUCT_TOO_SMALL,
            "advance with a result struct_size one byte below v1", machine, before);
    untouched(&tally, memcmp(raw, &result, sizeof result) == 0,
              "advance(small result) result");
  }
  T64C_SMALL_OUT("cpu_stop_detail with struct_size one byte below v1",
                 t64_cpu_stop_detail, T64_CPU_STOP_DETAIL_INIT,
                 t64_machine_cpu_stop_detail(machine, &out));
  {
    uint8_t data[8];

    poison(data, sizeof data);
    T64C_SMALL_OUT("output_drain result with struct_size one byte below v1",
                   t64_output_drain_result, T64_OUTPUT_DRAIN_RESULT_INIT,
                   t64_machine_output_drain(machine, data, sizeof data, &out));
    untouched(&tally, all_sentinel(data, sizeof data),
              "output_drain(small result) data buffer");
  }
  T64C_SMALL_OUT("output_status with struct_size one byte below v1",
                 t64_output_status, T64_OUTPUT_STATUS_INIT,
                 t64_machine_output_status(machine, &out));
  T64C_SMALL_OUT("video_info with struct_size one byte below v1",
                 t64_video_packet_info, T64_VIDEO_PACKET_INFO_INIT,
                 t64_video_info(machine, &out));
  T64C_SMALL_OUT("audio_info with struct_size one byte below v1",
                 t64_audio_packet_info, T64_AUDIO_PACKET_INFO_INIT,
                 t64_audio_info(machine, &out));
  T64C_SMALL_OUT("save_info with struct_size one byte below v1", t64_save_info,
                 T64_SAVE_INFO_INIT,
                 t64_machine_save_info(machine, T64_SAVE_SRAM_32K, &out));
  T64C_SMALL_OUT("state_import report with struct_size one byte below v1",
                 t64_state_import_report, T64_STATE_IMPORT_REPORT_INIT,
                 t64_machine_state_import(machine, blob, sizeof blob, 0u, &out));
  untouched(&tally, all_sentinel(blob, sizeof blob),
            "state_import(small report) blob");
  {
    t64_trace_record records[2];
    size_t count = 7u;
    uint64_t dropped = 7u;
    bool saturated = true;
    uint8_t raw[sizeof records];

    poison(records, sizeof records);
    (void)memcpy(raw, records, sizeof records);
    refused(&tally,
            t64_machine_trace_drain(machine, records, (uint32_t)sizeof records[0] - 1u,
                                    2u, &count, &dropped, &saturated),
            T64_STATUS_STRUCT_TOO_SMALL, "trace_drain with a record_size below v1",
            machine, before);
    untouched(&tally,
              memcmp(raw, records, sizeof records) == 0 && count == 7u &&
                  dropped == 7u && saturated,
              "trace_drain(small record_size) records and counters");
  }
  {
    t64_profile_descriptor small_profile = *profile;
    uint8_t digest[T64_DIGEST_SIZE];
    uint8_t text[8];
    size_t length = 7u;

    small_profile.struct_size = (uint32_t)sizeof small_profile - 1u;
    poison(digest, sizeof digest);
    refused(&tally, t64_profile_hash(&small_profile, digest),
            T64_STATUS_STRUCT_TOO_SMALL,
            "profile_hash with a descriptor struct_size one byte below v1", machine,
            before);
    untouched(&tally, all_sentinel(digest, sizeof digest), "profile_hash(small) digest");
    poison(text, sizeof text);
    refused(&tally, t64_profile_serialize(&small_profile, text, sizeof text, &length),
            T64_STATUS_STRUCT_TOO_SMALL,
            "profile_serialize with a descriptor struct_size one byte below v1",
            machine, before);
    untouched(&tally, all_sentinel(text, sizeof text) && length == 7u,
              "profile_serialize(small) buffer and length");
  }

  /* Larger than v1: accepted, struct_size and the bytes behind v1 untouched. */
  T64C_LARGE_OUT("advance with a result struct larger than v1 (trailing bytes untouched)",
                 t64_stop_result, T64_STOP_RESULT_INIT,
                 t64_machine_advance(machine,
                                     &(t64_stop_request)T64_STOP_REQUEST_INIT(
                                         T64_STOP_GUEST_TICK, UINT64_C(100)),
                                     &big.value),
                 big.value.reason == T64_STOP_REASON_BOUND && big.value.tick == 100u);
  T64C_LARGE_OUT("output_status with a struct larger than v1 (trailing bytes untouched)",
                 t64_output_status, T64_OUTPUT_STATUS_INIT,
                 t64_machine_output_status(machine, &big.value),
                 big.value.video_free + big.value.video_queued > 0u);
  T64C_LARGE_OUT("save_info with a struct larger than v1 (trailing bytes untouched)",
                 t64_save_info, T64_SAVE_INFO_INIT,
                 t64_machine_save_info(machine, T64_SAVE_SRAM_32K, &big.value),
                 big.value.kind == T64_SAVE_SRAM_32K &&
                     big.value.size_bytes == T64C_RUN_SAVE_BYTES);
  {
    uint8_t data[8];

    T64C_LARGE_OUT("output_drain result larger than v1 (trailing bytes untouched)",
                   t64_output_drain_result, T64_OUTPUT_DRAIN_RESULT_INIT,
                   t64_machine_output_drain(machine, data, sizeof data, &big.value),
                   big.value.copied_bytes == 0u);
  }
  {
    struct {
      t64_profile_descriptor value;
      uint8_t tail[16];
    } big;
    uint8_t plain[T64_DIGEST_SIZE];
    uint8_t larger[T64_DIGEST_SIZE];

    big.value = *profile;
    big.value.struct_size = (uint32_t)sizeof big;
    poison(big.tail, sizeof big.tail);
    accepted(&tally,
             t64_profile_hash(profile, plain) == T64_STATUS_OK &&
                 t64_profile_hash(&big.value, larger) == T64_STATUS_OK &&
                 memcmp(plain, larger, sizeof plain) == 0 &&
                 all_sentinel(big.tail, sizeof big.tail),
             "profile_hash of a larger descriptor equals the v1 hash (struct_size is outside it)");
  }

  accepted(&tally, close_balanced(fixture), "destroy balanced the ledger");
  return t64c_report("ops-struct-size", &tally);
}

/* ---- ops-profile ----------------------------------------------------------- */

static int case_profile(const t64c_args *args) {
  t64c_tally tally = {0ul, 0ul};
  t64c_fixture *fixture = open_admitted(args);
  const t64_profile_descriptor *profile = NULL;
  const t64_profile_descriptor *other = NULL;
  const t64_profile_descriptor *owned = NULL;
  const t64_profile_descriptor *sentinel_profile;
  uint8_t digest[T64_DIGEST_SIZE];
  uint8_t again[T64_DIGEST_SIZE];
  uint8_t bytes[T64_PROFILE_SERIALIZED_MAX_BYTES];
  uint8_t second[T64_PROFILE_SERIALIZED_MAX_BYTES];
  uint8_t before[T64_DIGEST_SIZE];
  size_t length = 0u;
  size_t required = 0u;

  if (fixture == NULL) {
    return 2;
  }
  sentinel_profile = (const t64_profile_descriptor *)(const void *)&tally;
  T64C_CHECK(&tally, digest_of(fixture->machine, before));

  /* lookup */
  accepted(&tally,
           t64_profile_lookup(T64C_PROFILE_ID, &profile) == T64_STATUS_OK &&
               profile != NULL && strcmp(profile->profile_id, T64C_PROFILE_ID) == 0 &&
               profile->struct_size >= (uint32_t)sizeof(uint32_t),
           "lookup of the composite profile id returns its descriptor");
  accepted(&tally,
           t64_profile_lookup(T64C_PROFILE_ID, &other) == T64_STATUS_OK && other == profile,
           "lookup is stable: the same immutable descriptor every time");
  other = sentinel_profile;
  refused(&tally, t64_profile_lookup("no-such-profile", &other), T64_STATUS_UNKNOWN_PROFILE,
          "lookup of an unknown profile id", fixture->machine, before);
  untouched(&tally, other == NULL || other == sentinel_profile,
            "lookup(unknown) returns no descriptor");
  refused(&tally, t64_profile_lookup(NULL, &other), T64_STATUS_INVALID_ARGUMENT,
          "lookup with a NULL id", fixture->machine, before);
  refused(&tally, t64_profile_lookup(T64C_PROFILE_ID, NULL), T64_STATUS_INVALID_ARGUMENT,
          "lookup with a NULL output slot", fixture->machine, before);

  /* hash */
  accepted(&tally,
           t64_profile_hash(profile, digest) == T64_STATUS_OK &&
               t64_profile_hash(profile, again) == T64_STATUS_OK &&
               memcmp(digest, again, sizeof digest) == 0,
           "hash of the descriptor is deterministic");
  poison(digest, sizeof digest);
  refused(&tally, t64_profile_hash(NULL, digest), T64_STATUS_INVALID_ARGUMENT,
          "hash of a NULL descriptor", fixture->machine, before);
  untouched(&tally, all_sentinel(digest, sizeof digest), "hash(NULL) digest");
  refused(&tally, t64_profile_hash(profile, NULL), T64_STATUS_INVALID_ARGUMENT,
          "hash into a NULL digest", fixture->machine, before);

  /* serialize: size query, exact fit, one byte short, caller faults */
  length = 0u;
  accepted(&tally,
           t64_profile_serialize(profile, NULL, 0u, &length) == T64_STATUS_BUFFER_TOO_SMALL &&
               length > 0u && length <= T64_PROFILE_SERIALIZED_MAX_BYTES,
           "serialize with no buffer reports the required length (BUFFER_TOO_SMALL)");
  required = length;
  length = 0u;
  accepted(&tally,
           required > 0u && t64_profile_serialize(profile, bytes, required, &length) ==
                                T64_STATUS_OK &&
               length == required,
           "serialize into an exact-size buffer");
  poison(second, sizeof second);
  length = 0u;
  accepted(&tally,
           t64_profile_serialize(profile, second, sizeof second, &length) == T64_STATUS_OK &&
               length == required && memcmp(bytes, second, required) == 0,
           "serialize is deterministic and a larger buffer changes no byte past the length");
  poison(second, sizeof second);
  length = 0u;
  refused(&tally,
          t64_profile_serialize(profile, second, required - 1u, &length),
          T64_STATUS_BUFFER_TOO_SMALL, "serialize into a buffer one byte short",
          fixture->machine, before);
  untouched(&tally, length == required && all_sentinel(second, required),
            "serialize(short) reports the required length and keeps the bytes");
  refused(&tally, t64_profile_serialize(profile, bytes, required, NULL),
          T64_STATUS_INVALID_ARGUMENT, "serialize with a NULL length output",
          fixture->machine, before);
  refused(&tally, t64_profile_serialize(NULL, bytes, required, &length),
          T64_STATUS_INVALID_ARGUMENT, "serialize of a NULL descriptor",
          fixture->machine, before);

  /* the instance's own descriptor hashes like the registry's */
  {
    uint8_t registry_hash[T64_DIGEST_SIZE];
    uint8_t owned_hash[T64_DIGEST_SIZE];

    accepted(&tally,
             t64_machine_profile(fixture->machine, &owned) == T64_STATUS_OK &&
                 owned != NULL && owned != profile &&
                 t64_profile_hash(owned, owned_hash) == T64_STATUS_OK &&
                 t64_profile_hash(profile, registry_hash) == T64_STATUS_OK &&
                 memcmp(owned_hash, registry_hash, sizeof owned_hash) == 0,
             "the machine's own descriptor copy hashes like the registry descriptor");
  }
  refused(&tally, t64_machine_profile(fixture->machine, NULL), T64_STATUS_INVALID_ARGUMENT,
          "machine_profile with a NULL output slot", fixture->machine, before);

  accepted(&tally, close_balanced(fixture), "destroy balanced the ledger");
  return t64c_report("ops-profile", &tally);
}

/* ---- ops-admission --------------------------------------------------------- */

/* The observations a refused admission must not move. */
typedef struct {
  uint8_t machine[T64_DIGEST_SIZE];
  uint8_t content[T64_DIGEST_SIZE];
  size_t rom_size;
  size_t outstanding;
} t64c_admission_snapshot;

static bool admission_snapshot(t64c_fixture *fixture, t64c_admission_snapshot *snap) {
  snap->outstanding = fixture->ledger.outstanding;
  return t64_machine_digest(fixture->machine, snap->machine) == T64_STATUS_OK &&
         t64_machine_content_digest(fixture->machine, snap->content) == T64_STATUS_OK &&
         t64_machine_rom_size(fixture->machine, &snap->rom_size) == T64_STATUS_OK;
}

static bool admission_unchanged(t64c_fixture *fixture, const t64c_admission_snapshot *was) {
  t64c_admission_snapshot now;

  return admission_snapshot(fixture, &now) &&
         memcmp(now.machine, was->machine, sizeof now.machine) == 0 &&
         memcmp(now.content, was->content, sizeof now.content) == 0 &&
         now.rom_size == was->rom_size && now.outstanding == was->outstanding;
}

static void admission_refusal(t64c_tally *tally, t64c_fixture *fixture, t64_status actual,
                              t64_status expected, const char *label,
                              const t64c_admission_snapshot *was) {
  refused(tally, actual, expected, label, fixture->machine, was->machine);
  tally->checks++;
  if (!admission_unchanged(fixture, was)) {
    tally->failures++;
    (void)printf("refused: %s: image, content digest or allocations CHANGED\n", label);
  } else {
    (void)printf("refused: %s: previous image, size, content digest and allocations kept\n",
                 label);
  }
}

static int case_admission(const t64c_args *args) {
  t64c_tally tally = {0ul, 0ul};
  t64c_fixture *fixture = t64c_fixture_new(args->rom_path);
  t64c_admission_snapshot first;
  uint8_t *copy = NULL;
  uint8_t header[T64_ROM_HEADER_SIZE];
  size_t header_length = 0u;
  if (fixture == NULL) {
    return 2;
  }
  T64C_CHECK(&tally, fixture->rom_length >= 4096u && fixture->rom_length % 4u == 0u);

  /* refused before any image: NOT_READY with the output untouched */
  {
    size_t size_slot = 77u;

    refused(&tally, t64_machine_rom_size(fixture->machine, &size_slot), T64_STATUS_NOT_READY,
            "rom_size before an image is admitted", fixture->machine, NULL);
    untouched(&tally, size_slot == 77u, "rom_size(NOT_READY) output");
  }

  /* accepted: first admission, observations, replacement by the same bytes */
  accepted(&tally, t64c_fixture_admit(fixture) == T64_STATUS_OK, "admit the composite ROM");
  T64C_CHECK(&tally, admission_snapshot(fixture, &first));
  accepted(&tally, first.rom_size == fixture->rom_length,
           "rom_size equals the length of the admitted file");
  header_length = 0u;
  accepted(&tally,
           t64_machine_rom_header(fixture->machine, NULL, 0u, &header_length) ==
                   T64_STATUS_BUFFER_TOO_SMALL &&
               header_length == T64_ROM_HEADER_SIZE &&
               t64_machine_rom_header(fixture->machine, header, sizeof header,
                                      &header_length) == T64_STATUS_OK &&
               header_length == T64_ROM_HEADER_SIZE &&
               memcmp(header, fixture->rom, T64_ROM_HEADER_SIZE) == 0,
           "rom_header: size query, then the 64 normalized bytes equal the file's header");
  accepted(&tally, t64c_fixture_admit(fixture) == T64_STATUS_OK && admission_unchanged(fixture, &first),
           "replacement by the same bytes reaches the same machine and content digest");

  /* refused: every candidate that fails keeps the previous image (transactional) */
  admission_refusal(&tally, fixture,
                    t64_machine_admit_rom(fixture->machine, fixture->rom, 0u),
                    T64_STATUS_INVALID_ROM, "admit a zero-length image", &first);
  admission_refusal(&tally, fixture,
                    t64_machine_admit_rom(fixture->machine, fixture->rom, 4092u),
                    T64_STATUS_INVALID_ROM, "admit an image below the 4096-byte minimum", &first);
  admission_refusal(&tally, fixture,
                    t64_machine_admit_rom(fixture->machine, fixture->rom,
                                          fixture->rom_length - 1u),
                    T64_STATUS_INVALID_ROM, "admit an image whose length is not a multiple of 4",
                    &first);
  copy = (uint8_t *)malloc(fixture->rom_length);
  if (copy == NULL) {
    t64c_fixture_free(fixture);
    return 2;
  }
  (void)memcpy(copy, fixture->rom, fixture->rom_length);
  copy[0] = 0u;
  copy[1] = 0u;
  copy[2] = 0u;
  copy[3] = 0u;
  admission_refusal(&tally, fixture,
                    t64_machine_admit_rom(fixture->machine, copy, fixture->rom_length),
                    T64_STATUS_INVALID_ROM, "admit an image with no dump signature", &first);
  {
    const t64_status status = t64_machine_admit_rom(fixture->machine, NULL, 4096u);

    /* The header documents no exact status for a NULL span: any refusal of the
     * refused class is accepted, and the previous image must be kept. */
    T64C_CHECK(&tally, status == T64_STATUS_INVALID_ARGUMENT || status == T64_STATUS_INVALID_ROM);
    admission_refusal(&tally, fixture, status, status,
                      "admit a NULL image pointer with a non-zero length", &first);
  }
  fixture->ledger.fail_in = 1u;
  admission_refusal(&tally, fixture,
                    t64_machine_admit_rom(fixture->machine, fixture->rom, fixture->rom_length),
                    T64_STATUS_ALLOCATION_FAILED, "admit while the host refuses the candidate storage",
                    &first);
  fixture->ledger.fail_in = 0u;
  refused(&tally, t64_machine_admit_rom(NULL, fixture->rom, fixture->rom_length),
          T64_STATUS_INVALID_ARGUMENT, "admit on a NULL machine", fixture->machine,
          first.machine);

  free(copy);
  accepted(&tally, close_balanced(fixture), "destroy balanced the ledger");
  return t64c_report("ops-admission", &tally);
}

/* ---- ops-input ------------------------------------------------------------- */

static int case_input(const t64c_args *args) {
  t64c_tally tally = {0ul, 0ul};
  t64c_fixture *fixture = t64c_fixture_new(args->rom_path);
  t64_machine *machine;
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  size_t queued = 0u;
  t64_status status = T64_STATUS_OK;

  if (fixture == NULL) {
    return 2;
  }
  machine = fixture->machine;

  /* refused before admission */
  refused(&tally, t64_machine_queue_input(machine, UINT64_C(100), 0u, 1u, 0, 0),
          T64_STATUS_NOT_READY, "queue_input before an image is admitted", machine, NULL);
  T64C_CHECK_U32(&tally, t64c_fixture_admit(fixture), T64_STATUS_OK);

  /* accepted: the two literal events of the composite, in tick order */
  T64C_CHECK(&tally, digest_of(machine, before));
  accepted(&tally, t64c_queue_literal_inputs(fixture),
           "queue_input of both composite events (ticks 564000 and 582000)");
  accepted(&tally,
           digest_of(machine, after) && memcmp(before, after, sizeof before) != 0,
           "an accepted input changes the machine digest (the queue is part of the state)");

  /* refused: bad port, bad axis on either axis, with the digest unchanged */
  T64C_CHECK(&tally, digest_of(machine, before));
  refused(&tally, t64_machine_queue_input(machine, UINT64_C(600000), 1u, 1u, 0, 0),
          T64_STATUS_INVALID_ARGUMENT, "queue_input on port 1", machine, before);
  refused(&tally, t64_machine_queue_input(machine, UINT64_C(600000), 0u, 1u, 128, 0),
          T64_STATUS_INVALID_ARGUMENT, "queue_input with axis_x 128", machine, before);
  refused(&tally, t64_machine_queue_input(machine, UINT64_C(600000), 0u, 1u, 0, -129),
          T64_STATUS_INVALID_ARGUMENT, "queue_input with axis_y -129", machine, before);
  refused(&tally, t64_machine_queue_input(NULL, UINT64_C(600000), 0u, 1u, 0, 0),
          T64_STATUS_INVALID_ARGUMENT, "queue_input on a NULL machine", machine, before);

  /* accepted at the axis limits; refused in the past */
  accepted(&tally,
           t64_machine_queue_input(machine, UINT64_C(700000), 0u, 0xffffu, -128, 127) ==
               T64_STATUS_OK,
           "queue_input at the axis limits -128 and 127 with every button bit");
  T64C_CHECK(&tally, advance_to(machine, UINT64_C(5000), NULL));
  T64C_CHECK(&tally, digest_of(machine, before));
  refused(&tally, t64_machine_queue_input(machine, UINT64_C(4999), 0u, 1u, 0, 0),
          T64_STATUS_TIME_REVERSED, "queue_input one tick in the past", machine, before);
  accepted(&tally,
           t64_machine_queue_input(machine, UINT64_C(5000), 0u, 1u, 0, 0) == T64_STATUS_OK,
           "queue_input at the current guest tick");

  /* refused: QUEUE_FULL, then room again after an advance */
  while (queued < 4096u) {
    status = t64_machine_queue_input(machine, UINT64_C(700000), 0u, 1u, 0, 0);
    if (status != T64_STATUS_OK) {
      break;
    }
    queued++;
  }
  accepted(&tally, queued > 0u && queued < 4096u, "queue_input filled the event queue");
  T64C_CHECK(&tally, digest_of(machine, before));
  refused(&tally, status, T64_STATUS_QUEUE_FULL, "queue_input into a full event queue",
          machine, before);
  refused(&tally,
          t64_machine_queue_input(machine, UINT64_C(700000), 0u, 1u, 0, 0),
          T64_STATUS_QUEUE_FULL, "a second queue_input into the full queue", machine, before);
  T64C_CHECK(&tally, advance_to(machine, UINT64_C(20000), NULL));
  accepted(&tally,
           t64_machine_queue_input(machine, UINT64_C(700000), 0u, 1u, 0, 0) == T64_STATUS_OK,
           "after an advance dispatched events the host retries and is accepted");

  accepted(&tally, close_balanced(fixture), "destroy balanced the ledger");
  return t64c_report("ops-input", &tally);
}

/* ---- ops-advance ----------------------------------------------------------- */

static bool stop_is_resumable_guest_stop(uint32_t reason) {
  return t64c_stop_class(reason) == T64C_CLASS_GUEST_STOP &&
         t64c_stop_resume(reason) == T64C_RESUME_RESUMABLE;
}

static int case_advance(const t64c_args *args) {
  t64c_tally tally = {0ul, 0ul};
  t64c_fixture *fixture = t64c_fixture_new(args->rom_path);
  t64c_run *run = (t64c_run *)calloc(1u, sizeof *run);
  t64_machine *machine;
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  t64_stop_result stop = T64_STOP_RESULT_INIT;
  t64_stop_result sentinel = T64_STOP_RESULT_INIT;
  t64_status status;

  if (fixture == NULL || run == NULL) {
    free(run);
    t64c_fixture_free(fixture);
    return 2;
  }
  machine = fixture->machine;

  /* refused before admission (the result stays as left) */
  sentinel.reason = 0x5a5a5a5au;
  sentinel.tick = UINT64_C(0x5a5a5a5a5a5a5a5a);
  stop = sentinel;
  refused(&tally,
          t64_machine_advance(machine,
                              &(t64_stop_request)T64_STOP_REQUEST_INIT(
                                  T64_STOP_GUEST_TICK, UINT64_C(10)),
                              &stop),
          T64_STATUS_NOT_READY, "advance before an image is admitted", machine, NULL);
  untouched(&tally, memcmp(&stop, &sentinel, sizeof stop) == 0, "advance(NOT_READY) result");
  T64C_CHECK_U32(&tally, t64c_fixture_admit(fixture), T64_STATUS_OK);

  /* accepted: the instruction-count kind stops with INSTRUCTION_COUNT */
  stop = sentinel;
  status = t64_machine_advance(
      machine,
      &(t64_stop_request)T64_STOP_REQUEST_INIT(T64_STOP_INSTRUCTION_COUNT, UINT64_C(1000)),
      &stop);
  accepted(&tally,
           status == T64_STATUS_OK && stop.reason == T64_STOP_REASON_INSTRUCTION_COUNT &&
               stop_is_resumable_guest_stop(stop.reason) && stop.tick > 0u,
           "advance by instruction count 1000 stops with INSTRUCTION_COUNT (guest-stop, resumable)");
  T64C_CHECK(&tally, digest_of(machine, before));
  refused(&tally,
          t64_machine_advance(machine,
                              &(t64_stop_request)T64_STOP_REQUEST_INIT(
                                  T64_STOP_INSTRUCTION_COUNT, UINT64_C(500)),
                              &stop),
          T64_STATUS_LIMIT_REVERSED, "instruction-count limit behind the counter", machine,
          before);

  /* accepted: the time kind reaches its bound exactly (through resumable stops) */
  accepted(&tally, advance_to(machine, UINT64_C(5000), &stop) &&
                       stop.reason == T64_STOP_REASON_BOUND && stop.tick == 5000u &&
                       stop_is_resumable_guest_stop(stop.reason),
           "advance by guest tick 5000 reaches BOUND at exactly 5000 (guest-stop, resumable)");
  T64C_CHECK(&tally, digest_of(machine, before));
  refused(&tally,
          t64_machine_advance(machine,
                              &(t64_stop_request)T64_STOP_REQUEST_INIT(
                                  T64_STOP_GUEST_TICK, UINT64_C(4000)),
                              &stop),
          T64_STATUS_TIME_REVERSED, "guest-tick bound behind the guest time", machine, before);
  accepted(&tally,
           t64_machine_advance(machine,
                               &(t64_stop_request)T64_STOP_REQUEST_INIT(
                                   T64_STOP_GUEST_TICK, UINT64_C(5000)),
                               &stop) == T64_STATUS_OK &&
               stop.reason == T64_STOP_REASON_BOUND && stop.tick == 5000u,
           "advance to the current tick is BOUND at that tick");

  /* refused: unknown kind, NULL request, NULL result, NULL machine */
  T64C_CHECK(&tally, digest_of(machine, before));
  stop = sentinel;
  refused(&tally,
          t64_machine_advance(machine, &(t64_stop_request)T64_STOP_REQUEST_INIT(99u, UINT64_C(1)),
                              &stop),
          T64_STATUS_INVALID_ARGUMENT, "advance with an unknown stop kind", machine, before);
  untouched(&tally, memcmp(&stop, &sentinel, sizeof stop) == 0, "advance(unknown kind) result");
  refused(&tally, t64_machine_advance(machine, NULL, &stop), T64_STATUS_INVALID_ARGUMENT,
          "advance with a NULL request", machine, before);
  refused(&tally,
          t64_machine_advance(machine,
                              &(t64_stop_request)T64_STOP_REQUEST_INIT(
                                  T64_STOP_GUEST_TICK, UINT64_C(6000)),
                              NULL),
          T64_STATUS_INVALID_ARGUMENT, "advance with a NULL result", machine, before);
  refused(&tally,
          t64_machine_advance(NULL,
                              &(t64_stop_request)T64_STOP_REQUEST_INIT(
                                  T64_STOP_GUEST_TICK, UINT64_C(6000)),
                              &stop),
          T64_STATUS_INVALID_ARGUMENT, "advance on a NULL machine", machine, before);
  untouched(&tally, memcmp(&stop, &sentinel, sizeof stop) == 0, "advance(NULL machine) result");

  /* accepted: the frame-event kind. The counter counts synthetic frame events
   * that only the in-tree scheduler tests schedule, so through the public API
   * the one limit that is reachable is the current value 0, which stops at
   * once with FRAME_EVENT_COUNT (a limit of 1 would run to the guest's end). */
  accepted(&tally,
           t64_machine_advance(machine,
                               &(t64_stop_request)T64_STOP_REQUEST_INIT(
                                   T64_STOP_FRAME_EVENT_COUNT, UINT64_C(0)),
                               &stop) == T64_STATUS_OK &&
               stop.reason == T64_STOP_REASON_FRAME_EVENT_COUNT && stop.tick == 5000u,
           "advance by frame-event count 0 stops at once with FRAME_EVENT_COUNT");
  T64C_CHECK(&tally, t64c_drain_all(fixture, run));
  T64C_CHECK(&tally, t64c_queue_literal_inputs(fixture));

  /* accepted: the guest completes, and the stop is terminal */
  run->tick = stop.tick;
  accepted(&tally, t64c_run_to_completion(fixture, args->slice, run) &&
                       run->stop_reason == T64_STOP_REASON_GUEST_COMPLETION &&
                       t64c_stop_class(run->stop_reason) == T64C_CLASS_GUEST_STOP &&
                       t64c_stop_resume(run->stop_reason) == T64C_RESUME_TERMINAL,
           "advance to the guest's debug stop ends with GUEST_COMPLETION (guest-stop, terminal)");
  T64C_CHECK(&tally, digest_of(machine, before));
  stop = sentinel;
  accepted(&tally,
           t64_machine_advance(machine,
                               &(t64_stop_request)T64_STOP_REQUEST_INIT(
                                   T64_STOP_GUEST_TICK, T64C_RUN_BOUND),
                               &stop) == T64_STATUS_OK &&
               stop.reason == T64_STOP_REASON_GUEST_COMPLETION && stop.tick == run->tick,
           "after the terminal stop every advance repeats GUEST_COMPLETION at the same tick");
  accepted(&tally, digest_of(machine, after) && memcmp(before, after, sizeof before) == 0,
           "a repeated terminal advance leaves the digest unchanged");
  refused(&tally, t64_machine_queue_input(machine, T64C_RUN_BOUND, 0u, 1u, 0, 0),
          T64_STATUS_NOT_READY, "queue_input on a machine at its terminal stop", machine,
          before);

  free(run);
  accepted(&tally, close_balanced(fixture), "destroy balanced the ledger");
  return t64c_report("ops-advance", &tally);
}

/* ---- ops-reset ------------------------------------------------------------- */

static int case_reset(const t64c_args *args) {
  t64c_tally tally = {0ul, 0ul};
  t64c_fixture *fixture = t64c_fixture_new(args->rom_path);
  t64c_run *first = (t64c_run *)calloc(1u, sizeof *first);
  t64c_run *second = (t64c_run *)calloc(1u, sizeof *second);
  t64_machine *machine;
  uint8_t admitted[T64_DIGEST_SIZE];
  uint8_t now[T64_DIGEST_SIZE];
  t64_output_status ring = T64_OUTPUT_STATUS_INIT;
  t64_cpu_stop_detail detail = T64_CPU_STOP_DETAIL_INIT;
  size_t calls_before;

  if (fixture == NULL || first == NULL || second == NULL) {
    free(first);
    free(second);
    t64c_fixture_free(fixture);
    return 2;
  }
  machine = fixture->machine;

  /* refused: no image, NULL machine */
  refused(&tally, t64_machine_reset(machine), T64_STATUS_NOT_READY,
          "reset before an image is admitted", machine, NULL);
  refused(&tally, t64_machine_reset(NULL), T64_STATUS_INVALID_ARGUMENT, "reset of a NULL machine",
          machine, NULL);

  /* accepted: reset after a complete run restores the first-admission state */
  T64C_CHECK_U32(&tally, t64c_fixture_admit(fixture), T64_STATUS_OK);
  T64C_CHECK(&tally, digest_of(machine, admitted));
  T64C_CHECK(&tally, t64c_queue_literal_inputs(fixture));
  T64C_CHECK(&tally, t64c_run_to_completion(fixture, args->slice, first));
  T64C_CHECK(&tally, t64c_finish_run(fixture, first));
  T64C_CHECK_U32(&tally, t64_machine_cpu_stop_detail(machine, &detail), T64_STATUS_OK);
  calls_before = fixture->ledger.calls;
  accepted(&tally, t64_machine_reset(machine) == T64_STATUS_OK, "reset after the guest completed");
  accepted(&tally, fixture->ledger.calls == calls_before,
           "reset calls neither allocator callback");
  accepted(&tally,
           digest_of(machine, now) && memcmp(now, admitted, sizeof now) == 0,
           "the digest after reset equals the digest right after the first admission");
  accepted(&tally,
           t64_machine_output_status(machine, &ring) == T64_STATUS_OK &&
               ring.video_queued == 0u && ring.audio_queued == 0u &&
               ring.text_pending_bytes == 0u,
           "both rings and the ISViewer cursor are empty after reset");
  detail = (t64_cpu_stop_detail)T64_CPU_STOP_DETAIL_INIT;
  refused(&tally, t64_machine_cpu_stop_detail(machine, &detail), T64_STATUS_NOT_READY,
          "cpu_stop_detail after reset (no terminal stop any more)", machine, admitted);

  /* accepted: the same run again reproduces the first run exactly */
  T64C_CHECK(&tally, t64c_queue_literal_inputs(fixture));
  T64C_CHECK(&tally, t64c_run_to_completion(fixture, args->slice, second));
  T64C_CHECK(&tally, t64c_finish_run(fixture, second));
  accepted(&tally,
           second->tick == first->tick && second->stop_reason == first->stop_reason &&
               memcmp(second->digest, first->digest, sizeof second->digest) == 0 &&
               second->video_count == first->video_count &&
               second->audio_count == first->audio_count &&
               second->text_length == first->text_length &&
               memcmp(second->text, first->text, first->text_length) == 0 &&
               memcmp(second->save, first->save, first->save_length) == 0,
           "a second run after reset gives the same tick, digest, packets, transcript and save");

  free(first);
  free(second);
  accepted(&tally, close_balanced(fixture), "destroy balanced the ledger");
  return t64c_report("ops-reset", &tally);
}

/* ---- ops-unload ------------------------------------------------------------ */

static int case_unload(const t64c_args *args) {
  t64c_tally tally = {0ul, 0ul};
  t64c_fixture *fixture = t64c_fixture_new(args->rom_path);
  t64_machine *machine;
  uint8_t admitted[T64_DIGEST_SIZE];
  uint8_t now[T64_DIGEST_SIZE];
  size_t size_slot = 77u;
  size_t releases_before;
  size_t outstanding_with_image;

  if (fixture == NULL) {
    return 2;
  }
  machine = fixture->machine;

  /* refused: no image, NULL machine */
  refused(&tally, t64_machine_unload(machine), T64_STATUS_NOT_READY,
          "unload before an image is admitted", machine, NULL);
  refused(&tally, t64_machine_unload(NULL), T64_STATUS_INVALID_ARGUMENT, "unload of a NULL machine",
          machine, NULL);

  /* accepted: unload releases the image through the host */
  T64C_CHECK_U32(&tally, t64c_fixture_admit(fixture), T64_STATUS_OK);
  T64C_CHECK(&tally, digest_of(machine, admitted));
  outstanding_with_image = fixture->ledger.outstanding;
  releases_before = fixture->ledger.releases;
  accepted(&tally, t64_machine_unload(machine) == T64_STATUS_OK, "unload after an admission");
  accepted(&tally,
           fixture->ledger.releases > releases_before &&
               fixture->ledger.outstanding < outstanding_with_image &&
               fixture->ledger.bad_releases == 0u,
           "unload released the image storage through the host allocator");

  /* refused: every entry that needs an image answers NOT_READY, as for an
   * instance that never admitted one */
  refused(&tally, t64_machine_digest(machine, now), T64_STATUS_NOT_READY,
          "digest after unload", machine, NULL);
  refused(&tally, t64_machine_rom_size(machine, &size_slot), T64_STATUS_NOT_READY,
          "rom_size after unload", machine, NULL);
  untouched(&tally, size_slot == 77u, "rom_size(NOT_READY) output");
  refused(&tally,
          t64_machine_advance(machine,
                              &(t64_stop_request)T64_STOP_REQUEST_INIT(
                                  T64_STOP_GUEST_TICK, UINT64_C(10)),
                              &(t64_stop_result)T64_STOP_RESULT_INIT),
          T64_STATUS_NOT_READY, "advance after unload", machine, NULL);
  refused(&tally, t64_machine_reset(machine), T64_STATUS_NOT_READY, "reset after unload", machine,
          NULL);
  refused(&tally, t64_machine_unload(machine), T64_STATUS_NOT_READY, "a second unload", machine,
          NULL);

  /* accepted: admission after unload equals a first admission */
  accepted(&tally,
           t64c_fixture_admit(fixture) == T64_STATUS_OK && digest_of(machine, now) &&
               memcmp(now, admitted, sizeof now) == 0 &&
               fixture->ledger.outstanding == outstanding_with_image,
           "admission after unload reaches the first-admission digest and allocation count");
  accepted(&tally, close_balanced(fixture), "destroy balanced the ledger");
  return t64c_report("ops-unload", &tally);
}

/* ---- dispatch -------------------------------------------------------------- */

typedef struct {
  const char *name;
  int (*run)(const t64c_args *args);
} t64c_ops_case;

static const t64c_ops_case t64c_ops_cases[] = {
    {"ops-lifetime", case_lifetime},   {"ops-handshake", case_handshake},
    {"ops-struct-size", case_struct_size}, {"ops-profile", case_profile},
    {"ops-admission", case_admission}, {"ops-input", case_input},
    {"ops-advance", case_advance},     {"ops-reset", case_reset},
    {"ops-unload", case_unload},
};

bool t64c_ops_known(const char *name) {
  size_t i;

  for (i = 0u; i < sizeof t64c_ops_cases / sizeof t64c_ops_cases[0]; i++) {
    if (strcmp(name, t64c_ops_cases[i].name) == 0) {
      return true;
    }
  }
  return false;
}

void t64c_ops_list(void) {
  size_t i;

  for (i = 0u; i < sizeof t64c_ops_cases / sizeof t64c_ops_cases[0]; i++) {
    (void)puts(t64c_ops_cases[i].name);
  }
}

int t64c_ops_run(const char *name, const t64c_args *args) {
  size_t i;

  for (i = 0u; i < sizeof t64c_ops_cases / sizeof t64c_ops_cases[0]; i++) {
    if (strcmp(name, t64c_ops_cases[i].name) == 0) {
      return t64c_ops_cases[i].run(args);
    }
  }
  return 2;
}
