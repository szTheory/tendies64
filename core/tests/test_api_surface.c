/*
 * Public-surface tests (Phase 11). This file includes only the public header
 * and the shared test header, so it sees exactly what an installed consumer
 * sees. Plan 11-02 adds the ABI version case, plan 12-16 the create-time major
 * rule at the bound ABI 1.0; later shape plans add cases here.
 */
#include <stdio.h>
#include <string.h>

#include "api_test_support.h"
#include "t64_test.h"
#include "tendies64/t64_machine.h"

/* t64_abi_version() agrees with the header constants: the upper 16 bits are
 * the major, the lower 16 the minor, and the build identity names its format. */
static void case_abi_version(void) {
  uint32_t version = t64_abi_version();
  const char *identity = t64_build_identity();

  T64_CHECK_INT(version, T64_ABI_VERSION);
  T64_CHECK_INT(version >> 16, T64_ABI_VERSION_MAJOR);
  T64_CHECK_INT(version & UINT32_C(0xffff), T64_ABI_VERSION_MINOR);
  T64_CHECK_INT(T64_ABI_VERSION_MAJOR, 1);
  T64_CHECK_INT(T64_ABI_VERSION_MINOR, 0);
  T64_CHECK(identity != NULL);
  T64_CHECK(identity != NULL && strncmp(identity, "t64-build-v1-", 13u) == 0);
}

/* A caller's abi_version built the way the header documents it: the major in
 * the upper 16 bits, the minor in the lower 16. */
static uint32_t version_of(uint32_t major, uint32_t minor) {
  return (major << UINT32_C(16)) | minor;
}

/* The create-time rule at the bound major (ABI 1.0, docs/spec/api.md 2.2, the
 * header's create check order): a caller whose major equals the library's and
 * whose minor is at most the library's is accepted; a caller minor above
 * T64_ABI_VERSION_MINOR, major 0 and any other major answer
 * T64_STATUS_ABI_MISMATCH with *machine_out NULL and the allocator never
 * called. unit.core.api.handshake pins the neighbouring refusals by arithmetic
 * on T64_ABI_VERSION; this case states the rule itself, from the documented
 * (major << 16) | minor form, at the bound major 1.
 *
 * NOT EXERCISED at library minor 0: "a caller minor below the library minor
 * is accepted" (the branch caller_minor <= T64_ABI_VERSION_MINOR with a strict
 * inequality) because no minor below 0 exists. The block below runs it at the
 * first 1.x minor; until then the case says so on stdout. */
static void case_abi_major_rule(void) {
  static const struct {
    uint32_t major;
    uint32_t minor;
    const char *why;
  } refused[] = {
      {T64_ABI_VERSION_MAJOR, T64_ABI_VERSION_MINOR + 1u,
       "the library's major with a minor above the library's"},
      {T64_ABI_VERSION_MAJOR, UINT32_C(0xffff),
       "the library's major with the largest minor"},
      {T64_ABI_VERSION_MAJOR + 1u, 0u, "the next major"},
      {T64_ABI_VERSION_MAJOR + 1u, 1u, "the next major with a minor"},
      {UINT32_C(0xffff), 0u, "the largest major"},
      {0u, 0u, "major 0 and minor 0 (a zeroed abi_version)"},
      {0u, 1u, "major 0, the 0.1 freeze-candidate version"},
  };
  counting_allocator counts = {0};
  t64_host_allocator allocator =
      T64_HOST_ALLOCATOR_INIT(counting_allocate, counting_release, &counts);
  const t64_profile_descriptor *registered = NULL;
  t64_machine_create_info info = T64_MACHINE_CREATE_INFO_INIT(NULL, NULL);
  t64_machine *machine = NULL;
  size_t index;

  T64_CHECK_INT(T64_ABI_VERSION, version_of(1u, 0u));
  T64_CHECK_INT(t64_profile_lookup("ntsc-cart-4m-skeleton", &registered),
                T64_STATUS_OK);
  if (registered == NULL) return;
  info = (t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(&allocator,
                                                               registered);

  /* Every refusal leaves *machine_out NULL (it starts as a non-NULL sentinel)
   * and calls the allocator never. */
  for (index = 0u; index < sizeof refused / sizeof refused[0]; index++) {
    static uint8_t not_a_machine;
    size_t before = counts.allocations;
    t64_status status;

    machine = (t64_machine *)(void *)&not_a_machine;
    info.abi_version = version_of(refused[index].major, refused[index].minor);
    status = t64_machine_create(&info, &machine);
    if (status != T64_STATUS_ABI_MISMATCH || machine != NULL ||
        counts.allocations != before) {
      fprintf(stderr, "abi-major-rule: %u.%u (%s): status %d, machine %s, "
              "allocations %zu -> %zu\n", (unsigned)refused[index].major,
              (unsigned)refused[index].minor, refused[index].why, (int)status,
              machine == NULL ? "NULL" : "not NULL", before, counts.allocations);
    }
    T64_CHECK_INT(status, T64_STATUS_ABI_MISMATCH);
    T64_CHECK(machine == NULL);
    T64_CHECK_INT(counts.allocations, before);
    T64_CHECK_INT(counts.releases, 0);
    T64_CHECK_INT(counts.live_count, 0);
  }

  /* The library's own version is accepted and the instance releases every
   * allocation it made. */
  info.abi_version = version_of(T64_ABI_VERSION_MAJOR, T64_ABI_VERSION_MINOR);
  T64_CHECK_INT(info.abi_version, T64_ABI_VERSION);
  machine = NULL;
  T64_CHECK_INT(t64_machine_create(&info, &machine), T64_STATUS_OK);
  T64_CHECK(machine != NULL);
  T64_CHECK(counts.allocations > 0u);
  t64_machine_destroy(machine);
  T64_CHECK_INT(counts.releases, counts.allocations);
  T64_CHECK_INT(counts.live_count, 0);
  T64_CHECK_INT(counts.unknown_releases, 0);

  /* The older-minor half of the rule. */
  if (T64_ABI_VERSION_MINOR > 0u) {
    info.abi_version =
        version_of(T64_ABI_VERSION_MAJOR, T64_ABI_VERSION_MINOR - 1u);
    machine = NULL;
    T64_CHECK_INT(t64_machine_create(&info, &machine), T64_STATUS_OK);
    T64_CHECK(machine != NULL);
    t64_machine_destroy(machine);
    T64_CHECK_INT(counts.releases, counts.allocations);
  } else {
    printf("abi-major-rule: older-minor acceptance NOT EXERCISED: the library "
           "minor is 0 and no lower minor exists (first 1.x minor)\n");
  }
}

int main(int argc, char **argv) {
  static const char *const cases[] = {"abi-version", "abi-major-rule"};

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    size_t index;

    for (index = 0u; index < sizeof cases / sizeof cases[0]; index++) {
      printf("%s\n", cases[index]);
    }
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    if (strcmp(argv[2], "abi-version") == 0) {
      case_abi_version();
      return T64_TEST_FINISH("unit.core.api.abi-version");
    }
    if (strcmp(argv[2], "abi-major-rule") == 0) {
      case_abi_major_rule();
      return T64_TEST_FINISH("unit.core.api.abi-major-rule");
    }
    fprintf(stderr, "unknown case: %s\n", argv[2]);
    return 2;
  }
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--list-cases|--case NAME]\n", argv[0]);
    return 2;
  }
  case_abi_version();
  case_abi_major_rule();
  return T64_TEST_FINISH("unit.core.api");
}
