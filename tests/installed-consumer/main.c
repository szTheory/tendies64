/*
 * Independent consumer of the installed tendies64 package (Phase 11, plan
 * 11-16, VER-05). It includes the public header of the package and system
 * headers only, links the archive through find_package(tendies64), and proves
 * that header and library agree.
 *
 *   t64c --list-cases     print the case names, one per line
 *   t64c --case smoke     the tracer case
 *
 * Exit status: 0 the case passed, 1 a check failed or ran no check, 2 usage.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tendies64/t64_machine.h>

#include "t64c_check.h"

#ifndef T64C_EXPECTED_BUILD_ID
#error "T64C_EXPECTED_BUILD_ID is defined by the consumer CMakeLists from tendies64_BUILD_ID"
#endif

/* A counting host allocator with a per-instance context: every allocation is
 * remembered, so a release of an unknown pointer, a double release and a leak
 * are all visible. */
#define T64C_MAX_SLOTS 256u

typedef struct {
  void *data;
  bool active;
} t64c_slot;

typedef struct {
  t64c_slot slots[T64C_MAX_SLOTS];
  size_t slot_count;
  size_t allocations;
  size_t releases;
  size_t bad_releases;
  size_t outstanding;
  size_t refusals;
} t64c_ledger;

static bool t64c_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  t64c_ledger *ledger = (t64c_ledger *)context;
  size_t host_alignment = alignment;
  size_t rounded;
  void *memory;

  if (ledger == NULL || storage_out == NULL || size == 0u || alignment == 0u ||
      (alignment & (alignment - 1u)) != 0u) {
    return false;
  }
  if (host_alignment < sizeof(void *)) {
    host_alignment = sizeof(void *);
  }
  if (ledger->slot_count >= T64C_MAX_SLOTS) {
    ledger->refusals++;
    return false;
  }
  rounded = ((size + host_alignment - 1u) / host_alignment) * host_alignment;
  memory = aligned_alloc(host_alignment, rounded);
  if (memory == NULL) {
    ledger->refusals++;
    return false;
  }
  ledger->slots[ledger->slot_count].data = memory;
  ledger->slots[ledger->slot_count].active = true;
  ledger->slot_count++;
  ledger->allocations++;
  ledger->outstanding++;
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = host_alignment;
  return true;
}

static void t64c_release(void *context, t64_host_storage storage) {
  t64c_ledger *ledger = (t64c_ledger *)context;
  size_t i;

  if (ledger == NULL) {
    return;
  }
  for (i = 0u; i < ledger->slot_count; i++) {
    if (ledger->slots[i].active && ledger->slots[i].data == storage.data) {
      ledger->slots[i].active = false;
      ledger->releases++;
      ledger->outstanding--;
      free(storage.data);
      return;
    }
  }
  ledger->bad_releases++;
}

/* The tracer: the linked archive reports the ABI version of the header it was
 * built with and the build identity the package recorded, and a create and
 * destroy round trip gives every allocation back. */
static int case_smoke(void) {
  t64c_tally tally = {0ul, 0ul};
  t64c_ledger *ledger = (t64c_ledger *)calloc(1u, sizeof *ledger);
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  t64_status status;
  const char *identity;

  if (ledger == NULL) {
    (void)fprintf(stderr, "smoke: out of memory for the ledger\n");
    return 1;
  }

  T64C_CHECK_U32(&tally, t64_abi_version(), T64_ABI_VERSION);
  identity = t64_build_identity();
  T64C_CHECK(&tally, identity != NULL);
  if (identity != NULL) {
    T64C_CHECK(&tally, strcmp(identity, T64C_EXPECTED_BUILD_ID) == 0);
  }

  status = t64_profile_lookup("ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k",
                              &profile);
  T64C_CHECK_U32(&tally, status, T64_STATUS_OK);
  T64C_CHECK(&tally, profile != NULL);

  if (profile != NULL) {
    t64_host_allocator allocator =
        T64_HOST_ALLOCATOR_INIT(t64c_allocate, t64c_release, ledger);
    status = t64_machine_create(
        &(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(&allocator,
                                                               profile),
        &machine);
    T64C_CHECK_U32(&tally, status, T64_STATUS_OK);
    T64C_CHECK(&tally, machine != NULL);
    T64C_CHECK(&tally, ledger->allocations > 0u);
    T64C_CHECK(&tally, ledger->outstanding == ledger->allocations);
    t64_machine_destroy(machine);
  }
  T64C_CHECK(&tally, ledger->releases == ledger->allocations);
  T64C_CHECK(&tally, ledger->outstanding == 0u);
  T64C_CHECK(&tally, ledger->bad_releases == 0u);
  T64C_CHECK(&tally, ledger->refusals == 0u);

  free(ledger);
  return t64c_report("smoke", &tally);
}

int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    (void)puts("smoke");
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    if (strcmp(argv[2], "smoke") == 0) {
      return case_smoke();
    }
    (void)fprintf(stderr, "t64c: unknown case: %s\n", argv[2]);
    return 2;
  }
  (void)fprintf(stderr, "usage: t64c --list-cases | --case NAME\n");
  return 2;
}
