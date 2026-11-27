/*
 * Size handshake of the public API (plan 11-12) and the struct_size contract
 * of every in/out entry (plan 11-13). This file includes only the public
 * header and the shared test header, so it sees exactly what an installed
 * consumer sees.
 *
 * Group "handshake" (CTest unit.core.api.handshake, the default group):
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
 *
 * Group "struct-size" (CTest unit.core.api.struct-size): per in/out entry, a
 * floor case (a struct_size below the v1 size answers
 * T64_STATUS_STRUCT_TOO_SMALL with the sentinel struct and the machine digest
 * unchanged) and a larger-caller case (a struct_size above the v1 size gets
 * exactly the v1 members, never the struct_size member and never a trailing
 * byte). The cases are named after their entry.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api_test_support.h"
#include "t64_test.h"
#include "tendies64/t64_machine.h"

#define AUTHORED_ROM_BYTES 4096u
#define TRACE_SLOTS 8u
#define TRACE_PAD 8u
#define MAX_FIXTURE_BYTES (64u * 1024u)
#define HELLO_PROFILE "ntsc-cart-4m-openipl3-warm-v1"
#define HELLO_TRANSCRIPT "Tendies64 hello via open IPL3\n"
#define HELLO_BYTES 30u
#define HELLO_BOUND_TICK UINT64_C(700000)
#define AV_PROFILE "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1"
#define SAVE_PROFILE "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1-e4k"
#define IPL3_ASSET_BYTES 4096u
#define TAIL_BYTES 16u
#define SENTINEL_BYTE 0xa5u

/* A machine pointer the refused create must overwrite with NULL. */
static t64_machine *sentinel_machine(void) {
  static uint8_t not_a_machine;

  return (t64_machine *)(void *)&not_a_machine;
}

/* create with info must be refused with want, write NULL to *machine_out and
 * call the allocator never. */
static void check_refused(const t64_machine_create_info *info,
                          counting_allocator *counts, t64_status want) {
  t64_machine *machine = sentinel_machine();
  size_t before = counts->allocations;

  T64_CHECK_INT(t64_machine_create(info, &machine), want);
  T64_CHECK(machine == NULL);
  T64_CHECK_INT(counts->allocations, before);
  T64_CHECK_INT(counts->releases, 0);
}

