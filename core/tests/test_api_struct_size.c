/*
 * Size handshake of the public API (plan 11-12; plan 11-13 adds the
 * struct_size cases of the other entries). This file includes only the public
 * header and the shared test header, so it sees exactly what an installed
 * consumer sees.
 *
 *   handshake            the create-info checks of DR-05: NULL arguments, a
 *                        short create-info, a skewed ABI version and a short
 *                        allocator or profile, each refused with no
 *                        allocation and *machine_out NULL
 *   profile-hash-stable  every registered profile hashes to the value of the
 *                        Wave 0 base (struct_size is not hashed or serialized)
 *   trace-record-size    the record_size parameter of t64_machine_trace_drain:
 *                        below the v1 record refused with every output
 *                        untouched, above it used as the stride with the
 *                        trailing bytes of each record untouched
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_test.h"
#include "tendies64/t64_machine.h"

#define AUTHORED_ROM_BYTES 4096u
#define TRACE_SLOTS 8u
#define TRACE_PAD 8u

typedef struct {
  unsigned allocations;
  unsigned releases;
} counting_context;

static bool counting_allocate(void *context, size_t size, size_t alignment,
                              t64_host_storage *storage_out) {
  counting_context *counts = (counting_context *)context;
  void *memory;

  if (counts == NULL || storage_out == NULL || size == 0u || alignment == 0u ||
      (alignment & (alignment - 1u)) != 0u) {
    return false;
  }
  if (alignment <= _Alignof(max_align_t)) {
    memory = malloc(size);
  } else {
    memory = aligned_alloc(alignment,
                           (size + alignment - 1u) / alignment * alignment);
  }
  if (memory == NULL || (uintptr_t)memory % alignment != 0u) {
    free(memory);
    return false;
  }
  counts->allocations++;
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = alignment;
  return true;
}

static void counting_release(void *context, t64_host_storage storage) {
  counting_context *counts = (counting_context *)context;

  if (counts != NULL) counts->releases++;
  free(storage.data);
}

/* A machine pointer the refused create must overwrite with NULL. */
static t64_machine *sentinel_machine(void) {
  static uint8_t not_a_machine;

  return (t64_machine *)(void *)&not_a_machine;
}

/* create with info must be refused with want, write NULL to *machine_out and
 * call the allocator never. */
static void check_refused(const t64_machine_create_info *info,
                          counting_context *counts, t64_status want) {
  t64_machine *machine = sentinel_machine();
  unsigned before = counts->allocations;

  T64_CHECK_INT(t64_machine_create(info, &machine), want);
  T64_CHECK(machine == NULL);
  T64_CHECK_INT(counts->allocations, before);
  T64_CHECK_INT(counts->releases, 0);
}

