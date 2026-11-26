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
#include "t64c_composite.h"
#include "t64c_sha256.h"

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

/* ---- ops-rings ------------------------------------------------------------- */

/* Advance with the video ring (and optionally the audio ring) drained after every
 * call, through RESOURCE_LIMIT stops, to a tick or to the guest's completion.
 * The ISViewer text is left alone. */
static bool advance_drained(t64c_fixture *fixture, t64c_run *run, uint64_t target,
                            bool drain_audio) {
  unsigned guard;

  for (guard = 0u; guard < 4096u; guard++) {
    t64_stop_result stop = T64_STOP_RESULT_INIT;
    t64_status status = t64_machine_advance(
        fixture->machine,
        &(t64_stop_request)T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK, target), &stop);

    if (status != T64_STATUS_OK || !t64c_drain_video(fixture, run) ||
        (drain_audio && !t64c_drain_audio(fixture, run))) {
      return false;
    }
    run->tick = stop.tick;
    run->stop_reason = stop.reason;
    if (stop.reason == T64_STOP_REASON_BOUND) {
      return stop.tick == target;
    }
    if (stop.reason == T64_STOP_REASON_GUEST_COMPLETION) {
      return true;
    }
    if (stop.reason != T64_STOP_REASON_RESOURCE_LIMIT) {
      return false;
    }
  }
  return false;
}

/* Run without draining anything until two successive advances end with
 * RESOURCE_LIMIT at the same tick: the machine stopped at backpressure. */
static bool run_undrained_to_stall(t64_machine *machine, uint64_t *stall_tick,
                                   unsigned *budget_stops) {
  uint64_t tick = 0u;
  unsigned same = 0u;
  unsigned guard;

  *budget_stops = 0u;
  for (guard = 0u; guard < 4096u; guard++) {
    const uint64_t target =
        T64C_RUN_BOUND - tick > T64C_RUN_DEFAULT_SLICE ? tick + T64C_RUN_DEFAULT_SLICE
                                                         : T64C_RUN_BOUND;
    t64_stop_result stop = T64_STOP_RESULT_INIT;
    t64_status status = t64_machine_advance(
        machine, &(t64_stop_request)T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK, target),
        &stop);

    if (status != T64_STATUS_OK) {
      return false;
    }
    if (stop.reason == T64_STOP_REASON_RESOURCE_LIMIT) {
      if (stop.tick == tick) {
        same++;
        if (same >= 2u) {
          *stall_tick = tick;
          return true;
        }
      } else {
        same = 0u;
        (*budget_stops)++;
      }
    } else if (stop.reason == T64_STOP_REASON_BOUND) {
      same = 0u;
    } else {
      return false;
    }
    tick = stop.tick;
  }
  return false;
}