static void case_handshake(void) {
  counting_allocator counts = {0};
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

  /* A skewed ABI version at the bound major 1: a minor above the library's,
   * major 0 (T64_ABI_VERSION - 1u is 0.65535) and another major are each
   * ABI_MISMATCH. unit.core.api.abi-major-rule states the rule itself. */
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
  counting_allocator counts = {0};
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

/* ---- struct_size group (plan 11-13) ---------------------------------------
 *
 * A sentinel is a byte fill of 0xa5 over the type and TAIL_BYTES more, kept in
 * a wrapper whose struct_size member is then set by name: no member of the type
 * can pass by accident, and a write onto struct_size or past the v1 size shows.
 */
#define SENTINEL_U32 UINT32_C(0xa5a5a5a5)
#define SENTINEL_U64 UINT64_C(0xa5a5a5a5a5a5a5a5)
#define ADVANCE_TICK UINT64_C(20000)

static uint8_t hello_rom[MAX_FIXTURE_BYTES];
static size_t hello_length;
static uint8_t audio_rom[MAX_FIXTURE_BYTES];
static size_t audio_length;
static uint8_t ipl3_asset[IPL3_ASSET_BYTES];

typedef struct {
  counting_allocator counts;
  t64_host_allocator allocator;
  t64_machine *machine;
} machine_item;

/* A machine of the profile with the image admitted (NULL image: none). */
static bool make_machine(machine_item *item, const char *profile_id,
                         const uint8_t *image, size_t image_length) {
  const t64_profile_descriptor *profile = NULL;

  (void)memset(item, 0, sizeof *item);
  item->allocator = (t64_host_allocator)T64_HOST_ALLOCATOR_INIT(
      counting_allocate, counting_release, &item->counts);
  T64_CHECK_INT(t64_profile_lookup(profile_id, &profile), T64_STATUS_OK);
  if (profile == NULL) return false;
  T64_CHECK_INT(t64_machine_create(
                    &(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(
                        &item->allocator, profile),
                    &item->machine),
                T64_STATUS_OK);
  if (item->machine == NULL) return false;
  if (image != NULL) {
    const t64_status status =
        t64_machine_admit_rom(item->machine, image, image_length);

    T64_CHECK_INT(status, T64_STATUS_OK);
    if (status != T64_STATUS_OK) return false;
  }
  return true;
}

static void destroy_machine(machine_item *item) {
  t64_machine_destroy(item->machine);
  item->machine = NULL;
  T64_CHECK(item->counts.allocations > 0u);
  T64_CHECK_INT(item->counts.releases, item->counts.allocations);
}

static bool digest_of(const t64_machine *machine,
                      uint8_t digest[T64_DIGEST_SIZE]) {
  const t64_status status = t64_machine_digest(machine, digest);

  T64_CHECK_INT(status, T64_STATUS_OK);
  return status == T64_STATUS_OK;
}

/* advance with a request and a result of the given struct_size values: the
 * answer is STRUCT_TOO_SMALL, the sentinel result comes back byte for byte and
 * the machine digest does not move. */
static void advance_refused(t64_machine *machine, bool admitted,
                            uint32_t request_size, uint32_t result_size) {
  t64_stop_request request =
      T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK, ADVANCE_TICK);
  t64_stop_result result =
      T64_STOP_RESULT_INIT_WITH(SENTINEL_U32, SENTINEL_U64);
  uint8_t pristine[sizeof result];
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  bool have_digest = false;

  request.struct_size = request_size;
  result.struct_size = result_size;
  (void)memcpy(pristine, &result, sizeof result);
  if (admitted) have_digest = digest_of(machine, before);
  T64_CHECK_INT(t64_machine_advance(machine, &request, &result),
                T64_STATUS_STRUCT_TOO_SMALL);
  T64_CHECK(memcmp(pristine, &result, sizeof result) == 0);
  if (admitted && have_digest && digest_of(machine, after)) {
    T64_CHECK(memcmp(before, after, sizeof before) == 0);
  }
}

typedef struct {
  t64_stop_request request;
  uint8_t tail[TAIL_BYTES];
} larger_request;

typedef struct {
  t64_stop_result result;
  uint8_t tail[TAIL_BYTES];
} larger_result;

/* advance: a request or a result below its v1 size is refused before any state
 * change (and before the not-admitted answer); a larger caller request and
 * result behave exactly as the v1 pair, write only the v1 members and leave
 * struct_size and the trailing bytes alone. */
static void case_advance(void) {
  const uint32_t request_v1 = (uint32_t)sizeof(t64_stop_request);
  const uint32_t result_v1 = (uint32_t)sizeof(t64_stop_result);
  const uint32_t request_shorter[] = {0u, 1u, 4u, request_v1 - 1u};
  const uint32_t result_shorter[] = {0u, 1u, 4u, result_v1 - 1u};
  machine_item exact;
  machine_item twin;
  machine_item bare;
  larger_request big_request;
  larger_result big_result;
  uint8_t request_pristine[sizeof(larger_request)];
  uint8_t tail_pristine[TAIL_BYTES];
  t64_stop_request request =
      T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK, ADVANCE_TICK);
  t64_stop_result result = T64_STOP_RESULT_INIT;
  uint8_t exact_digest[T64_DIGEST_SIZE];
  uint8_t twin_digest[T64_DIGEST_SIZE];
  size_t index;

  if (!make_machine(&exact, HELLO_PROFILE, hello_rom, hello_length)) return;
  if (!make_machine(&twin, HELLO_PROFILE, hello_rom, hello_length)) {
    destroy_machine(&exact);
    return;
  }
  if (!make_machine(&bare, HELLO_PROFILE, NULL, 0u)) {
    destroy_machine(&exact);
    destroy_machine(&twin);
    return;
  }

  /* Floor: a short request, a short result, both short. */
  for (index = 0u; index < sizeof request_shorter / sizeof request_shorter[0];
       index++) {
    advance_refused(exact.machine, true, request_shorter[index], result_v1);
    advance_refused(exact.machine, true, request_v1, result_shorter[index]);
    advance_refused(exact.machine, true, request_shorter[index],
                    result_shorter[index]);
    advance_refused(bare.machine, false, request_shorter[index], result_v1);
    advance_refused(bare.machine, false, request_v1, result_shorter[index]);
  }
  /* A well-formed pair on a machine without an image is still NOT_READY. */
  T64_CHECK_INT(t64_machine_advance(bare.machine, &request, &result),
                T64_STATUS_NOT_READY);

  /* Larger caller: the same answer, state and members as the v1 pair. */
  T64_CHECK_INT(t64_machine_advance(exact.machine, &request, &result),
                T64_STATUS_OK);
  T64_CHECK(result.reason != 0u);
  (void)memset(&big_request, SENTINEL_BYTE, sizeof big_request);
  (void)memset(&big_result, SENTINEL_BYTE, sizeof big_result);
  big_request.request = (t64_stop_request)T64_STOP_REQUEST_INIT(
      T64_STOP_GUEST_TICK, ADVANCE_TICK);
  big_request.request.struct_size = (uint32_t)sizeof big_request;
  big_result.result.struct_size = (uint32_t)sizeof big_result;
  (void)memcpy(request_pristine, &big_request, sizeof big_request);
  (void)memcpy(tail_pristine, big_result.tail, sizeof tail_pristine);
  T64_CHECK_INT(
      t64_machine_advance(twin.machine, &big_request.request,
                          &big_result.result),
      T64_STATUS_OK);
  T64_CHECK_INT(big_result.result.reason, result.reason);
  T64_CHECK(big_result.result.tick == result.tick);
  T64_CHECK_INT(big_result.result.struct_size, sizeof big_result);
  T64_CHECK(memcmp(big_result.tail, tail_pristine, sizeof tail_pristine) == 0);
  T64_CHECK(memcmp(&big_request, request_pristine, sizeof big_request) == 0);
  T64_CHECK(digest_of(exact.machine, exact_digest));
  T64_CHECK(digest_of(twin.machine, twin_digest));
  T64_CHECK(memcmp(exact_digest, twin_digest, sizeof exact_digest) == 0);
  destroy_machine(&exact);
  destroy_machine(&twin);
  destroy_machine(&bare);
}