static void case_handshake(void) {
  counting_context counts = {0u, 0u};
  t64_host_allocator allocator = T64_HOST_ALLOCATOR_INIT(
      counting_allocate, counting_release, &counts);
  const t64_profile_descriptor *registered = NULL;
  t64_profile_descriptor profile;
  t64_host_allocator short_allocator = T64_HOST_ALLOCATOR_INIT(
      counting_allocate, counting_release, &counts);
  t64_profile_descriptor short_profile;
  t64_machine_create_info info = T64_MACHINE_CREATE_INFO_INIT(NULL, NULL);
  t64_machine *machine = NULL;
  struct {
    t64_machine_create_info info;
    uint64_t extension;
  } larger;

  T64_CHECK_INT(t64_abi_version(), T64_ABI_VERSION);
  T64_CHECK_INT(t64_profile_lookup("ntsc-cart-4m-skeleton", &registered),
                T64_STATUS_OK);
  if (registered == NULL) return;
  profile = *registered;
  T64_CHECK_INT(profile.struct_size, sizeof(t64_profile_descriptor));
  T64_CHECK_INT(allocator.struct_size, sizeof(t64_host_allocator));

  /* NULL arguments. */
  T64_CHECK_INT(t64_machine_create(NULL, NULL), T64_STATUS_INVALID_ARGUMENT);
  info = (t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(&allocator,
                                                               &profile);
  T64_CHECK_INT(info.struct_size, sizeof(t64_machine_create_info));
  T64_CHECK_INT(info.abi_version, T64_ABI_VERSION);
  T64_CHECK_INT(t64_machine_create(&info, NULL), T64_STATUS_INVALID_ARGUMENT);
  check_refused(NULL, &counts, T64_STATUS_INVALID_ARGUMENT);

  /* A create-info below its v1 size. */
  info.struct_size = 0u;
  check_refused(&info, &counts, T64_STATUS_STRUCT_TOO_SMALL);
  info.struct_size = (uint32_t)sizeof(t64_machine_create_info) - 1u;
  check_refused(&info, &counts, T64_STATUS_STRUCT_TOO_SMALL);
  info.struct_size = (uint32_t)sizeof(t64_machine_create_info);

  /* A skewed ABI version: while the major is 0 the minor must match. */
  info.abi_version = T64_ABI_VERSION + 1u;
  check_refused(&info, &counts, T64_STATUS_ABI_MISMATCH);
  info.abi_version = T64_ABI_VERSION - 1u;
  check_refused(&info, &counts, T64_STATUS_ABI_MISMATCH);
  info.abi_version = T64_ABI_VERSION + (UINT32_C(1) << 16);
  check_refused(&info, &counts, T64_STATUS_ABI_MISMATCH);
  info.abi_version = 0u;
  check_refused(&info, &counts, T64_STATUS_ABI_MISMATCH);
  info.abi_version = T64_ABI_VERSION;

  /* The ABI check precedes the allocator and profile checks. */
  info.allocator = NULL;
  info.abi_version = T64_ABI_VERSION + 1u;
  check_refused(&info, &counts, T64_STATUS_ABI_MISMATCH);
  info.abi_version = T64_ABI_VERSION;
  check_refused(&info, &counts, T64_STATUS_INVALID_ARGUMENT);
  info.allocator = &allocator;
  info.profile = NULL;
  check_refused(&info, &counts, T64_STATUS_INVALID_ARGUMENT);

  /* An allocator or a profile below its v1 size. */
  short_allocator = allocator;
  short_allocator.struct_size = (uint32_t)sizeof(t64_host_allocator) - 1u;
  info = (t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(
      &short_allocator, &profile);
  check_refused(&info, &counts, T64_STATUS_STRUCT_TOO_SMALL);
  short_allocator.struct_size = 0u;
  check_refused(&info, &counts, T64_STATUS_STRUCT_TOO_SMALL);
  short_profile = profile;
  short_profile.struct_size = (uint32_t)sizeof(t64_profile_descriptor) - 1u;
  info = (t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(&allocator,
                                                               &short_profile);
  check_refused(&info, &counts, T64_STATUS_STRUCT_TOO_SMALL);
  short_profile.struct_size = 0u;
  check_refused(&info, &counts, T64_STATUS_STRUCT_TOO_SMALL);

  /* The well-formed create-info, and a larger caller create-info. */
  info = (t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(&allocator,
                                                               &profile);
  T64_CHECK_INT(t64_machine_create(&info, &machine), T64_STATUS_OK);
  T64_CHECK(machine != NULL);
  T64_CHECK(counts.allocations > 0u);
  t64_machine_destroy(machine);
  T64_CHECK_INT(counts.releases, counts.allocations);
  memset(&larger, 0xa5, sizeof larger);
  larger.info = (t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(
      &allocator, &profile);
  larger.info.struct_size = (uint32_t)sizeof larger;
  machine = NULL;
  T64_CHECK_INT(t64_machine_create(&larger.info, &machine), T64_STATUS_OK);
  T64_CHECK(machine != NULL);
  t64_machine_destroy(machine);
  T64_CHECK_INT(counts.releases, counts.allocations);
}

/* Profile hashes at the Wave 0 base (f9124ae9fce1), before struct_size. */
static const struct {
  const char *id;
  const char *hash;
} wave0_hashes[] = {
    {"ntsc-cart-4m-skeleton",
     "b7d1600ad960b4e04fdc3ec372675edc47b26b15be8e9cd84cbab4fd75835647"},
    {"ntsc-cart-4m-openipl3-warm-v1",
     "d8c12cb2a00410a2153ca62da318166f3ca029dea8e3d6b9c841262cb8203c71"},
    {"ntsc-cart-4m-openipl3-vr4300-v1",
     "f37861d8911ad9d3c0e54b070514f79dffb9ecb3a52f3f345e3aaf0dfc0083c4"},
    {"ntsc-cart-4m-openipl3-vr4300-compat-v1",
     "5955f3556c0653602c7c2da9faaee49dd2a647f0692d46f823ccb152011fa8d7"},
    {"ntsc-cart-4m-openipl3-vr4300-compat-v2",
     "1086516cd466bf48133533a8edf33a12cc8fd9e27dcd2b493e92b2bcf21a7634"},
    {"ntsc-cart-4m-openipl3-rsp-functional-v1",
     "1ece9425c7cd13a7e183e6d42ed3be12ec28f33977c46713ea612aa38e1e22ec"},
    {"ntsc-cart-4m-openipl3-rdram-a-t1-v1",
     "d59b5e9500acb56621f1995e2c816f73597dce9109b46a4008b12269e42c02a2"},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-v1",
     "4d807406f1e8ab897a8328cf9f205e8d8faf77dbf8892cb999156265403023fa"},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v2",
     "5612696247abb5dae1c2abb6c15d7fd6c5632136210be7b21a1085590af1c94e"},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-triangle-v3",
     "794710f767fce5f1bd37d1116a6067bac10026aa4c7d77fc9cfe7c3636d5830f"},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-texture-v4",
     "3ce5b6c75ba85a8b1d5ed333820d70736c26c003ec16c76a7cd9f8ee686f3e18"},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-combiner-v5",
     "0e9938d6b92159fe1f8c5dff0c03749979d5d10b11b458224f2dcca1a2345efa"},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-blender-v6",
     "7f73b46e6c695fddb60d89d6351c1c099f56e76dbe109ab9f2cb9f372fc56d0e"},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-depth-v7",
     "56f25e100d3c92792f836b0664e81697919487b4fdae49404476a85135ae5969"},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v8",
     "4f929a757bc6c257729918ccd92e632256fc5ad991dab2317a272cba1d22dc1d"},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9",
     "ebc4370b89cf52d0b2125bac2ab6b40b0fc9f5bef44c8bf9c28647c073514015"},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1",
     "eaa400aa23bfa3d0c9ba6d475d8eba7a60302151e22b6278ff6a4991d08bb5bf"},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-si-v1",
     "661b161dfee9a10857efe3f98d840c09f05cbf0492dfde59e8ec44190dabb6d5"},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e4k",
     "e8dcb8dc80f2d1efabe83ac0fd8b98c8211622750b70ff142e667b0023f63ce1"},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e16k",
     "22837f619e9537c3c678fbb484747177cbaee93b0bdbfa80d72ac028fda74f3f"},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-s32k",
     "52c47d606e2a0b4dce7a743327082c505ea6f062648cf37349f6e50ca398e87a"},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-si-v2",
     "3bc028bdb4c6f9622e20ecfff828a26245adc04ea69ddeeefdbb08d5b27dbc47"},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-e4k",
     "0a4bfc455adc8b66c4666087c71ff5c0ce3b2f46ab6c83784a37d862956c1fe8"},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-e16k",
     "34f767b0a9495d604719288515951c2e2ae726ef661f6a7c5fc378397718536c"},
    {"ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k",
     "9dfb90d8055b822a1767be1c5594d1b2d904f042ef6fe82f2e7a3243e5756578"},
};