static int case_rings(const t64c_args *args) {
  static const uint8_t video_literal[T64C_LIT_VIDEO_PACKET_BYTES] = T64C_LIT_VIDEO_PACKET_INIT;
  static const uint8_t audio_literal[T64C_LIT_AUDIO_PACKET_COUNT][T64C_LIT_AUDIO_PACKET_BYTES] = {
      T64C_LIT_AUDIO_PACKET0_INIT, T64C_LIT_AUDIO_PACKET1_INIT};
  t64c_tally tally = {0ul, 0ul};
  t64c_fixture *fixture = t64c_fixture_new(args->rom_path);
  t64c_fixture *empty = t64c_fixture_new(args->rom_path);
  t64c_run *run = (t64c_run *)calloc(1u, sizeof *run);
  t64_machine *machine;
  t64_output_status ring = T64_OUTPUT_STATUS_INIT;
  t64_video_packet_info video = T64_VIDEO_PACKET_INFO_INIT;
  t64_audio_packet_info audio = T64_AUDIO_PACKET_INFO_INIT;
  uint8_t packet[T64C_RUN_VIDEO_BYTES + 8u];
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  size_t length = 0u;
  uint64_t stall_tick = 0u;
  unsigned budget_stops = 0u;
  size_t i;

  if (fixture == NULL || empty == NULL || run == NULL) {
    free(run);
    t64c_fixture_free(fixture);
    t64c_fixture_free(empty);
    return 2;
  }
  machine = fixture->machine;
  T64C_CHECK_U32(&tally, t64c_fixture_admit(fixture), T64_STATUS_OK);
  T64C_CHECK_U32(&tally, t64c_fixture_admit(empty), T64_STATUS_OK);
  T64C_CHECK(&tally, t64c_queue_literal_inputs(fixture));

  /* Empty rings: info and copy answer NOT_READY with the outputs untouched, and
   * output_status reports empty rings with the full capacity free. */
  T64C_CHECK(&tally, digest_of(empty->machine, before));
  {
    t64_video_packet_info probe = T64_VIDEO_PACKET_INFO_INIT;
    t64_audio_packet_info probe_audio = T64_AUDIO_PACKET_INFO_INIT;
    uint8_t raw[sizeof probe];
    uint8_t raw_audio[sizeof probe_audio];

    poison(&probe, sizeof probe);
    probe.struct_size = (uint32_t)sizeof probe;
    (void)memcpy(raw, &probe, sizeof probe);
    refused(&tally, t64_video_info(empty->machine, &probe), T64_STATUS_NOT_READY,
            "video_info on an empty ring", empty->machine, before);
    untouched(&tally, memcmp(raw, &probe, sizeof probe) == 0, "video_info(empty) output");
    poison(&probe_audio, sizeof probe_audio);
    probe_audio.struct_size = (uint32_t)sizeof probe_audio;
    (void)memcpy(raw_audio, &probe_audio, sizeof probe_audio);
    refused(&tally, t64_audio_info(empty->machine, &probe_audio), T64_STATUS_NOT_READY,
            "audio_info on an empty ring", empty->machine, before);
    untouched(&tally, memcmp(raw_audio, &probe_audio, sizeof probe_audio) == 0,
              "audio_info(empty) output");
  }
  poison(packet, sizeof packet);
  length = 77u;
  refused(&tally, t64_video_copy(empty->machine, packet, T64C_RUN_VIDEO_BYTES, &length),
          T64_STATUS_NOT_READY, "video_copy on an empty ring", empty->machine, before);
  untouched(&tally, length == 77u && all_sentinel(packet, sizeof packet),
            "video_copy(empty) bytes and length");
  accepted(&tally,
           t64_machine_output_status(empty->machine, &ring) == T64_STATUS_OK &&
               ring.video_queued == 0u && ring.video_free == 2u && ring.video_next_bytes == 0u &&
               ring.audio_queued == 0u && ring.audio_free == 2u && ring.audio_next_bytes == 0u &&
               ring.text_pending_bytes == 0u,
           "output_status of empty rings: nothing queued, capacity two packets each, no text");

  /* (A) An undrained run stops with RESOURCE_LIMIT at backpressure. */
  accepted(&tally, run_undrained_to_stall(machine, &stall_tick, &budget_stops),
           "an undrained run reaches a RESOURCE_LIMIT stall at backpressure");
  (void)printf("observed: undrained composite run stalls at backpressure at guest tick %llu "
               "(after %u budget stops)\n",
               (unsigned long long)stall_tick, budget_stops);
  accepted(&tally, stall_tick > 0u && stall_tick < T64C_PIN_COMPLETION_TICK,
           "the stall is before the guest's completion tick");
  accepted(&tally,
           t64_machine_output_status(machine, &ring) == T64_STATUS_OK && ring.video_queued == 2u &&
               ring.video_free == 0u && ring.video_next_bytes == T64C_LIT_VIDEO_PACKET_BYTES,
           "at the stall the video ring is full (two packets queued, none free)");
  T64C_CHECK(&tally, digest_of(machine, before));
  {
    t64_stop_result stop = T64_STOP_RESULT_INIT;
    const t64_status status = t64_machine_advance(
        machine,
        &(t64_stop_request)T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK, T64C_RUN_BOUND), &stop);

    accepted(&tally,
             status == T64_STATUS_OK && stop.reason == T64_STOP_REASON_RESOURCE_LIMIT &&
                 stop.tick == stall_tick && t64c_stop_class(stop.reason) == T64C_CLASS_GUEST_STOP &&
                 t64c_stop_resume(stop.reason) == T64C_RESUME_RESUMABLE &&
                 digest_of(machine, after) && memcmp(before, after, sizeof before) == 0,
             "a further advance without a drain stops again at the same tick and moves nothing");
  }

  /* (B) Copy-out: info, size query, short copy refused, exact copy accepted. */
  accepted(&tally,
           t64_video_info(machine, &video) == T64_STATUS_OK &&
               video.width == T64C_LIT_VIDEO_WIDTH && video.height == T64C_LIT_VIDEO_HEIGHT &&
               video.packet_bytes == T64C_LIT_VIDEO_PACKET_BYTES &&
               video.output_encoding == T64_VIDEO_ENCODING_RGBA8 &&
               strcmp(video.profile_id, T64C_PROFILE_ID) == 0,
           "video_info of the oldest packet: 4x3 RGBA8, 48 bytes, the composite profile");
  accepted(&tally,
           t64_machine_output_status(machine, &ring) == T64_STATUS_OK &&
               ring.video_next_bytes == video.packet_bytes && ring.video_queued == 2u,
           "output_status agrees with video_info (next packet size, two queued)");
  length = 0u;
  refused(&tally, t64_video_copy(machine, NULL, 0u, &length), T64_STATUS_BUFFER_TOO_SMALL,
          "video_copy size query (NULL, capacity 0)", machine, before);
  untouched(&tally, length == T64C_LIT_VIDEO_PACKET_BYTES, "the size query reports 48 bytes");
  poison(packet, sizeof packet);
  length = 0u;
  refused(&tally, t64_video_copy(machine, packet, T64C_LIT_VIDEO_PACKET_BYTES - 1u, &length),
          T64_STATUS_BUFFER_TOO_SMALL, "video_copy into a buffer one byte short", machine, before);
  untouched(&tally,
            length == T64C_LIT_VIDEO_PACKET_BYTES && all_sentinel(packet, sizeof packet) &&
                t64_machine_output_status(machine, &ring) == T64_STATUS_OK &&
                ring.video_queued == 2u,
            "the short copy changed no byte and dequeued nothing");
  refused(&tally, t64_video_copy(machine, NULL, T64C_LIT_VIDEO_PACKET_BYTES, &length),
          T64_STATUS_INVALID_ARGUMENT, "video_copy into a NULL buffer with a capacity", machine,
          before);
  refused(&tally, t64_video_copy(machine, packet, sizeof packet, NULL), T64_STATUS_INVALID_ARGUMENT,
          "video_copy with a NULL length output", machine, before);
  refused(&tally, t64_video_info(machine, NULL), T64_STATUS_INVALID_ARGUMENT,
          "video_info with a NULL output", machine, before);
  poison(packet, sizeof packet);
  length = 0u;
  accepted(&tally,
           t64_video_copy(machine, packet, sizeof packet, &length) == T64_STATUS_OK &&
               length == T64C_LIT_VIDEO_PACKET_BYTES &&
               memcmp(packet, video_literal, sizeof video_literal) == 0 &&
               all_sentinel(packet + length, sizeof packet - length) &&
               t64_machine_output_status(machine, &ring) == T64_STATUS_OK && ring.video_queued == 1u &&
               ring.video_free == 1u,
           "video_copy copies the 48 literal bytes, writes nothing past them and dequeues one packet");
  run->video_count = 0u;
  (void)memcpy(run->video[run->video_count++], packet, T64C_LIT_VIDEO_PACKET_BYTES);

  /* (C) A drain lets the next advance make progress; nothing is lost. */
  T64C_CHECK(&tally, t64c_drain_video(fixture, run));
  accepted(&tally,
           run->video_count == 2u && t64_machine_output_status(machine, &ring) == T64_STATUS_OK &&
               ring.video_queued == 0u && ring.video_free == 2u,
           "draining the ring leaves it empty with both slots free");
  run->tick = stall_tick;
  accepted(&tally, advance_drained(fixture, run, T64C_RUN_BOUND, false) &&
                       run->stop_reason == T64_STOP_REASON_GUEST_COMPLETION &&
                       run->tick > stall_tick,
           "after the drain the next advance makes progress and the guest completes");
  T64C_CHECK(&tally, t64c_drain_video(fixture, run));
  {
    bool equal = true;

    for (i = 0u; i < run->video_count; i++) {
      equal = equal && memcmp(run->video[i], video_literal, sizeof video_literal) == 0;
    }
    accepted(&tally, equal, "every video packet matches the independent literal bytes");
  }
  (void)printf("%s: video packet count %u (pin %u): %s\n", T64C_PIN_LABEL,
               (unsigned)run->video_count, (unsigned)T64C_PIN_VIDEO_PACKET_COUNT,
               run->video_count == T64C_PIN_VIDEO_PACKET_COUNT ? "equal" : "DIFFERENT");
  T64C_CHECK(&tally, run->video_count == T64C_PIN_VIDEO_PACKET_COUNT);

  /* (D) The audio ring holds both packets; the text cursor was never drained. */
  accepted(&tally,
           t64_machine_output_status(machine, &ring) == T64_STATUS_OK && ring.audio_queued == 2u &&
               ring.audio_free == 0u && ring.audio_next_bytes == T64C_LIT_AUDIO_PACKET_BYTES &&
               ring.text_pending_bytes == T64C_LIT_TRANSCRIPT_BYTES && ring.video_queued == 0u,
           "output_status at completion: audio ring full, 188 transcript bytes pending, video empty");
  accepted(&tally,
           t64_audio_info(machine, &audio) == T64_STATUS_OK &&
               audio.byte_length == T64C_LIT_AUDIO_PACKET_BYTES &&
               audio.encoding == T64_AUDIO_ENCODING_S16BE && audio.channels == 2u &&
               audio.bits_per_sample == 16u && strcmp(audio.profile_id, T64C_PROFILE_ID) == 0,
           "audio_info of the oldest packet: S16BE stereo, 16 bytes");
  T64C_CHECK(&tally, digest_of(machine, before));
  length = 0u;
  refused(&tally, t64_audio_copy(machine, NULL, 0u, &length), T64_STATUS_BUFFER_TOO_SMALL,
          "audio_copy size query (NULL, capacity 0)", machine, before);
  untouched(&tally, length == T64C_LIT_AUDIO_PACKET_BYTES, "the size query reports 16 bytes");
  poison(packet, sizeof packet);
  refused(&tally, t64_audio_copy(machine, packet, T64C_LIT_AUDIO_PACKET_BYTES - 1u, &length),
          T64_STATUS_BUFFER_TOO_SMALL, "audio_copy into a buffer one byte short", machine, before);
  untouched(&tally, all_sentinel(packet, sizeof packet), "the short audio copy changed no byte");
  refused(&tally, t64_audio_info(machine, NULL), T64_STATUS_INVALID_ARGUMENT,
          "audio_info with a NULL output", machine, before);
  for (i = 0u; i < T64C_LIT_AUDIO_PACKET_COUNT; i++) {
    poison(packet, sizeof packet);
    length = 0u;
    accepted(&tally,
             t64_audio_copy(machine, packet, sizeof packet, &length) == T64_STATUS_OK &&
                 length == T64C_LIT_AUDIO_PACKET_BYTES &&
                 memcmp(packet, audio_literal[i], T64C_LIT_AUDIO_PACKET_BYTES) == 0 &&
                 all_sentinel(packet + length, sizeof packet - length),
             i == 0u ? "audio_copy of packet 0 equals the independent literal"
                     : "audio_copy of packet 1 equals the independent literal");
  }
  accepted(&tally,
           t64_machine_output_status(machine, &ring) == T64_STATUS_OK && ring.audio_queued == 0u &&
               ring.audio_free == 2u,
           "both audio packets dequeued: the ring is empty");

  free(run);
  accepted(&tally, close_balanced(fixture), "destroy balanced the ledger");
  accepted(&tally, close_balanced(empty), "destroy of the second instance balanced its ledger");
  return t64c_report("ops-rings", &tally);
}

/* ---- ops-text -------------------------------------------------------------- */