/* Advance in strides, resuming after the resumable per-advance event budget
 * stop, until the bound is reached or the machine stops for good. */
static uint32_t advance_to(t64_machine *machine, uint64_t tick) {
  uint32_t reason = 0u;
  uint64_t previous_tick = UINT64_MAX;
  unsigned attempt;

  for (attempt = 0u; attempt < 200000u; attempt++) {
    t64_stop_request request =
        T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK, tick);
    t64_stop_result result = T64_STOP_RESULT_INIT;
    const t64_status status = t64_machine_advance(machine, &request, &result);

    T64_CHECK_INT(status, T64_STATUS_OK);
    if (status != T64_STATUS_OK) return 0u;
    reason = result.reason;
    if (reason != T64_STOP_REASON_RESOURCE_LIMIT) break;
    if (result.tick == previous_tick) return 0u;
    previous_tick = result.tick;
  }
  return reason;
}

/* ---- the generic floor and larger-caller probes --------------------------
 * A probe is a byte buffer big enough for the largest in/out struct and
 * aligned for any of them. The entry under test sees a pointer to it as the
 * struct, with struct_size set by the test. */
typedef union {
  max_align_t alignment;
  uint8_t bytes[sizeof(t64_profile_descriptor) + 2u * TAIL_BYTES];
} probe_buffer;

typedef t64_status (*entry_fn)(t64_machine *machine, void *out, void *aux);

static void probe_fill(probe_buffer *probe, uint32_t struct_size) {
  (void)memset(probe->bytes, SENTINEL_BYTE, sizeof probe->bytes);
  (void)memcpy(probe->bytes, &struct_size, sizeof struct_size);
}

static uint32_t probe_struct_size(const probe_buffer *probe) {
  uint32_t value;

  (void)memcpy(&value, probe->bytes, sizeof value);
  return value;
}

/* Floor: a struct_size below the v1 size is refused with the sentinel struct
 * byte for byte as it was and the machine digest unchanged. */
static void check_floor(const char *label, entry_fn call,
                        t64_machine *machine, void *aux, uint32_t known_size) {
  const uint32_t shorter[] = {0u, 1u, 4u, known_size - 1u};
  size_t index;

  for (index = 0u; index < sizeof shorter / sizeof shorter[0]; index++) {
    probe_buffer probe;
    probe_buffer pristine;
    uint8_t before[T64_DIGEST_SIZE];
    uint8_t after[T64_DIGEST_SIZE];
    bool have_digest = false;
    t64_status status;

    probe_fill(&probe, shorter[index]);
    pristine = probe;
    if (machine != NULL) have_digest = digest_of(machine, before);
    status = call(machine, probe.bytes, aux);
    if (status != T64_STATUS_STRUCT_TOO_SMALL) {
      fprintf(stderr, "%s floor: struct_size %u answered %d\n", label,
              (unsigned)shorter[index], (int)status);
    }
    T64_CHECK_INT(status, T64_STATUS_STRUCT_TOO_SMALL);
    T64_CHECK(memcmp(&probe, &pristine, sizeof probe) == 0);
    if (have_digest && digest_of(machine, after)) {
      T64_CHECK(memcmp(before, after, sizeof before) == 0);
    }
  }
}

