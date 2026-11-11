#include "t64_real_fixture.h"

#include <string.h>

/* Pins are independently reflected in fixtures/manifest.json. Only the
 * original positive hello entry can produce pass. Completion PCs are the
 * authored software-stop locations in the pinned assembly variants. */
static const t64_real_fixture fixtures[] = {
    {"hello-openipl3",
     "fa276cc175a4e764b6fd5413193c44b51f38fd8135e33e400032827e02f27c7c",
     4280u, UINT32_C(0x80000478), true},
    {"hello-empty-break",
     "5ae986bb263cafa28e2b06a17e3d3f3ee229c55f75cd1ab91df7a9698e35a652",
     4128u, UINT32_C(0x80000400), false},
    {"hello-wrong-text",
     "7626a661296fd8f5a5939ceb3b1fa3321f96e23be0e0bbf6b06b7e57871d86b8",
     4272u, UINT32_C(0x80000478), false},
    {"hello-empty-loop",
     "88edd612852e36897a979182db53bf7cb184c31d56128c74e9544ec797f3c815",
     4128u, 0u, false},
    {"hello-incomplete-loop",
     "ba62967b56a7317d9ca4e4c8ffa3acfce5856b3307f6ca67da1739ba394b1054",
     4272u, 0u, false},
    {"hello-unsupported",
     "7c89481f15137f24a7c7067a67ccd6929dad6a5f982ec459bf39114d2f9926e9",
     4128u, 0u, false},
    {"hello-output-overflow",
     "4ae73021dbdc2465944887a356c8f9154b7f1b3109487b82c808e9b3cccde18e",
     4264u, 0u, false},
};

const t64_real_fixture *t64_real_fixture_find_bytes(
    size_t size, const char *sha256) {
  size_t i;
  if (sha256 == NULL || strlen(sha256) != T64_REAL_FIXTURE_SHA256_SIZE - 1u) {
    return NULL;
  }
  for (i = 0u; i < sizeof fixtures / sizeof fixtures[0]; i++) {
    if (fixtures[i].size == size && strcmp(fixtures[i].sha256, sha256) == 0) {
      return &fixtures[i];
    }
  }
  return NULL;
}

const t64_real_fixture *t64_real_fixture_find_identity(
    const char *id, const char *sha256) {
  size_t i;
  if (id == NULL || sha256 == NULL) return NULL;
  for (i = 0u; i < sizeof fixtures / sizeof fixtures[0]; i++) {
    if (strcmp(fixtures[i].id, id) == 0 &&
        strcmp(fixtures[i].sha256, sha256) == 0) {
      return &fixtures[i];
    }
  }
  return NULL;
}