static int case_text(const t64c_args *args) {
  static const char literal[] = T64C_LIT_TRANSCRIPT_TEXT;
  t64c_tally tally = {0ul, 0ul};
  t64c_fixture *fixture = t64c_fixture_new(args->rom_path);
  t64c_run *run = (t64c_run *)calloc(1u, sizeof *run);
  t64_machine *machine;
  t64_output_status ring = T64_OUTPUT_STATUS_INIT;
  t64_output_drain_result result = T64_OUTPUT_DRAIN_RESULT_INIT;
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t text[T64C_RUN_TEXT_MAX];
  uint8_t sentinel[T64C_RUN_TEXT_MAX];
  uint8_t unused[8];
  char hex[T64C_SHA256_HEX_SIZE];
  size_t pending;
  size_t total;

  if (fixture == NULL || run == NULL) {
    free(run);
    t64c_fixture_free(fixture);
    return 2;
  }
  machine = fixture->machine;

  /* refused before admission */
  poison(unused, sizeof unused);
  refused(&tally, t64_machine_output_drain(machine, unused, sizeof unused, &result),
          T64_STATUS_NOT_READY, "output_drain before an image is admitted", machine, NULL);
  untouched(&tally, all_sentinel(unused, sizeof unused), "output_drain(NOT_READY) bytes");
  T64C_CHECK_U32(&tally, t64c_fixture_admit(fixture), T64_STATUS_OK);
  T64C_CHECK(&tally, t64c_queue_literal_inputs(fixture));

  /* accepted: an empty drain succeeds with no bytes */
  accepted(&tally,
           t64_machine_output_drain(machine, unused, sizeof unused, &result) == T64_STATUS_OK &&
               result.copied_bytes == 0u && result.lost_output == 0u,
           "an empty drain succeeds with zero copied bytes");

  /* Run until the guest has printed its first lines (video and audio drained). */
  accepted(&tally, advance_drained(fixture, run, UINT64_C(546000), true) && run->tick == 546000u,
           "advance to tick 546000 with the video and audio rings drained");
  T64C_CHECK_U32(&tally, t64_machine_output_status(machine, &ring), T64_STATUS_OK);
  pending = (size_t)ring.text_pending_bytes;
  accepted(&tally, pending > 0u && pending < sizeof literal - 1u,
           "the guest has printed part of its transcript (output_status text_pending_bytes)");
  T64C_CHECK(&tally, digest_of(machine, before));

  /* refused: caller faults leave the cursor and the buffers alone */
  poison(text, sizeof text);
  (void)memcpy(sentinel, text, sizeof sentinel);
  refused(&tally, t64_machine_output_drain(machine, text, pending - 1u, &result),
          T64_STATUS_BUFFER_TOO_SMALL, "output_drain into a buffer one byte short", machine, before);
  untouched(&tally,
            result.copied_bytes == 0u && result.required_bytes == pending &&
                memcmp(text, sentinel, sizeof text) == 0 &&
                t64_machine_output_status(machine, &ring) == T64_STATUS_OK &&
                ring.text_pending_bytes == pending,
            "the short drain reports required_bytes, copies nothing and keeps the cursor");
  refused(&tally, t64_machine_output_drain(machine, NULL, 0u, &result), T64_STATUS_BUFFER_TOO_SMALL,
          "output_drain size query (NULL, capacity 0)", machine, before);
  untouched(&tally, result.required_bytes == pending, "the size query reports the pending bytes");
  refused(&tally, t64_machine_output_drain(machine, NULL, 16u, &result), T64_STATUS_INVALID_ARGUMENT,
          "output_drain into a NULL buffer with a capacity", machine, before);
  refused(&tally, t64_machine_output_drain(machine, text, sizeof text, NULL),
          T64_STATUS_INVALID_ARGUMENT, "output_drain with a NULL result", machine, before);
  refused(&tally, t64_machine_output_drain(NULL, text, sizeof text, &result),
          T64_STATUS_INVALID_ARGUMENT, "output_drain on a NULL machine", machine, before);

  /* accepted: an exact drain copies the printed prefix and moves the cursor */
  accepted(&tally,
           t64_machine_output_drain(machine, text, pending, &result) == T64_STATUS_OK &&
               result.copied_bytes == pending && memcmp(text, literal, pending) == 0 &&
               result.lost_output == 0u && result.total_attempted_bytes_exact == 1u &&
               result.total_attempted_bytes == pending &&
               t64_machine_output_status(machine, &ring) == T64_STATUS_OK &&
               ring.text_pending_bytes == 0u,
           "an exact drain copies the printed prefix of the literal transcript, nothing lost");
  total = pending;

  /* accepted: the rest after completion; the whole transcript equals the literal */
  T64C_CHECK(&tally, advance_drained(fixture, run, T64C_RUN_BOUND, true));
  T64C_CHECK_U32(&tally, t64_machine_output_status(machine, &ring), T64_STATUS_OK);
  pending = (size_t)ring.text_pending_bytes;
  accepted(&tally,
           t64_machine_output_drain(machine, text + total, sizeof text - total, &result) ==
                   T64_STATUS_OK &&
               result.copied_bytes == pending && total + pending == T64C_LIT_TRANSCRIPT_BYTES &&
               result.total_attempted_bytes == T64C_LIT_TRANSCRIPT_BYTES &&
               result.total_attempted_bytes_exact == 1u && result.lost_output == 0u,
           "the rest of the transcript drains after the guest completes (188 bytes in all)");
  total += pending;
  accepted(&tally, total == T64C_LIT_TRANSCRIPT_BYTES && memcmp(text, literal, total) == 0,
           "the two drains concatenate to the independent literal transcript");
  t64c_sha256_hex_of(text, total, hex);
  accepted(&tally, strcmp(hex, T64C_LIT_TRANSCRIPT_SHA256_HEX) == 0,
           "transcript SHA-256 (consumer's own implementation) equals the literal");
  accepted(&tally,
           t64_machine_output_drain(machine, unused, sizeof unused, &result) == T64_STATUS_OK &&
               result.copied_bytes == 0u,
           "a drain after the cursor reached the end copies nothing");

  free(run);
  accepted(&tally, close_balanced(fixture), "destroy balanced the ledger");
  return t64c_report("ops-text", &tally);
}

/* ---- ops-save -------------------------------------------------------------- */

#define T64C_SAVE_BYTES 32768u

static void save_pattern(uint8_t *bytes) {
  size_t i;

  for (i = 0u; i < T64C_SAVE_BYTES; i++) {
    bytes[i] = (uint8_t)(i * 7u + i / 251u);
  }
}

static bool save_generation(t64_machine *machine, uint64_t *generation) {
  t64_save_info info = T64_SAVE_INFO_INIT;

  if (t64_machine_save_info(machine, T64_SAVE_SRAM_32K, &info) != T64_STATUS_OK) {
    return false;
  }
  *generation = info.write_generation;
  return true;
}

