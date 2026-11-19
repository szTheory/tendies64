#include "t64_real_fixture.h"

#include <string.h>

/* The legacy transcript pin: the 30-byte hello text. Rows without their own
 * pin keep being judged by it, exactly as before the per-row pin existed. */
#define LEGACY_LOG_SHA256 \
  "86594505bea53c9843f9eaf07e4cf0121cbe0055670324674fd8385c88ffd18e"

/* Pins are independently reflected in fixtures/manifest.json. Only the
 * original positive hello entry can produce pass. Completion PCs are the
 * authored software-stop locations in the pinned assembly variants. */
static const t64_real_fixture fixtures[] = {
    {"hello-openipl3",
     "fa276cc175a4e764b6fd5413193c44b51f38fd8135e33e400032827e02f27c7c",
     4280u, "ntsc-cart-4m-openipl3-warm-v1",
     "d8c12cb2a00410a2153ca62da318166f3ca029dea8e3d6b9c841262cb8203c71",
     UINT32_C(0x80000478), true, 0u, 0u, NULL},
    {"hello-empty-break",
     "5ae986bb263cafa28e2b06a17e3d3f3ee229c55f75cd1ab91df7a9698e35a652",
     4128u, "ntsc-cart-4m-openipl3-warm-v1",
     "d8c12cb2a00410a2153ca62da318166f3ca029dea8e3d6b9c841262cb8203c71",
     UINT32_C(0x80000400), false, 0u, 0u, NULL},
    {"hello-wrong-text",
     "7626a661296fd8f5a5939ceb3b1fa3321f96e23be0e0bbf6b06b7e57871d86b8",
     4272u, "ntsc-cart-4m-openipl3-warm-v1",
     "d8c12cb2a00410a2153ca62da318166f3ca029dea8e3d6b9c841262cb8203c71",
     UINT32_C(0x80000478), false, 0u, 0u, NULL},
    {"hello-empty-loop",
     "88edd612852e36897a979182db53bf7cb184c31d56128c74e9544ec797f3c815",
     4128u, "ntsc-cart-4m-openipl3-warm-v1",
     "d8c12cb2a00410a2153ca62da318166f3ca029dea8e3d6b9c841262cb8203c71",
     0u, false, 0u, 0u, NULL},
    {"hello-incomplete-loop",
     "ba62967b56a7317d9ca4e4c8ffa3acfce5856b3307f6ca67da1739ba394b1054",
     4272u, "ntsc-cart-4m-openipl3-warm-v1",
     "d8c12cb2a00410a2153ca62da318166f3ca029dea8e3d6b9c841262cb8203c71",
     0u, false, 0u, 0u, NULL},
    {"hello-unsupported",
     "7c89481f15137f24a7c7067a67ccd6929dad6a5f982ec459bf39114d2f9926e9",
     4128u, "ntsc-cart-4m-openipl3-warm-v1",
     "d8c12cb2a00410a2153ca62da318166f3ca029dea8e3d6b9c841262cb8203c71",
     0u, false, 0u, 0u, NULL},
    {"hello-output-overflow",
     "4ae73021dbdc2465944887a356c8f9154b7f1b3109487b82c808e9b3cccde18e",
     4264u, "ntsc-cart-4m-openipl3-warm-v1",
     "d8c12cb2a00410a2153ca62da318166f3ca029dea8e3d6b9c841262cb8203c71",
     0u, false, 0u, 0u, NULL},
    {"rdp-vi-fill-diagnostic",
     "b469340e7caa0ac341a2a5b525aa7e3b39c8a47f295ecd10710ae695e46dfc3d",
     4552u, "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v2",
     "5612696247abb5dae1c2abb6c15d7fd6c5632136210be7b21a1085590af1c94e",
     UINT32_C(0x80000558), true, 0u, 0u, NULL},
    {"audio-pcm-literal",
     "cd9556444480b8b02b043207013e7ce5bf5697a1c040058bcde1348e55e80b99",
     4728u, "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1",
     "eaa400aa23bfa3d0c9ba6d475d8eba7a60302151e22b6278ff6a4991d08bb5bf",
     UINT32_C(0x800005e8), true, 0u, 0u, NULL},
    {"audio-pcm-wrong-sample",
     "046f663adee4cb41dff2925cd8a62a938495798c2d6b7541da528afd5e1eeb46",
     4728u, "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1",
     "eaa400aa23bfa3d0c9ba6d475d8eba7a60302151e22b6278ff6a4991d08bb5bf",
     UINT32_C(0x800005e8), true, 0u, 0u, NULL},
    {"phase09-admit-si-v1",
     "b3a97e238137feece98b4b39e2f76973a531500c35b39470bfcd6cd23ae67460",
     4264u, "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1",
     "661b161dfee9a10857efe3f98d840c09f05cbf0492dfde59e8ec44190dabb6d5",
     UINT32_C(0x80000470), true, 0u, 20u,
     "5fc4d0d36f6eb18aeaa37457e14d3020e14614b97b9e94c6c219f6036624fbc9"},
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

bool t64_real_fixture_profile_allowed(const char *profile_id) {
  size_t i;
  if (profile_id == NULL) return false;
  for (i = 0u; i < sizeof fixtures / sizeof fixtures[0]; i++) {
    if (strcmp(fixtures[i].profile_id, profile_id) == 0) return true;
  }
  return false;
}

uint64_t t64_real_fixture_tick_cap(const t64_real_fixture *fixture) {
  if (fixture == NULL || fixture->guest_tick_cap == 0u) {
    return T64_REAL_GUEST_TICKS_DEFAULT_CAP;
  }
  return fixture->guest_tick_cap;
}

bool t64_real_fixture_log_pin(const t64_real_fixture *fixture, size_t *bytes,
                              const char **sha256_hex) {
  if (fixture != NULL && fixture->expected_log_sha256 != NULL) {
    *bytes = fixture->expected_log_bytes;
    *sha256_hex = fixture->expected_log_sha256;
    return true;
  }
  *bytes = T64_REAL_LEGACY_LOG_BYTES;
  *sha256_hex = LEGACY_LOG_SHA256;
  return false;
}