/* Larger caller of a repeatable entry: the exact-size call and the larger call
 * both answer want; the larger call writes the same members as the exact one,
 * leaves struct_size and every trailing byte as it was, and the exact call
 * wrote members (it is not a vacuous pass). */
static void check_larger(const char *label, entry_fn call,
                         t64_machine *machine, void *aux, uint32_t known_size,
                         t64_status want) {
  probe_buffer exact;
  probe_buffer larger;
  probe_buffer baseline;
  const uint32_t larger_size = known_size + TAIL_BYTES;
  t64_status status;

  probe_fill(&exact, known_size);
  baseline = exact;
  status = call(machine, exact.bytes, aux);
  if (status != want) {
    fprintf(stderr, "%s exact: answered %d, want %d\n", label, (int)status,
            (int)want);
  }
  T64_CHECK_INT(status, want);
  T64_CHECK_INT(probe_struct_size(&exact), known_size);
  T64_CHECK(memcmp(exact.bytes + sizeof(uint32_t),
                   baseline.bytes + sizeof(uint32_t),
                   known_size - sizeof(uint32_t)) != 0);
  T64_CHECK(memcmp(exact.bytes + known_size, baseline.bytes + known_size,
                   sizeof exact.bytes - known_size) == 0);

  probe_fill(&larger, larger_size);
  baseline = larger;
  status = call(machine, larger.bytes, aux);
  if (status != want) {
    fprintf(stderr, "%s larger: answered %d, want %d\n", label, (int)status,
            (int)want);
  }
  T64_CHECK_INT(status, want);
  T64_CHECK_INT(probe_struct_size(&larger), larger_size);
  T64_CHECK(memcmp(larger.bytes + sizeof(uint32_t),
                   exact.bytes + sizeof(uint32_t),
                   known_size - sizeof(uint32_t)) == 0);
  T64_CHECK(memcmp(larger.bytes + known_size, baseline.bytes + known_size,
                   sizeof larger.bytes - known_size) == 0);
}

static t64_status call_cpu_stop_detail(t64_machine *machine, void *out,
                                       void *aux) {
  (void)aux;
  return t64_machine_cpu_stop_detail(machine, (t64_cpu_stop_detail *)out);
}

typedef struct {
  uint8_t *bytes;
  size_t capacity;
} drain_args;

static t64_status call_output_drain(t64_machine *machine, void *out,
                                    void *aux) {
  const drain_args *args = (const drain_args *)aux;

  return t64_machine_output_drain(machine, args->bytes, args->capacity,
                                  (t64_output_drain_result *)out);
}

typedef struct {
  const uint8_t *bytes;
  size_t length;
  uint32_t flags;
} import_args;

static t64_status call_state_import(t64_machine *machine, void *out,
                                    void *aux) {
  const import_args *args = (const import_args *)aux;

  return t64_machine_state_import(machine, args->bytes, args->length,
                                  args->flags, (t64_state_import_report *)out);
}

static t64_status call_video_info(t64_machine *machine, void *out, void *aux) {
  (void)aux;
  return t64_video_info(machine, (t64_video_packet_info *)out);
}

static t64_status call_audio_info(t64_machine *machine, void *out, void *aux) {
  (void)aux;
  return t64_audio_info(machine, (t64_audio_packet_info *)out);
}

static t64_status call_save_info(t64_machine *machine, void *out, void *aux) {
  const uint32_t *kind = (const uint32_t *)aux;

  return t64_machine_save_info(machine, *kind, (t64_save_info *)out);
}

static t64_status call_output_status(t64_machine *machine, void *out,
                                     void *aux) {
  (void)aux;
  return t64_machine_output_status(machine, (t64_output_status *)out);
}

/* cpu_stop_detail: the floor holds before the terminal stop (it precedes the
 * not-ready answer) and after it; a larger caller gets the v1 members. */