static void to_hex(const uint8_t digest[T64_DIGEST_SIZE],
                   char hex[2u * T64_DIGEST_SIZE + 1u]) {
  static const char digits[] = "0123456789abcdef";
  size_t i;

  for (i = 0u; i < T64_DIGEST_SIZE; i++) {
    hex[2u * i] = digits[digest[i] >> 4];
    hex[2u * i + 1u] = digits[digest[i] & 0x0fu];
  }
  hex[2u * T64_DIGEST_SIZE] = '\0';
}

static void case_profile_hash_stable(void) {
  size_t index;

  for (index = 0u; index < sizeof wave0_hashes / sizeof wave0_hashes[0];
       index++) {
    const t64_profile_descriptor *profile = NULL;
    t64_profile_descriptor copy;
    uint8_t digest[T64_DIGEST_SIZE];
    uint8_t copy_digest[T64_DIGEST_SIZE];
    uint8_t bytes[T64_PROFILE_SERIALIZED_MAX_BYTES];
    uint8_t copy_bytes[T64_PROFILE_SERIALIZED_MAX_BYTES];
    size_t length = 0u;
    size_t copy_length = 0u;
    char hex[2u * T64_DIGEST_SIZE + 1u];

    T64_CHECK_INT(t64_profile_lookup(wave0_hashes[index].id, &profile),
                  T64_STATUS_OK);
    if (profile == NULL) continue;
    T64_CHECK_INT(profile->struct_size, sizeof(t64_profile_descriptor));
    T64_CHECK_INT(t64_profile_hash(profile, digest), T64_STATUS_OK);
    to_hex(digest, hex);
    T64_CHECK_STR(hex, wave0_hashes[index].hash);
    /* struct_size is outside the hash and the serialized bytes. */
    copy = *profile;
    copy.struct_size = UINT32_MAX;
    T64_CHECK_INT(t64_profile_hash(&copy, copy_digest), T64_STATUS_OK);
    T64_CHECK(memcmp(digest, copy_digest, sizeof digest) == 0);
    T64_CHECK_INT(
        t64_profile_serialize(profile, bytes, sizeof bytes, &length),
        T64_STATUS_OK);
    T64_CHECK_INT(t64_profile_serialize(&copy, copy_bytes, sizeof copy_bytes,
                                        &copy_length),
                  T64_STATUS_OK);
    T64_CHECK_INT(copy_length, length);
    T64_CHECK(length <= sizeof bytes &&
              memcmp(bytes, copy_bytes, length) == 0);
  }
}