static int case_save(const t64c_args *args) {
  t64c_tally tally = {0ul, 0ul};
  t64c_fixture *fixture = open_admitted(args);
  t64c_fixture *empty = t64c_fixture_new(args->rom_path);
  t64_machine *machine;
  t64_save_info info = T64_SAVE_INFO_INIT;
  uint8_t *pattern = (uint8_t *)malloc(T64C_SAVE_BYTES);
  uint8_t *swapped = (uint8_t *)malloc(T64C_SAVE_BYTES);
  uint8_t *reversed = (uint8_t *)malloc(T64C_SAVE_BYTES);
  uint8_t *out = (uint8_t *)malloc(T64C_SAVE_BYTES);
  uint8_t before[T64_DIGEST_SIZE];
  uint64_t generation = 0u;
  uint64_t generation_before = 0u;
  size_t length = 0u;
  size_t i;
  bool blank = true;

  if (fixture == NULL || empty == NULL || pattern == NULL || swapped == NULL || reversed == NULL ||
      out == NULL) {
    free(pattern);
    free(swapped);
    free(reversed);
    free(out);
    t64c_fixture_free(fixture);
    t64c_fixture_free(empty);
    return 2;
  }
  machine = fixture->machine;
  save_pattern(pattern);
  for (i = 0u; i + 1u < T64C_SAVE_BYTES; i += 2u) {
    swapped[i] = pattern[i + 1u];
    swapped[i + 1u] = pattern[i];
  }
  for (i = 0u; i + 3u < T64C_SAVE_BYTES; i += 4u) {
    reversed[i] = pattern[i + 3u];
    reversed[i + 1u] = pattern[i + 2u];
    reversed[i + 2u] = pattern[i + 1u];
    reversed[i + 3u] = pattern[i];
  }

  /* refused before admission (R1) */
  refused(&tally, t64_machine_save_info(empty->machine, T64_SAVE_SRAM_32K, &info),
          T64_STATUS_NOT_READY, "save_info before an image is admitted", empty->machine, NULL);
  refused(&tally,
          t64_machine_save_export(empty->machine, T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_RAW, out,
                                  T64C_SAVE_BYTES, &length),
          T64_STATUS_NOT_READY, "save_export before an image is admitted", empty->machine, NULL);
  refused(&tally,
          t64_machine_save_import(empty->machine, T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_RAW, pattern,
                                  T64C_SAVE_BYTES),
          T64_STATUS_NOT_READY, "save_import before an image is admitted", empty->machine, NULL);

  /* accepted: info and a blank export of a fresh admission */
  accepted(&tally,
           t64_machine_save_info(machine, T64_SAVE_SRAM_32K, &info) == T64_STATUS_OK &&
               info.kind == T64_SAVE_SRAM_32K && info.size_bytes == T64C_SAVE_BYTES &&
               info.flags == 0u && info.write_generation == 0u,
           "save_info: SRAM 32K, 32768 bytes, not busy, write_generation 0");
  length = 0u;
  accepted(&tally,
           t64_machine_save_export(machine, T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_RAW, out,
                                   T64C_SAVE_BYTES, &length) == T64_STATUS_OK &&
               length == T64C_SAVE_BYTES,
           "save_export of a fresh admission copies 32768 bytes");
  for (i = 0u; i < T64C_SAVE_BYTES; i++) {
    blank = blank && out[i] == 0u;
  }
  accepted(&tally, blank, "a fresh admission's save image is blank (all zero)");

  /* refused: the documented check order of export and import, digest unchanged */
  T64C_CHECK(&tally, digest_of(machine, before));
  T64C_CHECK(&tally, save_generation(machine, &generation_before));
  refused(&tally,
          t64_machine_save_export(NULL, T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_RAW, out, T64C_SAVE_BYTES,
                                  &length),
          T64_STATUS_INVALID_ARGUMENT, "save_export on a NULL machine (R0)", machine, before);
  refused(&tally,
          t64_machine_save_import(NULL, T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_RAW, pattern,
                                  T64C_SAVE_BYTES),
          T64_STATUS_INVALID_ARGUMENT, "save_import on a NULL machine (R0)", machine, before);
  refused(&tally,
          t64_machine_save_import(machine, T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_RAW, NULL,
                                  T64C_SAVE_BYTES),
          T64_STATUS_INVALID_ARGUMENT, "save_import of a NULL buffer (R2)", machine, before);
  refused(&tally,
          t64_machine_save_export(machine, T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_RAW, out,
                                  T64C_SAVE_BYTES, NULL),
          T64_STATUS_INVALID_ARGUMENT, "save_export with a NULL length output (R2)", machine, before);
  refused(&tally,
          t64_machine_save_export(machine, T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_RAW, NULL, 5u,
                                  &length),
          T64_STATUS_INVALID_ARGUMENT, "save_export into a NULL buffer with a capacity (R2)",
          machine, before);
  refused(&tally,
          t64_machine_save_info(machine, T64_SAVE_SRAM_32K, NULL), T64_STATUS_INVALID_ARGUMENT,
          "save_info with a NULL output", machine, before);
  refused(&tally, t64_machine_save_info(machine, 0u, &info), T64_STATUS_SAVE_UNKNOWN_DEVICE,
          "save_info of kind 0 (R4)", machine, before);
  refused(&tally,
          t64_machine_save_export(machine, 8u, T64_SAVE_FORMAT_RAW, out, T64C_SAVE_BYTES, &length),
          T64_STATUS_SAVE_UNKNOWN_DEVICE, "save_export of kind 8 (R4)", machine, before);
  refused(&tally, t64_machine_save_info(machine, T64_SAVE_FLASHRAM, &info),
          T64_STATUS_SAVE_UNSUPPORTED_DEVICE, "save_info of FLASHRAM (R5)", machine, before);
  refused(&tally,
          t64_machine_save_import(machine, T64_SAVE_TRANSFER_PAK, T64_SAVE_FORMAT_RAW, pattern,
                                  T64C_SAVE_BYTES),
          T64_STATUS_SAVE_UNSUPPORTED_DEVICE, "save_import of the transfer pak (R5)", machine,
          before);
  refused(&tally, t64_machine_save_info(machine, T64_SAVE_EEPROM_4K, &info),
          T64_STATUS_SAVE_DEVICE_MISMATCH, "save_info of EEPROM 4K on an SRAM profile (R6)", machine,
          before);
  refused(&tally,
          t64_machine_save_export(machine, T64_SAVE_EEPROM_16K, T64_SAVE_FORMAT_RAW, out,
                                  T64C_SAVE_BYTES, &length),
          T64_STATUS_SAVE_DEVICE_MISMATCH, "save_export of EEPROM 16K on an SRAM profile (R6)",
          machine, before);
  refused(&tally, t64_machine_save_export(machine, T64_SAVE_SRAM_32K, 0u, out, T64C_SAVE_BYTES, &length),
          T64_STATUS_SAVE_BAD_FORMAT, "save_export in format 0 (R7)", machine, before);
  refused(&tally, t64_machine_save_import(machine, T64_SAVE_SRAM_32K, 4u, pattern, T64C_SAVE_BYTES),
          T64_STATUS_SAVE_BAD_FORMAT, "save_import in format 4 (R7)", machine, before);
  refused(&tally,
          t64_machine_save_import(machine, T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_RAW, pattern,
                                  T64C_SAVE_BYTES - 1u),
          T64_STATUS_SAVE_SIZE_MISMATCH, "save_import one byte short (R8)", machine, before);
  refused(&tally,
          t64_machine_save_import(machine, T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_RAW, pattern,
                                  T64C_SAVE_BYTES + 1u),
          T64_STATUS_SAVE_SIZE_MISMATCH, "save_import one byte long (R8)", machine, before);
  poison(out, T64C_SAVE_BYTES);
  length = 0u;
  refused(&tally,
          t64_machine_save_export(machine, T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_RAW, out,
                                  T64C_SAVE_BYTES - 1u, &length),
          T64_STATUS_BUFFER_TOO_SMALL, "save_export into a buffer one byte short (R8)", machine,
          before);
  untouched(&tally, length == T64C_SAVE_BYTES && all_sentinel(out, T64C_SAVE_BYTES),
            "the short export reports 32768 and leaves the buffer untouched");
  {
    uint64_t now = 99u;

    accepted(&tally, save_generation(machine, &now) && now == generation_before,
             "no refusal moved write_generation");
  }

  /* accepted: import then export in every byte order */
  accepted(&tally,
           t64_machine_save_import(machine, T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_RAW, pattern,
                                   T64C_SAVE_BYTES) == T64_STATUS_OK &&
               save_generation(machine, &generation) && generation == 1u,
           "save_import RAW of a 32768-byte pattern; write_generation becomes 1");
  accepted(&tally,
           t64_machine_save_export(machine, T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_RAW, out,
                                   T64C_SAVE_BYTES, &length) == T64_STATUS_OK &&
               length == T64C_SAVE_BYTES && memcmp(out, pattern, T64C_SAVE_BYTES) == 0,
           "save_export RAW returns the imported bytes");
  accepted(&tally,
           t64_machine_save_export(machine, T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_SWAP16, out,
                                   T64C_SAVE_BYTES, &length) == T64_STATUS_OK &&
               memcmp(out, swapped, T64C_SAVE_BYTES) == 0,
           "save_export SWAP16 swaps the two bytes of every 16-bit unit");
  accepted(&tally,
           t64_machine_save_export(machine, T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_WORDREV32, out,
                                   T64C_SAVE_BYTES, &length) == T64_STATUS_OK &&
               memcmp(out, reversed, T64C_SAVE_BYTES) == 0,
           "save_export WORDREV32 reverses the four bytes of every 32-bit unit");
  accepted(&tally,
           t64_machine_save_import(machine, T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_SWAP16, pattern,
                                   T64C_SAVE_BYTES) == T64_STATUS_OK &&
               t64_machine_save_export(machine, T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_RAW, out,
                                       T64C_SAVE_BYTES, &length) == T64_STATUS_OK &&
               memcmp(out, swapped, T64C_SAVE_BYTES) == 0 && save_generation(machine, &generation) &&
               generation == 2u,
           "save_import SWAP16 stores the swapped bytes; write_generation becomes 2");

  /* refused: SAVE_BUSY while a PI DMA to the SRAM window is in flight (R9);
   * export has no R9 and still returns the committed bytes */
  T64C_CHECK_U32(&tally, t64_machine_bus_write(machine, UINT32_C(0x04600000), 4u, UINT32_C(0x1000)),
                 T64_STATUS_OK);
  T64C_CHECK_U32(&tally,
                 t64_machine_bus_write(machine, UINT32_C(0x04600004), 4u, UINT32_C(0x08000000)),
                 T64_STATUS_OK);
  T64C_CHECK_U32(&tally, t64_machine_bus_write(machine, UINT32_C(0x04600008), 4u, UINT32_C(0x3f)),
                 T64_STATUS_OK);
  accepted(&tally,
           t64_machine_save_info(machine, T64_SAVE_SRAM_32K, &info) == T64_STATUS_OK &&
               (info.flags & T64_SAVE_INFO_BUSY) != 0u,
           "a PI DMA into the SRAM window sets T64_SAVE_INFO_BUSY");
  T64C_CHECK(&tally, digest_of(machine, before));
  refused(&tally,
          t64_machine_save_import(machine, T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_RAW, pattern,
                                  T64C_SAVE_BYTES),
          T64_STATUS_SAVE_BUSY, "save_import while the SRAM DMA is active (R9)", machine, before);
  accepted(&tally,
           t64_machine_save_export(machine, T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_RAW, out,
                                   T64C_SAVE_BYTES, &length) == T64_STATUS_OK,
           "save_export while busy still answers with the committed bytes (no R9)");

  free(pattern);
  free(swapped);
  free(reversed);
  free(out);
  accepted(&tally, close_balanced(fixture), "destroy balanced the ledger");
  accepted(&tally, close_balanced(empty), "destroy of the second instance balanced its ledger");
  return t64c_report("ops-save", &tally);
}

/* ---- ops-state ------------------------------------------------------------- */

#define T64C_STATE_TRAILER 32u

/* Recompute the whole-blob SHA-256 trailer with the consumer's own hash, so a
 * blob edited past the integrity check reaches the identity checks behind it. */
static void state_fix_trailer(uint8_t *blob, size_t length) {
  t64c_sha256(blob, length - T64C_STATE_TRAILER, blob + length - T64C_STATE_TRAILER);
}

/* An import that must be refused: the exact status, the report's status and
 * reason, and an unchanged machine digest. */
static void state_refusal(t64c_tally *tally, t64_machine *machine, const uint8_t before[T64_DIGEST_SIZE],
                          const char *label, const uint8_t *blob, size_t length, uint32_t flags,
                          t64_status expected, uint32_t reason) {
  t64_state_import_report report = T64_STATE_IMPORT_REPORT_INIT;
  const t64_status status = t64_machine_state_import(machine, blob, length, flags, &report);

  refused(tally, status, expected, label, machine, before);
  tally->checks++;
  if (report.status != expected || report.reason != reason ||
      report.struct_size != (uint32_t)sizeof report) {
    tally->failures++;
    (void)printf("refused: %s: report status=%u reason=%u (expected %u and %u) FAILED\n", label,
                 (unsigned)report.status, (unsigned)report.reason, (unsigned)expected,
                 (unsigned)reason);
  } else {
    (void)printf("refused: %s: report status=%u reason=%u, struct_size kept\n", label,
                 (unsigned)report.status, (unsigned)report.reason);
  }
}