static void case_cpu_stop_detail(void) {
  machine_item item;
  t64_cpu_stop_detail detail = T64_CPU_STOP_DETAIL_INIT;

  if (!make_machine(&item, HELLO_PROFILE, hello_rom, hello_length)) return;
  check_floor("cpu_stop_detail", call_cpu_stop_detail, item.machine, NULL,
              (uint32_t)sizeof(t64_cpu_stop_detail));
  T64_CHECK_INT(t64_machine_cpu_stop_detail(item.machine, &detail),
                T64_STATUS_NOT_READY);
  T64_CHECK_INT(advance_to(item.machine, HELLO_BOUND_TICK),
                T64_STOP_REASON_GUEST_COMPLETION);
  T64_CHECK_INT(t64_machine_cpu_stop_detail(item.machine, &detail),
                T64_STATUS_OK);
  T64_CHECK(detail.retired_instructions != 0u);
  check_floor("cpu_stop_detail", call_cpu_stop_detail, item.machine, NULL,
              (uint32_t)sizeof(t64_cpu_stop_detail));
  check_larger("cpu_stop_detail", call_cpu_stop_detail, item.machine, NULL,
               (uint32_t)sizeof(t64_cpu_stop_detail), T64_STATUS_OK);
  destroy_machine(&item);
}

/* output_drain: floor; a larger result with an undersized capacity (the result
 * is written, nothing is consumed) equals the exact-size result; then a larger
 * result on a real drain returns the transcript and keeps struct_size and the
 * tail. */
static void case_output_drain(void) {
  machine_item item;
  drain_args empty = {NULL, 0u};
  uint8_t drained[HELLO_BYTES + 2u];
  drain_args roomy = {drained, sizeof drained};
  struct {
    t64_output_drain_result result;
    uint8_t tail[TAIL_BYTES];
  } big;
  uint8_t tail_pristine[TAIL_BYTES];

  if (!make_machine(&item, HELLO_PROFILE, hello_rom, hello_length)) return;
  T64_CHECK_INT(advance_to(item.machine, HELLO_BOUND_TICK),
                T64_STOP_REASON_GUEST_COMPLETION);
  check_floor("output_drain", call_output_drain, item.machine, &empty,
              (uint32_t)sizeof(t64_output_drain_result));
  check_floor("output_drain", call_output_drain, item.machine, &roomy,
              (uint32_t)sizeof(t64_output_drain_result));
  check_larger("output_drain", call_output_drain, item.machine, &empty,
               (uint32_t)sizeof(t64_output_drain_result),
               T64_STATUS_BUFFER_TOO_SMALL);

  (void)memset(&big, SENTINEL_BYTE, sizeof big);
  big.result.struct_size = (uint32_t)sizeof big;
  (void)memcpy(tail_pristine, big.tail, sizeof tail_pristine);
  (void)memset(drained, 0, sizeof drained);
  T64_CHECK_INT(
      t64_machine_output_drain(item.machine, drained, sizeof drained,
                               &big.result),
      T64_STATUS_OK);
  T64_CHECK_INT(big.result.struct_size, sizeof big);
  T64_CHECK_INT(big.result.copied_bytes, HELLO_BYTES);
  T64_CHECK_INT(big.result.required_bytes, HELLO_BYTES);
  T64_CHECK(memcmp(drained, HELLO_TRANSCRIPT, HELLO_BYTES) == 0);
  T64_CHECK(memcmp(big.tail, tail_pristine, sizeof tail_pristine) == 0);
  destroy_machine(&item);
}

/* state_import: a report below its v1 size is refused before the blob is read
 * (a NULL blob still answers STRUCT_TOO_SMALL) with the machine unchanged; a
 * NULL report is still accepted; a larger report equals the exact-size report
 * on a refusal and on a real import. */