/* Authored 4 KiB image the skeleton profile accepts (the pattern of
 * core/tests/test_digest_v25.c). */
static void fill_authored_rom(uint8_t image[AUTHORED_ROM_BYTES]) {
  size_t i;

  for (i = 0u; i < AUTHORED_ROM_BYTES; i++) {
    image[i] = (uint8_t)((i * 13u + 7u) & 0xffu);
  }
  image[0] = 0x80u;
  image[1] = 0x37u;
  image[2] = 0x12u;
  image[3] = 0x40u;
  image[0x3eu] = (uint8_t)'E';
}

/* A skeleton machine with tracing on, the admission record and two RDRAM
 * stores through the debug bus, so the trace holds at least two records. */
static t64_machine *traced_machine(const t64_host_allocator *allocator) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t image[AUTHORED_ROM_BYTES];

  T64_CHECK_INT(t64_profile_lookup("ntsc-cart-4m-skeleton", &profile),
                T64_STATUS_OK);
  if (profile == NULL) return NULL;
  T64_CHECK_INT(t64_machine_create(
                    &(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(
                        allocator, profile),
                    &machine),
                T64_STATUS_OK);
  if (machine == NULL) return NULL;
  T64_CHECK_INT(t64_machine_trace_set_enabled(machine, true), T64_STATUS_OK);
  fill_authored_rom(image);
  T64_CHECK_INT(t64_machine_admit_rom(machine, image, sizeof image),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, 0x0u, 4u, 0x01020304u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, 0x4u, 4u, 0x05060708u),
                T64_STATUS_OK);
  return machine;
}