static int case_state(const t64c_args *args) {
  t64c_tally tally = {0ul, 0ul};
  t64c_fixture *fixture = open_admitted(args);
  t64c_fixture *twin = open_admitted(args);
  t64c_fixture *empty = t64c_fixture_new(args->rom_path);
  t64_machine *machine;
  uint8_t d1[T64_DIGEST_SIZE];
  uint8_t d2[T64_DIGEST_SIZE];
  uint8_t now[T64_DIGEST_SIZE];
  uint8_t trailer[T64C_STATE_TRAILER];
  uint8_t *blob = NULL;
  uint8_t *probe = NULL;
  size_t length = 0u;
  size_t required = 0u;
  uint8_t saved;
  t64_state_import_report report = T64_STATE_IMPORT_REPORT_INIT;

  if (fixture == NULL || twin == NULL || empty == NULL) {
    t64c_fixture_free(fixture);
    t64c_fixture_free(twin);
    t64c_fixture_free(empty);
    return 2;
  }
  machine = fixture->machine;

  /* refused before an image exists */
  refused(&tally, t64_machine_state_export(empty->machine, NULL, 0u, &length), T64_STATUS_NOT_READY,
          "state_export before an image is admitted", empty->machine, NULL);
  {
    uint8_t one[1] = {0u};
    t64_state_import_report fresh = T64_STATE_IMPORT_REPORT_INIT;

    refused(&tally, t64_machine_state_import(empty->machine, one, sizeof one, 0u, &fresh),
            T64_STATUS_NOT_READY, "state_import before an image is admitted", empty->machine, NULL);
  }

  /* accepted: size query, exact export */
  T64C_CHECK(&tally, advance_to(machine, UINT64_C(3000), NULL));
  T64C_CHECK(&tally, digest_of(machine, d1));
  length = 0u;
  accepted(&tally,
           t64_machine_state_export(machine, NULL, 0u, &length) == T64_STATUS_BUFFER_TOO_SMALL &&
               length > 0u,
           "state_export size query (NULL, capacity 0) reports the container length");
  required = length;
  blob = (uint8_t *)malloc(required);
  probe = (uint8_t *)malloc(required);
  if (blob == NULL || probe == NULL) {
    free(blob);
    free(probe);
    t64c_fixture_free(fixture);
    t64c_fixture_free(twin);
    t64c_fixture_free(empty);
    return 2;
  }
  poison(probe, required);
  length = 0u;
  refused(&tally, t64_machine_state_export(machine, probe, required - 1u, &length),
          T64_STATUS_BUFFER_TOO_SMALL, "state_export into a buffer one byte short", machine, d1);
  untouched(&tally, length == required && all_sentinel(probe, required),
            "the short export reports the required length and writes no byte");
  refused(&tally, t64_machine_state_export(NULL, probe, required, &length),
          T64_STATUS_INVALID_ARGUMENT, "state_export on a NULL machine", machine, d1);
  refused(&tally, t64_machine_state_export(machine, probe, required, NULL),
          T64_STATUS_INVALID_ARGUMENT, "state_export with a NULL length output", machine, d1);
  accepted(&tally,
           t64_machine_state_export(machine, blob, required, &length) == T64_STATUS_OK &&
               length == required && memcmp(blob, "T64STATE", 8u) == 0 && digest_of(machine, now) &&
               memcmp(now, d1, sizeof now) == 0,
           "state_export into an exact buffer: a T64STATE container, the machine unchanged");
  accepted(&tally,
           memcmp(blob + 0x70, d1, T64_DIGEST_SIZE) == 0,
           "the container's declared canonical digest equals t64_machine_digest");

  /* accepted: roll back with an import, then run on identically */
  T64C_CHECK(&tally, advance_to(machine, UINT64_C(6000), NULL));
  T64C_CHECK(&tally, digest_of(machine, d2));
  accepted(&tally, memcmp(d1, d2, sizeof d1) != 0, "advancing changed the machine digest");
  accepted(&tally,
           t64_machine_state_import(machine, blob, required, 0u, &report) == T64_STATUS_OK &&
               report.status == T64_STATUS_OK && report.reason == T64_STATE_REASON_NONE &&
               report.struct_size == (uint32_t)sizeof report && digest_of(machine, now) &&
               memcmp(now, d1, sizeof now) == 0,
           "state_import of the exported container restores the exported digest, report OK");
  T64C_CHECK(&tally, advance_to(machine, UINT64_C(6000), NULL));
  accepted(&tally, digest_of(machine, now) && memcmp(now, d2, sizeof now) == 0,
           "advancing again from the imported state reaches the same digest as before");
  accepted(&tally,
           t64_machine_state_import(twin->machine, blob, required, 0u, &report) == T64_STATUS_OK &&
               digest_of(twin->machine, now) && memcmp(now, d1, sizeof now) == 0 &&
               advance_to(twin->machine, UINT64_C(6000), NULL) && digest_of(twin->machine, now) &&
               memcmp(now, d2, sizeof now) == 0,
           "a second instance imports the container and reaches the same digests");
  accepted(&tally,
           t64_machine_state_import(machine, blob, required, T64_STATE_IMPORT_DROP_OBSERVERS, NULL) ==
                   T64_STATUS_OK &&
               digest_of(machine, now) && memcmp(now, d1, sizeof now) == 0,
           "state_import with DROP_OBSERVERS and a NULL report is accepted, same canonical digest");
  T64C_LARGE_OUT("state_import with a report larger than v1 (trailing bytes untouched)",
                 t64_state_import_report, T64_STATE_IMPORT_REPORT_INIT,
                 t64_machine_state_import(machine, blob, required, 0u, &big.value),
                 big.value.status == T64_STATUS_OK && big.value.reason == T64_STATE_REASON_NONE);

  /* refused: hostile blobs, the documented check order, machine unchanged */
  T64C_CHECK(&tally, digest_of(machine, d1));
  state_refusal(&tally, machine, d1, "state_import with an unknown flag bit", blob, required, 0x2u,
                T64_STATUS_INVALID_ARGUMENT, T64_STATE_REASON_ARG_FLAGS);
  state_refusal(&tally, machine, d1, "state_import of length 0", blob, 0u, 0u,
                T64_STATUS_INVALID_ARGUMENT, T64_STATE_REASON_ARG_LENGTH);
  state_refusal(&tally, machine, d1, "state_import of a NULL blob", NULL, required, 0u,
                T64_STATUS_INVALID_ARGUMENT, T64_STATE_REASON_ARG_NULL);
  state_refusal(&tally, machine, d1, "state_import of a truncated blob (one byte short)", blob,
                required - 1u, 0u, T64_STATUS_STATE_FORMAT, T64_STATE_REASON_TOTAL_LENGTH);
  saved = blob[0];
  blob[0] = (uint8_t)(blob[0] ^ 1u);
  state_refusal(&tally, machine, d1, "state_import with a wrong magic", blob, required, 0u,
                T64_STATUS_STATE_FORMAT, T64_STATE_REASON_MAGIC);
  blob[0] = saved;
  saved = blob[0x0b];
  blob[0x0b] = 9u;
  state_refusal(&tally, machine, d1, "state_import with an unknown container version", blob, required,
                0u, T64_STATUS_STATE_FORMAT, T64_STATE_REASON_CONTAINER_VERSION);
  blob[0x0b] = saved;
  saved = blob[required / 2u];
  blob[required / 2u] = (uint8_t)(saved ^ 1u);
  state_refusal(&tally, machine, d1, "state_import with one flipped body byte", blob, required, 0u,
                T64_STATUS_STATE_INTEGRITY, T64_STATE_REASON_TRAILER_SHA256);
  blob[required / 2u] = saved;
  (void)memcpy(trailer, blob + required - T64C_STATE_TRAILER, sizeof trailer);
  saved = blob[0x20];
  blob[0x20] = (uint8_t)(saved ^ 1u);
  state_fix_trailer(blob, required);
  state_refusal(&tally, machine, d1, "state_import of another profile digest (trailer recomputed)",
                blob, required, 0u, T64_STATUS_STATE_PROFILE, T64_STATE_REASON_PROFILE_DIGEST);
  blob[0x20] = saved;
  saved = blob[0x40];
  blob[0x40] = (uint8_t)(saved ^ 1u);
  state_fix_trailer(blob, required);
  state_refusal(&tally, machine, d1, "state_import of another content digest (trailer recomputed)",
                blob, required, 0u, T64_STATUS_STATE_CONTENT, T64_STATE_REASON_CONTENT_DIGEST);
  blob[0x40] = saved;
  saved = blob[0xa0];
  blob[0xa0] = (uint8_t)(saved ^ 1u);
  state_fix_trailer(blob, required);
  state_refusal(&tally, machine, d1, "state_import of another build identity (trailer recomputed)",
                blob, required, 0u, T64_STATUS_STATE_BUILD, T64_STATE_REASON_BUILD_ID);
  blob[0xa0] = saved;
  (void)memcpy(blob + required - T64C_STATE_TRAILER, trailer, sizeof trailer);
  refused(&tally, t64_machine_state_import(NULL, blob, required, 0u, &report),
          T64_STATUS_INVALID_ARGUMENT, "state_import on a NULL machine", machine, d1);

  free(blob);
  free(probe);
  accepted(&tally, close_balanced(fixture), "destroy balanced the ledger");
  accepted(&tally, close_balanced(twin), "destroy of the second instance balanced its ledger");
  accepted(&tally, close_balanced(empty), "destroy of the third instance balanced its ledger");
  return t64c_report("ops-state", &tally);
}