static void case_state_import(void) {
  machine_item item;
  uint8_t garbage[4096];
  uint8_t *blob;
  size_t blob_length = 0u;
  import_args bad = {garbage, sizeof garbage, 0u};
  import_args no_blob = {NULL, 0u, 0u};
  import_args good;
  uint8_t before[T64_DIGEST_SIZE];
  uint8_t after[T64_DIGEST_SIZE];
  probe_buffer probe;
  t64_status status;
  uint32_t larger_size;

  (void)memset(garbage, 0x5a, sizeof garbage);
  if (!make_machine(&item, SAVE_PROFILE, ipl3_asset, sizeof ipl3_asset)) {
    return;
  }
  T64_CHECK_INT(advance_to(item.machine, UINT64_C(20000)) != 0u, true);
  T64_CHECK_INT(t64_machine_state_export(item.machine, NULL, 0u, &blob_length),
                T64_STATUS_BUFFER_TOO_SMALL);
  blob = (uint8_t *)malloc(blob_length);
  if (blob == NULL) {
    T64_CHECK(false);
    destroy_machine(&item);
    return;
  }
  T64_CHECK_INT(
      t64_machine_state_export(item.machine, blob, blob_length, &blob_length),
      T64_STATUS_OK);
  good.bytes = blob;
  good.length = blob_length;
  good.flags = 0u;

  check_floor("state_import", call_state_import, item.machine, &bad,
              (uint32_t)sizeof(t64_state_import_report));
  check_floor("state_import", call_state_import, item.machine, &good,
              (uint32_t)sizeof(t64_state_import_report));
  check_floor("state_import", call_state_import, item.machine, &no_blob,
              (uint32_t)sizeof(t64_state_import_report));
  /* No report is still allowed: the refusal is the blob's, not the size's. */
  T64_CHECK_INT(t64_machine_state_import(item.machine, garbage, sizeof garbage,
                                         0u, NULL),
                T64_STATUS_STATE_FORMAT);
  check_larger("state_import", call_state_import, item.machine, &bad,
               (uint32_t)sizeof(t64_state_import_report),
               T64_STATUS_STATE_FORMAT);

  /* A real import through a larger report: the same state comes back. */
  T64_CHECK(digest_of(item.machine, before));
  larger_size = (uint32_t)sizeof(t64_state_import_report) + TAIL_BYTES;
  probe_fill(&probe, larger_size);
  status = call_state_import(item.machine, probe.bytes, &good);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(probe_struct_size(&probe), larger_size);
  {
    t64_state_import_report report = T64_STATE_IMPORT_REPORT_INIT;
    uint8_t baseline[TAIL_BYTES];

    (void)memcpy(&report, probe.bytes, sizeof report);
    T64_CHECK_INT(report.status, T64_STATUS_OK);
    (void)memset(baseline, SENTINEL_BYTE, sizeof baseline);
    T64_CHECK(memcmp(probe.bytes + sizeof(t64_state_import_report), baseline,
                     sizeof baseline) == 0);
  }
  T64_CHECK(digest_of(item.machine, after));
  T64_CHECK(memcmp(before, after, sizeof before) == 0);
  check_larger("state_import", call_state_import, item.machine, &good,
               (uint32_t)sizeof(t64_state_import_report), T64_STATUS_OK);
  free(blob);
  destroy_machine(&item);
}

/* The packet and status observers need a machine with raw video and audio
 * packets queued at once: the audio fixture on the AI profile. The video ring
 * fills first (two packets) and then advance stops resumably at backpressure,
 * so the video ring is drained until the first audio packet exists. */
static uint8_t video_scratch[320u * 240u * 4u];

static bool av_machine(machine_item *item) {
  uint64_t tick = 0u;
  unsigned stride;

  if (!make_machine(item, AV_PROFILE, audio_rom, audio_length)) return false;
  for (stride = 0u; stride < 64u; stride++) {
    t64_video_packet_info video = T64_VIDEO_PACKET_INFO_INIT;
    t64_audio_packet_info audio = T64_AUDIO_PACKET_INFO_INIT;
    size_t length = 0u;

    tick += UINT64_C(50000);
    (void)advance_to(item->machine, tick);
    if (t64_audio_info(item->machine, &audio) == T64_STATUS_OK) {
      if (t64_video_info(item->machine, &video) == T64_STATUS_OK) return true;
      continue;
    }
    while (t64_video_copy(item->machine, video_scratch, sizeof video_scratch,
                          &length) == T64_STATUS_OK) {
    }
  }
  T64_CHECK(false);
  destroy_machine(item);
  return false;
}

static void case_video_info(void) {
  machine_item item;

  if (!av_machine(&item)) return;
  check_floor("video_info", call_video_info, item.machine, NULL,
              (uint32_t)sizeof(t64_video_packet_info));
  check_larger("video_info", call_video_info, item.machine, NULL,
               (uint32_t)sizeof(t64_video_packet_info), T64_STATUS_OK);
  destroy_machine(&item);
}

static void case_audio_info(void) {
  machine_item item;

  if (!av_machine(&item)) return;
  check_floor("audio_info", call_audio_info, item.machine, NULL,
              (uint32_t)sizeof(t64_audio_packet_info));
  check_larger("audio_info", call_audio_info, item.machine, NULL,
               (uint32_t)sizeof(t64_audio_packet_info), T64_STATUS_OK);
  destroy_machine(&item);
}

/* output_status (plan 11-04) joins the matrix so every in/out struct has both
 * cases in one place. */
static void case_output_status(void) {
  machine_item item;

  if (!av_machine(&item)) return;
  check_floor("output_status", call_output_status, item.machine, NULL,
              (uint32_t)sizeof(t64_output_status));
  check_larger("output_status", call_output_status, item.machine, NULL,
               (uint32_t)sizeof(t64_output_status), T64_STATUS_OK);
  destroy_machine(&item);
}