static void case_trace_record_size(void) {
  counting_context counts = {0u, 0u};
  t64_host_allocator allocator = T64_HOST_ALLOCATOR_INIT(
      counting_allocate, counting_release, &counts);
  t64_trace_record reference[TRACE_SLOTS];
  size_t reference_count = 0u;
  uint64_t reference_dropped = 0u;
  bool reference_saturated = true;
  static const uint32_t stride = (uint32_t)sizeof(t64_trace_record) +
                                 TRACE_PAD;
  uint8_t buffer[TRACE_SLOTS * (sizeof(t64_trace_record) + TRACE_PAD)];
  uint8_t pristine[sizeof buffer];
  static const uint32_t short_sizes[] = {
      0u, 1u, (uint32_t)sizeof(t64_trace_record) - 1u};
  t64_machine *machine;
  t64_machine *twin;
  size_t count;
  uint64_t dropped;
  bool saturated;
  size_t index;

  twin = traced_machine(&allocator);
  machine = traced_machine(&allocator);
  if (twin == NULL || machine == NULL) {
    t64_machine_destroy(twin);
    t64_machine_destroy(machine);
    T64_CHECK(false);
    return;
  }
  T64_CHECK_INT(t64_machine_trace_drain(twin, reference,
                                        (uint32_t)sizeof(t64_trace_record),
                                        TRACE_SLOTS, &reference_count,
                                        &reference_dropped,
                                        &reference_saturated),
                T64_STATUS_OK);
  T64_CHECK(reference_count >= 2u);
  T64_CHECK_INT(reference_dropped, 0u);
  T64_CHECK(!reference_saturated);

  /* Below the v1 record: refused, sentinel outputs and the queue untouched. */
  memset(pristine, 0xa5, sizeof pristine);
  for (index = 0u; index < sizeof short_sizes / sizeof short_sizes[0];
       index++) {
    memcpy(buffer, pristine, sizeof buffer);
    count = SIZE_MAX;
    dropped = UINT64_MAX;
    saturated = true;
    T64_CHECK_INT(t64_machine_trace_drain(machine,
                                          (t64_trace_record *)(void *)buffer,
                                          short_sizes[index], TRACE_SLOTS,
                                          &count, &dropped, &saturated),
                  T64_STATUS_STRUCT_TOO_SMALL);
    T64_CHECK(count == SIZE_MAX);
    T64_CHECK(dropped == UINT64_MAX);
    T64_CHECK(saturated);
    T64_CHECK(memcmp(buffer, pristine, sizeof buffer) == 0);
  }

  /* A larger caller record: written at that stride, the first v1 bytes of
   * each record are the record, the trailing bytes stay as they were. */
  memcpy(buffer, pristine, sizeof buffer);
  count = SIZE_MAX;
  dropped = UINT64_MAX;
  saturated = true;
  T64_CHECK_INT(t64_machine_trace_drain(machine,
                                        (t64_trace_record *)(void *)buffer,
                                        stride, TRACE_SLOTS, &count, &dropped,
                                        &saturated),
                T64_STATUS_OK);
  T64_CHECK_INT(count, reference_count);
  T64_CHECK_INT(dropped, reference_dropped);
  T64_CHECK(!saturated);
  for (index = 0u; index < TRACE_SLOTS; index++) {
    const uint8_t *slot = buffer + index * stride;

    if (index < count) {
      t64_trace_record got;

      memcpy(&got, slot, sizeof got);
      T64_CHECK(got.tick == reference[index].tick);
      T64_CHECK_INT(got.device, reference[index].device);
      T64_CHECK_INT(got.kind, reference[index].kind);
      T64_CHECK(got.value == reference[index].value);
      T64_CHECK(memcmp(slot + sizeof(t64_trace_record),
                       pristine + sizeof(t64_trace_record), TRACE_PAD) == 0);
    } else {
      T64_CHECK(memcmp(slot, pristine, stride) == 0);
    }
  }
  /* The drain consumed the records: the queue is now empty. */
  T64_CHECK_INT(t64_machine_trace_drain(machine,
                                        (t64_trace_record *)(void *)buffer,
                                        stride, TRACE_SLOTS, &count, &dropped,
                                        &saturated),
                T64_STATUS_OK);
  T64_CHECK_INT(count, 0u);
  t64_machine_destroy(machine);
  t64_machine_destroy(twin);
  T64_CHECK_INT(counts.releases, counts.allocations);
}

int main(int argc, char **argv) {
  static const char *const cases[] = {"handshake", "profile-hash-stable",
                                      "trace-record-size"};

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    size_t index;

    for (index = 0u; index < sizeof cases / sizeof cases[0]; index++) {
      printf("%s\n", cases[index]);
    }
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    if (strcmp(argv[2], "handshake") == 0) {
      case_handshake();
    } else if (strcmp(argv[2], "profile-hash-stable") == 0) {
      case_profile_hash_stable();
    } else if (strcmp(argv[2], "trace-record-size") == 0) {
      case_trace_record_size();
    } else {
      fprintf(stderr, "unknown case: %s\n", argv[2]);
      return 2;
    }
    return T64_TEST_FINISH(argv[2]);
  }
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--list-cases|--case NAME]\n", argv[0]);
    return 2;
  }
  case_handshake();
  case_profile_hash_stable();
  case_trace_record_size();
  return T64_TEST_FINISH("unit.core.api.handshake");
}