/* ---- ops-queries ----------------------------------------------------------- */

#define T64C_NOT_READY_OUT(label, type, init_macro, call, digest_arg)         \
  do {                                                                         \
    type out = init_macro;                                                     \
    uint8_t raw[sizeof out];                                                   \
    t64_status nr_status;                                                      \
    poison(&out, sizeof out);                                                  \
    out.struct_size = (uint32_t)sizeof out;                                    \
    (void)memcpy(raw, &out, sizeof out);                                       \
    nr_status = (call);                                                        \
    refused(&tally, nr_status, T64_STATUS_NOT_READY, label, machine, digest_arg); \
    untouched(&tally, memcmp(raw, &out, sizeof out) == 0, label);              \
  } while (0)

static int case_queries(const t64c_args *args) {
  t64c_tally tally = {0ul, 0ul};
  t64c_fixture *fixture = t64c_fixture_new(args->rom_path);
  t64c_run *run = (t64c_run *)calloc(1u, sizeof *run);
  t64_machine *machine;
  uint8_t content[T64_DIGEST_SIZE];
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t header[T64_ROM_HEADER_SIZE];
  uint8_t digest_slot[T64_DIGEST_SIZE];
  uint8_t own[T64C_SHA256_SIZE];
  size_t size_slot = 77u;
  size_t length_slot = 77u;
  t64_output_status ring = T64_OUTPUT_STATUS_INIT;
  const t64_profile_descriptor *shown = NULL;
  uint32_t poisoned_slot = 5u;

  if (fixture == NULL || run == NULL) {
    free(run);
    t64c_fixture_free(fixture);
    return 2;
  }
  machine = fixture->machine;

  /* NOT_READY before admission: every query that needs an image, outputs untouched */
  poison(digest_slot, sizeof digest_slot);
  refused(&tally, t64_machine_digest(machine, digest_slot), T64_STATUS_NOT_READY,
          "digest before an image is admitted", machine, NULL);
  untouched(&tally, all_sentinel(digest_slot, sizeof digest_slot), "digest(NOT_READY) output");
  refused(&tally, t64_machine_content_digest(machine, digest_slot), T64_STATUS_NOT_READY,
          "content_digest before an image is admitted", machine, NULL);
  untouched(&tally, all_sentinel(digest_slot, sizeof digest_slot), "content_digest(NOT_READY) output");
  refused(&tally, t64_machine_rom_size(machine, &size_slot), T64_STATUS_NOT_READY,
          "rom_size before an image is admitted", machine, NULL);
  poison(header, sizeof header);
  refused(&tally, t64_machine_rom_header(machine, header, sizeof header, &length_slot),
          T64_STATUS_NOT_READY, "rom_header before an image is admitted", machine, NULL);
  untouched(&tally, length_slot == 77u && all_sentinel(header, sizeof header),
            "rom_header(NOT_READY) outputs");
  T64C_NOT_READY_OUT("cpu_stop_detail before an image is admitted", t64_cpu_stop_detail,
                     T64_CPU_STOP_DETAIL_INIT, t64_machine_cpu_stop_detail(machine, &out), NULL);
  accepted(&tally,
           t64_machine_output_status(machine, &ring) == T64_STATUS_OK && ring.video_queued == 0u &&
               ring.video_free == 2u && ring.audio_queued == 0u && ring.audio_free == 2u &&
               ring.text_pending_bytes == 0u,
           "output_status needs no image: zero counts, both rings free");
  accepted(&tally, t64_machine_is_poisoned(machine, &poisoned_slot) == T64_STATUS_OK && poisoned_slot == 0u,
           "is_poisoned needs no image and reports 0");

  /* accepted after admission */
  T64C_CHECK_U32(&tally, t64c_fixture_admit(fixture), T64_STATUS_OK);
  T64C_CHECK(&tally, digest_of(machine, before));
  accepted(&tally,
           t64_machine_content_digest(machine, content) == T64_STATUS_OK &&
               (t64c_sha256(fixture->rom, fixture->rom_length, own), true) &&
               memcmp(content, own, sizeof own) == 0,
           "content_digest equals the consumer's own SHA-256 of the ROM file bytes");
  accepted(&tally,
           t64_machine_rom_size(machine, &size_slot) == T64_STATUS_OK &&
               size_slot == fixture->rom_length,
           "rom_size equals the file length");
  accepted(&tally,
           t64_machine_rom_header(machine, header, sizeof header, &length_slot) == T64_STATUS_OK &&
               length_slot == T64_ROM_HEADER_SIZE && memcmp(header, fixture->rom, sizeof header) == 0,
           "rom_header copies the 64 header bytes of the file");
  accepted(&tally,
           t64_machine_profile(machine, &shown) == T64_STATUS_OK && shown != NULL &&
               strcmp(shown->profile_id, T64C_PROFILE_ID) == 0,
           "machine_profile names the composite profile");

  /* refused: NULL machine on every query, outputs untouched */
  poison(digest_slot, sizeof digest_slot);
  refused(&tally, t64_machine_digest(NULL, digest_slot), T64_STATUS_INVALID_ARGUMENT,
          "digest of a NULL machine", machine, before);
  untouched(&tally, all_sentinel(digest_slot, sizeof digest_slot), "digest(NULL machine) output");
  refused(&tally, t64_machine_content_digest(NULL, digest_slot), T64_STATUS_INVALID_ARGUMENT,
          "content_digest of a NULL machine", machine, before);
  size_slot = 77u;
  refused(&tally, t64_machine_rom_size(NULL, &size_slot), T64_STATUS_INVALID_ARGUMENT,
          "rom_size of a NULL machine", machine, before);
  untouched(&tally, size_slot == 77u, "rom_size(NULL machine) output");
  refused(&tally, t64_machine_digest(machine, NULL), T64_STATUS_INVALID_ARGUMENT,
          "digest into a NULL output", machine, before);
  refused(&tally, t64_machine_rom_size(machine, NULL), T64_STATUS_INVALID_ARGUMENT,
          "rom_size into a NULL output", machine, before);
  length_slot = 0u;
  refused(&tally, t64_machine_rom_header(machine, NULL, 0u, &length_slot), T64_STATUS_BUFFER_TOO_SMALL,
          "rom_header size query (NULL, capacity 0)", machine, before);
  untouched(&tally, length_slot == T64_ROM_HEADER_SIZE, "rom_header size query reports 64");
  poison(header, sizeof header);
  refused(&tally, t64_machine_rom_header(machine, header, sizeof header - 1u, &length_slot),
          T64_STATUS_BUFFER_TOO_SMALL, "rom_header into 63 bytes", machine, before);
  untouched(&tally, all_sentinel(header, sizeof header), "rom_header(short) bytes");
  refused(&tally, t64_machine_rom_header(machine, NULL, sizeof header, &length_slot),
          T64_STATUS_INVALID_ARGUMENT, "rom_header into a NULL buffer with a capacity", machine, before);
  refused(&tally, t64_machine_profile(NULL, &shown), T64_STATUS_INVALID_ARGUMENT,
          "machine_profile of a NULL machine", machine, before);
  refused(&tally, t64_machine_output_status(NULL, &ring), T64_STATUS_INVALID_ARGUMENT,
          "output_status of a NULL machine", machine, before);
  refused(&tally, t64_machine_output_status(machine, NULL), T64_STATUS_INVALID_ARGUMENT,
          "output_status into a NULL output", machine, before);

  /* cpu_stop_detail: NOT_READY until the guest stops, then the terminal detail */
  T64C_NOT_READY_OUT("cpu_stop_detail before the guest stops", t64_cpu_stop_detail,
                     T64_CPU_STOP_DETAIL_INIT, t64_machine_cpu_stop_detail(machine, &out), before);
  T64C_CHECK(&tally, t64c_queue_literal_inputs(fixture));
  T64C_CHECK(&tally, t64c_run_to_completion(fixture, args->slice, run));
  T64C_CHECK(&tally, digest_of(machine, before));
  {
    t64_cpu_stop_detail detail = T64_CPU_STOP_DETAIL_INIT;

    accepted(&tally,
             t64_machine_cpu_stop_detail(machine, &detail) == T64_STATUS_OK &&
                 detail.retired_instructions > 0u && detail.struct_size == (uint32_t)sizeof detail,
             "cpu_stop_detail after the terminal stop: retired instructions, pc and opcode");
  }
  T64C_LARGE_OUT("cpu_stop_detail with a struct larger than v1 (trailing bytes untouched)",
                 t64_cpu_stop_detail, T64_CPU_STOP_DETAIL_INIT,
                 t64_machine_cpu_stop_detail(machine, &big.value),
                 big.value.retired_instructions > 0u);
  refused(&tally, t64_machine_cpu_stop_detail(machine, NULL), T64_STATUS_INVALID_ARGUMENT,
          "cpu_stop_detail into a NULL output", machine, before);
  refused(&tally, t64_machine_cpu_stop_detail(NULL, &(t64_cpu_stop_detail)T64_CPU_STOP_DETAIL_INIT),
          T64_STATUS_INVALID_ARGUMENT, "cpu_stop_detail of a NULL machine", machine, before);

  free(run);
  accepted(&tally, close_balanced(fixture), "destroy balanced the ledger");
  return t64c_report("ops-queries", &tally);
}