static void case_save_info(void) {
  machine_item item;
  const t64_profile_descriptor *profile = NULL;
  uint32_t kind;

  if (!make_machine(&item, SAVE_PROFILE, ipl3_asset, sizeof ipl3_asset)) {
    return;
  }
  T64_CHECK_INT(t64_machine_profile(item.machine, &profile), T64_STATUS_OK);
  if (profile == NULL) {
    destroy_machine(&item);
    return;
  }
  kind = profile->save_kind;
  T64_CHECK(kind != 0u);
  check_floor("save_info", call_save_info, item.machine, &kind,
              (uint32_t)sizeof(t64_save_info));
  check_larger("save_info", call_save_info, item.machine, &kind,
               (uint32_t)sizeof(t64_save_info), T64_STATUS_OK);
  destroy_machine(&item);
}

/* t64_profile_hash and t64_profile_serialize read a descriptor: a struct_size
 * below the v1 size is refused with every output untouched; a larger caller
 * descriptor hashes and serializes exactly as the v1 one (named members only,
 * never the struct_size member or the bytes after the known layout). */
static void case_profile_descriptor(void) {
  const t64_profile_descriptor *registered = NULL;
  const uint32_t known = (uint32_t)sizeof(t64_profile_descriptor);
  const uint32_t shorter[] = {0u, 1u, 4u, known - 1u};
  uint8_t reference[T64_DIGEST_SIZE];
  uint8_t reference_bytes[T64_PROFILE_SERIALIZED_MAX_BYTES];
  size_t reference_length = 0u;
  probe_buffer probe;
  probe_buffer pristine;
  size_t index;

  T64_CHECK_INT(t64_profile_lookup("ntsc-cart-4m-skeleton", &registered),
                T64_STATUS_OK);
  if (registered == NULL) return;
  T64_CHECK_INT(t64_profile_hash(registered, reference), T64_STATUS_OK);
  T64_CHECK_INT(t64_profile_serialize(registered, reference_bytes,
                                      sizeof reference_bytes,
                                      &reference_length),
                T64_STATUS_OK);

  for (index = 0u; index < sizeof shorter / sizeof shorter[0]; index++) {
    uint8_t digest[T64_DIGEST_SIZE];
    uint8_t digest_pristine[T64_DIGEST_SIZE];
    uint8_t bytes[T64_PROFILE_SERIALIZED_MAX_BYTES];
    uint8_t bytes_pristine[T64_PROFILE_SERIALIZED_MAX_BYTES];
    size_t length = (size_t)SENTINEL_U64;

    probe_fill(&probe, known);
    (void)memcpy(probe.bytes, registered, known);
    (void)memcpy(probe.bytes, &shorter[index], sizeof shorter[index]);
    pristine = probe;
    (void)memset(digest, SENTINEL_BYTE, sizeof digest);
    (void)memcpy(digest_pristine, digest, sizeof digest);
    (void)memset(bytes, SENTINEL_BYTE, sizeof bytes);
    (void)memcpy(bytes_pristine, bytes, sizeof bytes);
    T64_CHECK_INT(
        t64_profile_hash((const t64_profile_descriptor *)(const void *)probe.bytes,
                         digest),
        T64_STATUS_STRUCT_TOO_SMALL);
    T64_CHECK(memcmp(digest, digest_pristine, sizeof digest) == 0);
    T64_CHECK_INT(
        t64_profile_serialize(
            (const t64_profile_descriptor *)(const void *)probe.bytes, bytes,
            sizeof bytes, &length),
        T64_STATUS_STRUCT_TOO_SMALL);
    T64_CHECK(memcmp(bytes, bytes_pristine, sizeof bytes) == 0);
    T64_CHECK(length == (size_t)SENTINEL_U64);
    T64_CHECK(memcmp(&probe, &pristine, sizeof probe) == 0);
  }

  /* A larger caller descriptor: the known layout is read, nothing else. */
  {
    const uint32_t larger_size = known + TAIL_BYTES;
    uint8_t digest[T64_DIGEST_SIZE];
    uint8_t bytes[T64_PROFILE_SERIALIZED_MAX_BYTES];
    size_t length = 0u;

    probe_fill(&probe, known);
    (void)memcpy(probe.bytes, registered, known);
    (void)memcpy(probe.bytes, &larger_size, sizeof larger_size);
    pristine = probe;
    T64_CHECK_INT(
        t64_profile_hash((const t64_profile_descriptor *)(const void *)probe.bytes,
                         digest),
        T64_STATUS_OK);
    T64_CHECK(memcmp(digest, reference, sizeof digest) == 0);
    T64_CHECK_INT(
        t64_profile_serialize(
            (const t64_profile_descriptor *)(const void *)probe.bytes, bytes,
            sizeof bytes, &length),
        T64_STATUS_OK);
    T64_CHECK(length == reference_length);
    T64_CHECK(length <= sizeof bytes &&
              memcmp(bytes, reference_bytes, length) == 0);
    T64_CHECK(memcmp(&probe, &pristine, sizeof probe) == 0);
    T64_CHECK_INT(probe_struct_size(&probe), larger_size);
  }
}

