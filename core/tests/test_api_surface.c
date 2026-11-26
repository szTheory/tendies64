/*
 * Public-surface tests (Phase 11). This file includes only the public header
 * and the shared test header, so it sees exactly what an installed consumer
 * sees. Plan 11-02 adds the ABI version case; later shape plans add cases here.
 */
#include <stdio.h>
#include <string.h>

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

int main(int argc, char **argv) {
  static const char *const cases[] = {"abi-version"};

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
    fprintf(stderr, "unknown case: %s\n", argv[2]);
    return 2;
  }
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--list-cases|--case NAME]\n", argv[0]);
    return 2;
  }
  case_abi_version();
  return T64_TEST_FINISH("unit.core.api");
}