/* ---- ops-trace ------------------------------------------------------------- */

typedef struct {
  t64_trace_record record;
  uint8_t tail[8];
} t64c_wide_record;

static int case_trace(const t64c_args *args) {
  t64c_tally tally = {0ul, 0ul};
  t64c_fixture *traced = open_admitted(args);
  t64c_fixture *plain = open_admitted(args);
  t64_machine *machine;
  t64_trace_record records[T64_TRACE_RECORD_CAPACITY];
  t64c_wide_record wide[4];
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t d_plain[T64_DIGEST_SIZE];
  uint8_t d_traced[T64_DIGEST_SIZE];
  size_t count = 99u;
  uint64_t dropped = 99u;
  bool saturated = true;
  size_t first_count;
  size_t i;
  bool ordered = true;

  if (traced == NULL || plain == NULL) {
    t64c_fixture_free(traced);
    t64c_fixture_free(plain);
    return 2;
  }
  machine = traced->machine;

  /* refused: a NULL machine (tracing itself needs no image) */
  refused(&tally, t64_machine_trace_set_enabled(NULL, true), T64_STATUS_INVALID_ARGUMENT,
          "trace_set_enabled on a NULL machine", NULL, NULL);

  /* accepted: tracing is observer-only (the digest equals an untraced run) */
  accepted(&tally, t64_machine_trace_set_enabled(machine, true) == T64_STATUS_OK,
           "trace_set_enabled(true)");
  T64C_CHECK(&tally, advance_to(machine, UINT64_C(3000), NULL));
  T64C_CHECK(&tally, advance_to(plain->machine, UINT64_C(3000), NULL));
  accepted(&tally,
           digest_of(machine, d_traced) && digest_of(plain->machine, d_plain) &&
               memcmp(d_traced, d_plain, sizeof d_traced) == 0,
           "the machine digest of a traced run equals that of an untraced run");

  /* accepted: drain at record_size v1, FIFO, a second drain finds the queue empty */
  T64C_CHECK(&tally, digest_of(machine, before));
  poison(records, sizeof records);
  count = 0u;
  dropped = 99u;
  saturated = true;
  accepted(&tally,
           t64_machine_trace_drain(machine, NULL, (uint32_t)sizeof records[0], 0u, &count, &dropped,
                                   &saturated) == T64_STATUS_OK &&
               count == 0u,
           "trace_drain with capacity 0 and a NULL buffer copies nothing");
  accepted(&tally,
           t64_machine_trace_drain(machine, records, (uint32_t)sizeof records[0],
                                   T64_TRACE_RECORD_CAPACITY, &count, &dropped, &saturated) ==
                   T64_STATUS_OK &&
               count > 0u && count <= T64_TRACE_RECORD_CAPACITY && !saturated,
           "trace_drain returns the queued records of the first 3000 ticks");
  first_count = count;
  for (i = 1u; i < count; i++) {
    ordered = ordered && records[i].tick >= records[i - 1u].tick;
  }
  accepted(&tally, ordered, "the drained records are in FIFO order (ticks never decrease)");
  accepted(&tally,
           t64_machine_trace_drain(machine, records, (uint32_t)sizeof records[0],
                                   T64_TRACE_RECORD_CAPACITY, &count, &dropped, &saturated) ==
                   T64_STATUS_OK &&
               (count == 0u || count < first_count),
           "a second drain does not return the records again");

  /* a record_size above v1: the stride is the caller's, bytes behind v1 untouched */
  T64C_CHECK(&tally, advance_to(machine, UINT64_C(6000), NULL));
  poison(wide, sizeof wide);
  count = 0u;
  accepted(&tally,
           t64_machine_trace_drain(machine, &wide[0].record, (uint32_t)sizeof wide[0], 4u, &count,
                                   &dropped, &saturated) == T64_STATUS_OK &&
               count > 0u && count <= 4u,
           "trace_drain with a record_size larger than v1 fills the caller's stride");
  {
    bool tails = true;

    for (i = 0u; i < count; i++) {
      tails = tails && all_sentinel(wide[i].tail, sizeof wide[i].tail);
    }
    accepted(&tally, tails, "the bytes behind each v1 record are untouched");
  }

  /* refused: record_size below v1 and NULL outputs leave the queue alone */
  T64C_CHECK(&tally, digest_of(machine, before));
  refused(&tally,
          t64_machine_trace_drain(machine, records, (uint32_t)sizeof records[0] - 1u, 4u, &count,
                                  &dropped, &saturated),
          T64_STATUS_STRUCT_TOO_SMALL, "trace_drain with a record_size one byte below v1", machine,
          before);
  refused(&tally,
          t64_machine_trace_drain(machine, records, (uint32_t)sizeof records[0], 4u, NULL, &dropped,
                                  &saturated),
          T64_STATUS_INVALID_ARGUMENT, "trace_drain with a NULL count output", machine, before);
  refused(&tally,
          t64_machine_trace_drain(machine, NULL, (uint32_t)sizeof records[0], 4u, &count, &dropped,
                                  &saturated),
          T64_STATUS_INVALID_ARGUMENT, "trace_drain into a NULL buffer with a capacity", machine,
          before);
  refused(&tally,
          t64_machine_trace_drain(NULL, records, (uint32_t)sizeof records[0], 4u, &count, &dropped,
                                  &saturated),
          T64_STATUS_INVALID_ARGUMENT, "trace_drain on a NULL machine", machine, before);
  accepted(&tally, t64_machine_trace_set_enabled(machine, false) == T64_STATUS_OK,
           "trace_set_enabled(false)");

  accepted(&tally, close_balanced(traced), "destroy balanced the ledger");
  accepted(&tally, close_balanced(plain), "destroy of the second instance balanced its ledger");
  return t64c_report("ops-trace", &tally);
}

/* ---- ops-bus --------------------------------------------------------------- */