typedef struct {
  const char *name;
  void (*run)(void);
} named_case;

static const named_case handshake_cases[] = {
    {"handshake", case_handshake},
    {"profile-hash-stable", case_profile_hash_stable},
    {"trace-record-size", case_trace_record_size},
};

static const named_case struct_size_cases[] = {
    {"advance", case_advance},
    {"cpu-stop-detail", case_cpu_stop_detail},
    {"output-drain", case_output_drain},
    {"state-import", case_state_import},
    {"video-info", case_video_info},
    {"audio-info", case_audio_info},
    {"output-status", case_output_status},
    {"save-info", case_save_info},
    {"profile-descriptor", case_profile_descriptor},
};

static const named_case *find_case(const named_case *table, size_t count,
                                   const char *name) {
  size_t index;

  for (index = 0u; index < count; index++) {
    if (strcmp(table[index].name, name) == 0) return &table[index];
  }
  return NULL;
}

static void run_group(const named_case *table, size_t count) {
  size_t index;

  for (index = 0u; index < count; index++) table[index].run();
}

#define HANDSHAKE_COUNT (sizeof handshake_cases / sizeof handshake_cases[0])
#define STRUCT_SIZE_COUNT \
  (sizeof struct_size_cases / sizeof struct_size_cases[0])

static bool load_fixtures(void) {
  const char *ipl3_path = getenv("T64_TEST_IPL3_PATH");
  size_t ipl3_length = 0u;

  return read_fixture("hello.z64", hello_rom, sizeof hello_rom,
                      &hello_length) &&
         read_fixture("audio-pcm.z64", audio_rom, sizeof audio_rom,
                      &audio_length) &&
         ipl3_path != NULL && ipl3_path[0] != '\0' &&
         read_file(ipl3_path, ipl3_asset, sizeof ipl3_asset, &ipl3_length) &&
         ipl3_length == sizeof ipl3_asset;
}

/* The struct-size cases need the boot fixtures; the handshake cases do not. */
static bool fixtures_for(const char *case_name) {
  if (find_case(struct_size_cases, STRUCT_SIZE_COUNT, case_name) == NULL) {
    return true;
  }
  if (!load_fixtures()) {
    fprintf(stderr, "T64_BOOT_FIXTURES_DIR and T64_TEST_IPL3_PATH must name the fixtures\n");
    return false;
  }
  return true;
}

int main(int argc, char **argv) {
  const named_case *selected;
  size_t index;

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (index = 0u; index < HANDSHAKE_COUNT; index++) {
      printf("%s\n", handshake_cases[index].name);
    }
    for (index = 0u; index < STRUCT_SIZE_COUNT; index++) {
      printf("%s\n", struct_size_cases[index].name);
    }
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    selected = find_case(handshake_cases, HANDSHAKE_COUNT, argv[2]);
    if (selected == NULL) {
      selected = find_case(struct_size_cases, STRUCT_SIZE_COUNT, argv[2]);
    }
    if (selected == NULL) {
      fprintf(stderr, "unknown case: %s\n", argv[2]);
      return 2;
    }
    if (!fixtures_for(argv[2])) return 2;
    selected->run();
    return T64_TEST_FINISH(argv[2]);
  }
  if (argc == 3 && strcmp(argv[1], "--group") == 0 &&
      strcmp(argv[2], "struct-size") == 0) {
    if (!load_fixtures()) {
      fprintf(stderr, "T64_BOOT_FIXTURES_DIR and T64_TEST_IPL3_PATH must name the fixtures\n");
      return 2;
    }
    run_group(struct_size_cases, STRUCT_SIZE_COUNT);
    return T64_TEST_FINISH("unit.core.api.struct-size");
  }
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--list-cases|--case NAME|--group struct-size]\n",
            argv[0]);
    return 2;
  }
  run_group(handshake_cases, HANDSHAKE_COUNT);
  return T64_TEST_FINISH("unit.core.api.handshake");
}
