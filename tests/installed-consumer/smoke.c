/*
 * The tracer case of the installed consumer (Phase 11, plans 11-16 and 11-19,
 * VER-05): the linked archive reports the ABI version of the header it was built
 * with and the build identity the package recorded, and a create and destroy
 * round trip gives every allocation back. It lives in its own file so that the
 * small executable t64c_smoke (smoke_main.c) can be built alone: the negative
 * controls of controls.tsv build only that target against a doctored or relocated
 * copy of the prefix, and print and judge the create status it reports.
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

#ifndef T64C_EXPECTED_BUILD_ID
#error "T64C_EXPECTED_BUILD_ID is defined by the consumer CMakeLists from tendies64_BUILD_ID"
#endif

/* The tracer: the linked archive reports the ABI version of the header it was
 * built with and the build identity the package recorded, and a create and
 * destroy round trip gives every allocation back. */
int t64c_case_smoke(void) {
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
    /* The negative controls of controls.tsv read this line: a doctored header
     * must show as a refused create with no machine. */
    (void)printf("smoke: create status=%u machine=%s\n", (unsigned)status,
                 machine == NULL ? "NULL" : "set");
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
