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
    {"phase09-admit-e4k",
     "5015baa4127a56dca9322a453bd2880ead9c3beb08f3f3dd83aada26f8272d18",
     4264u, "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e4k",
     "e8dcb8dc80f2d1efabe83ac0fd8b98c8211622750b70ff142e667b0023f63ce1",
     UINT32_C(0x80000470), true, 0u, 20u,
     "bd084623780adfd35591144d77f299f3107a3399fe536230c6c891ed7de4f015"},
    {"phase09-admit-e16k",
     "f0fbe7d56aa06bbd0bc9eff1961b3985bbcf104264bf7203040ea9194b510588",
     4264u, "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e16k",
     "22837f619e9537c3c678fbb484747177cbaee93b0bdbfa80d72ac028fda74f3f",
     UINT32_C(0x80000470), true, 0u, 20u,
     "c8ba071062fca5ba87baf198de9cf41bf387fa286754844410b644907eaa2bba"},
    {"phase09-admit-s32k",
     "73344e3f57c93b715147a0430714c86d46f6f7929c14556f8c0511d7791d2df8",
     4264u, "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-s32k",
     "52c47d606e2a0b4dce7a743327082c505ea6f062648cf37349f6e50ca398e87a",
     UINT32_C(0x80000470), true, 0u, 20u,
     "6a0845a8f00f0078e2dc05dddb9daf4ff66430dd2c74010054759107ab25130f"},
    {"phase09-admit-wrong-si-v1",
     "1220f8a1230ac83b28601028e8b1ecca84bbc89e8d78c3284a8af8fe51f6f29a",
     4264u, "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1",
     "661b161dfee9a10857efe3f98d840c09f05cbf0492dfde59e8ec44190dabb6d5",
     UINT32_C(0x80000470), true, 0u, 20u,
     "5fc4d0d36f6eb18aeaa37457e14d3020e14614b97b9e94c6c219f6036624fbc9"},
    {"phase09-admit-long-si-v1",
     "6e803800afe03de0604ceb541deabee9a1db1a3a7d6ea397d90d8537c3f77e33",
     4280u, "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1",
     "661b161dfee9a10857efe3f98d840c09f05cbf0492dfde59e8ec44190dabb6d5",
     UINT32_C(0x80000484), true, T64_REAL_GUEST_TICKS_LONG_CAP, 20u,
     "5fc4d0d36f6eb18aeaa37457e14d3020e14614b97b9e94c6c219f6036624fbc9"},
    {"phase09-pad-poll-si-v1",
     "29a005be19b2f86e866d00cfb3ab2f8aac39e376d85a5e5d24e78c113a80dab6",
     4688u, "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1",
     "661b161dfee9a10857efe3f98d840c09f05cbf0492dfde59e8ec44190dabb6d5",
     UINT32_C(0x800005a8), true, T64_REAL_GUEST_TICKS_LONG_CAP, 68u,
     "715aa3854b455dc9fd8e038e1aa27e5d200ea311acbfd032930cdc760498d3f6"},
    {"phase09-pad-poll-wrong-si-v1",
     "84bacc101384d52b109a4c41e4b4fac340143a89ad79a4fc4460c5890923a8f6",
     4688u, "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1",
     "661b161dfee9a10857efe3f98d840c09f05cbf0492dfde59e8ec44190dabb6d5",
     UINT32_C(0x800005ac), true, T64_REAL_GUEST_TICKS_LONG_CAP, 68u,
     "715aa3854b455dc9fd8e038e1aa27e5d200ea311acbfd032930cdc760498d3f6"},
    {"phase09-eeprom-rw-e4k-v1",
     "0f3cf3c72a0312d90c915f5d148817f9d5f201b96f1575abbc96a81e389be0e9",
     5584u, "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e4k",
     "e8dcb8dc80f2d1efabe83ac0fd8b98c8211622750b70ff142e667b0023f63ce1",
     UINT32_C(0x80000688), true, T64_REAL_GUEST_TICKS_LONG_CAP, 191u,
     "03891120e98176bc11f8d9f6cde6278ff33ef0ebe9018d2f24ce083b3d2c6502"},
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

size_t t64_real_fixture_count(void) {
  return sizeof fixtures / sizeof fixtures[0];
}

const t64_real_fixture *t64_real_fixture_at(size_t index) {
  return index < sizeof fixtures / sizeof fixtures[0] ? &fixtures[index]
                                                      : NULL;
}