static int case_bus(const t64c_args *args) {
  t64c_tally tally = {0ul, 0ul};
  t64c_fixture *fixture = t64c_fixture_new(args->rom_path);
  t64_machine *machine;
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  uint32_t value = 0u;
  uint32_t slot = UINT32_C(0x55555555);
  t64_trace_record records[8];
  size_t count = 0u;
  uint64_t dropped = 0u;
  bool saturated = false;

  if (fixture == NULL) {
    return 2;
  }
  machine = fixture->machine;
  T64C_CHECK_U32(&tally, t64c_fixture_admit(fixture), T64_STATUS_OK);

  /* accepted: the cartridge window reads the image bytes big-endian at every width */
  accepted(&tally,
           t64_machine_bus_read(machine, UINT32_C(0x10000000), 4u, &value) == T64_STATUS_OK &&
               value == UINT32_C(0x80371240),
           "bus_read of the cartridge window word equals the file's first four bytes");
  accepted(&tally,
           t64_machine_bus_read(machine, UINT32_C(0x10000000), 2u, &value) == T64_STATUS_OK &&
               value == UINT32_C(0x8037) &&
               t64_machine_bus_read(machine, UINT32_C(0x10000000), 1u, &value) == T64_STATUS_OK &&
               value == UINT32_C(0x80),
           "bus_read of the halfword and the byte equal the file's first bytes");

  /* accepted: RDRAM writes at every width, read back, the digest follows the write */
  T64C_CHECK(&tally, digest_of(machine, before));
  accepted(&tally,
           t64_machine_bus_write(machine, UINT32_C(0x00001000), 4u, UINT32_C(0xdeadbeef)) ==
                   T64_STATUS_OK &&
               t64_machine_bus_read(machine, UINT32_C(0x00001000), 4u, &value) == T64_STATUS_OK &&
               value == UINT32_C(0xdeadbeef) && digest_of(machine, after) &&
               memcmp(before, after, sizeof before) != 0,
           "bus_write of an RDRAM word reads back and changes the machine digest");
  accepted(&tally,
           t64_machine_bus_write(machine, UINT32_C(0x00001002), 2u, UINT32_C(0xabcd)) ==
                   T64_STATUS_OK &&
               t64_machine_bus_write(machine, UINT32_C(0x00001003), 1u, UINT32_C(0x11)) ==
                   T64_STATUS_OK &&
               t64_machine_bus_read(machine, UINT32_C(0x00001000), 4u, &value) == T64_STATUS_OK &&
               value == UINT32_C(0xdeadab11),
           "halfword and byte writes land big-endian inside the word");
  T64C_CHECK(&tally, digest_of(machine, before));

  /* refused: address, width, alignment and range; value_out stays, digest stays */
  slot = UINT32_C(0x55555555);
  refused(&tally, t64_machine_bus_read(machine, UINT32_C(0x80000000), 4u, &slot),
          T64_STATUS_BUS_UNMAPPED, "bus_read of an unmapped address", machine, before);
  untouched(&tally, slot == UINT32_C(0x55555555), "bus_read(unmapped) output");
  refused(&tally, t64_machine_bus_write(machine, UINT32_C(0x80000000), 4u, 1u),
          T64_STATUS_BUS_UNMAPPED, "bus_write of an unmapped address", machine, before);
  refused(&tally, t64_machine_bus_read(machine, UINT32_C(0x00800000), 4u, &slot),
          T64_STATUS_BUS_UNSUPPORTED, "bus_read of a recognized, unsupported window", machine, before);
  refused(&tally, t64_machine_bus_read(machine, UINT32_C(0x00000001), 4u, &slot),
          T64_STATUS_BUS_ALIGNMENT, "bus_read of a misaligned word", machine, before);
  refused(&tally, t64_machine_bus_write(machine, UINT32_C(0x00000001), 2u, 1u),
          T64_STATUS_BUS_ALIGNMENT, "bus_write of a misaligned halfword", machine, before);
  refused(&tally, t64_machine_bus_read(machine, UINT32_C(0xfffffffe), 4u, &slot),
          T64_STATUS_BUS_RANGE, "bus_read leaving the 32-bit address space", machine, before);
  refused(&tally, t64_machine_bus_read(machine, UINT32_C(0x10000000), 3u, &slot),
          T64_STATUS_INVALID_ARGUMENT, "bus_read of width 3", machine, before);
  refused(&tally, t64_machine_bus_write(machine, UINT32_C(0x00001000), 8u, 1u),
          T64_STATUS_INVALID_ARGUMENT, "bus_write of width 8", machine, before);
  refused(&tally, t64_machine_bus_read(machine, UINT32_C(0x10000000), 4u, NULL),
          T64_STATUS_INVALID_ARGUMENT, "bus_read into a NULL output", machine, before);
  refused(&tally, t64_machine_bus_read(NULL, UINT32_C(0x10000000), 4u, &slot),
          T64_STATUS_INVALID_ARGUMENT, "bus_read on a NULL machine", machine, before);
  untouched(&tally, slot == UINT32_C(0x55555555), "the refused bus_reads left the output alone");

  /* the carve-out: a device refusal with no record to keep changes nothing */
  refused(&tally, t64_machine_bus_write(machine, UINT32_C(0x10000000), 4u, UINT32_C(0xdeadbeef)),
          T64_STATUS_BUS_DEVICE_REFUSED,
          "bus_write into the read-only cartridge window (device refusal, no latch)", machine, before);

  /* the carve-out: a PI request the device rejects latches its own records and
   * leaves every public observation alone. The write is refused, the digest may
   * move (the latched length register) and exactly one PI_REFUSED record appears. */
  {
    uint8_t content_before[T64_DIGEST_SIZE];
    uint8_t content_after[T64_DIGEST_SIZE];
    size_t size_before = 0u;
    size_t size_after = 0u;
    t64_save_info save_before = T64_SAVE_INFO_INIT;
    t64_save_info save_after = T64_SAVE_INFO_INIT;

    T64C_CHECK_U32(&tally, t64_machine_trace_set_enabled(machine, true), T64_STATUS_OK);
    T64C_CHECK_U32(&tally,
                   t64_machine_trace_drain(machine, records, (uint32_t)sizeof records[0], 8u, &count,
                                           &dropped, &saturated),
                   T64_STATUS_OK);
    T64C_CHECK_U32(&tally, t64_machine_bus_write(machine, UINT32_C(0x04600000), 4u, UINT32_C(0x1000)),
                   T64_STATUS_OK);
    T64C_CHECK_U32(&tally,
                   t64_machine_bus_write(machine, UINT32_C(0x04600004), 4u, UINT32_C(0x10100000)),
                   T64_STATUS_OK);
    T64C_CHECK_U32(&tally,
                   t64_machine_trace_drain(machine, records, (uint32_t)sizeof records[0], 8u, &count,
                                           &dropped, &saturated),
                   T64_STATUS_OK);
    T64C_CHECK(&tally,
               t64_machine_content_digest(machine, content_before) == T64_STATUS_OK &&
                   t64_machine_rom_size(machine, &size_before) == T64_STATUS_OK &&
                   t64_machine_save_info(machine, T64_SAVE_SRAM_32K, &save_before) == T64_STATUS_OK);
    refused(&tally, t64_machine_bus_write(machine, UINT32_C(0x0460000c), 4u, UINT32_C(0x3f)),
            T64_STATUS_BUS_DEVICE_REFUSED,
            "bus_write of a PI request the device rejects (carve-out: latches its own records)",
            NULL, NULL);
    count = 0u;
    T64C_CHECK_U32(&tally,
                   t64_machine_trace_drain(machine, records, (uint32_t)sizeof records[0], 8u, &count,
                                           &dropped, &saturated),
                   T64_STATUS_OK);
    accepted(&tally,
             count == 1u && records[0].device == T64_TRACE_DEVICE_PI &&
                 records[0].kind == T64_TRACE_KIND_PI_REFUSED,
             "exactly one trace record appeared: the PI refusal");
    accepted(&tally,
             t64_machine_content_digest(machine, content_after) == T64_STATUS_OK &&
                 t64_machine_rom_size(machine, &size_after) == T64_STATUS_OK &&
                 t64_machine_save_info(machine, T64_SAVE_SRAM_32K, &save_after) == T64_STATUS_OK &&
                 memcmp(content_before, content_after, sizeof content_before) == 0 &&
                 size_before == size_after && memcmp(&save_before, &save_after, sizeof save_before) == 0,
             "the content digest, the ROM size and the save info did not move");
  }
  accepted(&tally, close_balanced(fixture), "destroy balanced the ledger");
  return t64c_report("ops-bus", &tally);
}

/* ---- ops-poison-query ------------------------------------------------------ */

static int case_poison_query(const t64c_args *args) {
  t64c_tally tally = {0ul, 0ul};
  t64c_fixture *fixture = t64c_fixture_new(args->rom_path);
  t64_machine *machine;
  uint32_t slot = UINT32_C(0x5a5a5a5a);
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];

  if (fixture == NULL) {
    return 2;
  }
  machine = fixture->machine;
  accepted(&tally, t64_machine_is_poisoned(machine, &slot) == T64_STATUS_OK && slot == 0u,
           "a fresh instance is not poisoned");
  T64C_CHECK_U32(&tally, t64c_fixture_admit(fixture), T64_STATUS_OK);
  accepted(&tally, t64_machine_is_poisoned(machine, &slot) == T64_STATUS_OK && slot == 0u,
           "an instance with an admitted image is not poisoned");
  slot = UINT32_C(0x5a5a5a5a);
  T64C_CHECK(&tally, digest_of(machine, before));
  refused(&tally, t64_machine_is_poisoned(NULL, &slot), T64_STATUS_INVALID_ARGUMENT,
          "is_poisoned of a NULL machine", machine, before);
  untouched(&tally, slot == UINT32_C(0x5a5a5a5a), "is_poisoned(NULL machine) output");
  refused(&tally, t64_machine_is_poisoned(machine, NULL), T64_STATUS_INVALID_ARGUMENT,
          "is_poisoned into a NULL output", machine, before);

  /* a caller fault never poisons: a run of refused calls leaves it at 0 */
  (void)t64_machine_queue_input(machine, UINT64_C(1), 1u, 0u, 0, 0);
  (void)t64_machine_advance(machine, &(t64_stop_request)T64_STOP_REQUEST_INIT(99u, UINT64_C(1)),
                            &(t64_stop_result)T64_STOP_RESULT_INIT);
  (void)t64_machine_admit_rom(machine, fixture->rom, 3u);
  (void)t64_machine_bus_read(machine, UINT32_C(0x80000000), 4u, &slot);
  (void)t64_machine_save_import(machine, 9u, 1u, fixture->rom, 1u);
  slot = UINT32_C(0x5a5a5a5a);
  accepted(&tally,
           t64_machine_is_poisoned(machine, &slot) == T64_STATUS_OK && slot == 0u &&
               digest_of(machine, after),
           "after refused calls of every kind the instance is still not poisoned");
  accepted(&tally, close_balanced(fixture), "destroy balanced the ledger");
  return t64c_report("ops-poison-query", &tally);
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
    {"ops-unload", case_unload},       {"ops-rings", case_rings},
    {"ops-text", case_text},           {"ops-save", case_save},
    {"ops-state", case_state},         {"ops-queries", case_queries},
    {"ops-trace", case_trace},         {"ops-bus", case_bus},
    {"ops-poison-query", case_poison_query},
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
