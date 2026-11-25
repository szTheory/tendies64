/*
 * Digest sweep printer (plan 10-02).
 *
 * Linked against ONE tree's core library by scripts/digest-sweep.py, which
 * runs it once per tree and compares the sorted outputs. For every registered
 * id of the --ids file and every candidate ROM of the --fixtures directory it
 * creates the machine, tries admission and prints one line per observation:
 *
 *   skip ID ROM STATUS            admission (or the ROM file) was refused
 *   ID ROM TICK DIGESTHEX         canonical machine digest at a checkpoint
 *   stop ID ROM TICK STATUS REASON STOPTICK
 *                                 an advance ended before its bound; the
 *                                 digest line of that checkpoint follows and
 *                                 the checkpoints after it are not run
 *   ID export SHA256HEX           SHA-256 of the state container of the first
 *                                 admitted ROM at its last checkpoint, for
 *                                 ids whose profile supports state export
 *                                 (the four si-v1 ids, container version 1,
 *                                 and since plan 10-11 the four si-v2 ids,
 *                                 container version 2: head-only lines, the
 *                                 base list has no such id)
 *
 * Only the public machine API is used so the same source builds against the
 * base tree and the head tree. No path, host name or time is printed. The ROMs
 * are authored or cleared fixtures already built into build/fixtures; this
 * program only reads them.
 *
 * Two API generations (plan 11-01, PATTERNS H4). The Phase 11 shape wave
 * replaces the positional struct initializers and the three-argument create
 * call with the public init macros and a create-info, so one source has to
 * compile against both. The head path is selected by whether the header
 * defines T64_MACHINE_CREATE_INFO_INIT (born with the flip of plan 11-12);
 * the base path is the code as it was before. The selector is a defined()
 * test, never a bare macro value, because -Wundef is on. Every site that
 * differs is behind the local names SWEEP_ALLOCATOR_INIT,
 * SWEEP_STOP_REQUEST_INIT, SWEEP_STOP_RESULT_INIT and sweep_create, so the
 * observations printed are the same in both generations. The allocator
 * callbacks keep their signatures in both generations and write the storage
 * fields by name, which compiles whether a member is size_t or uint64_t.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_sha256.h"
#include "tendies64/t64_machine.h"

#define MAX_ROM_BYTES (64u * 1024u * 1024u)
#define MAX_ID_BYTES 128u
#define AUTHORED_ROM_BYTES 4096u
/* The replay player bound (REPLAY_MAX_YIELDS_WITHOUT_PROGRESS in
 * runner/t64_replay.c): the digest of a stalled machine depends on how many
 * advance calls were made, so the sweep makes the same number as the player. */
#define YIELDS_WITHOUT_PROGRESS 64u

/* Candidate ROMs, in output order of the first admitted one (used for the
 * export line). "authored-4k" is generated in memory (the image the skeleton
 * probe and core/tests/test_digest_v25.c use), every other name is a file of
 * the fixtures directory. */
static const char *const candidates[] = {
    "ipl3_compat.z64",           "hello.z64",
    "rdp-fill.z64",              "audio-pcm.z64",
    "phase09-admit-si-v1.z64",   "phase09-admit-e4k.z64",
    "phase09-admit-e16k.z64",    "phase09-admit-s32k.z64",
    "authored-4k"};
#define CANDIDATE_COUNT (sizeof candidates / sizeof candidates[0])

/* Guest ticks of the checkpoints; tick 0 is the machine right after
 * admission. */
static const uint64_t checkpoints[] = {
    UINT64_C(0),       UINT64_C(1),       UINT64_C(16),
    UINT64_C(256),     UINT64_C(4096),    UINT64_C(65536),
    UINT64_C(262144),  UINT64_C(1048576), UINT64_C(2097152),
    UINT64_C(4194304)};
#define CHECKPOINT_COUNT (sizeof checkpoints / sizeof checkpoints[0])

static bool sweep_allocate(void *context, size_t size, size_t alignment,
                           t64_host_storage *storage_out) {
  void *memory;

  (void)context;
  if (storage_out == NULL || size == 0u || alignment == 0u) return false;
  memory = malloc(size);
  if (memory == NULL || (uintptr_t)memory % alignment != 0u) {
    free(memory);
    return false;
  }
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = alignment;
  return true;
}

static void sweep_release(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

#if defined(T64_MACHINE_CREATE_INFO_INIT)
/* Head generation: public init macros and the create-info handshake. */
#define SWEEP_ALLOCATOR_INIT \
  T64_HOST_ALLOCATOR_INIT(sweep_allocate, sweep_release, NULL)
#define SWEEP_STOP_REQUEST_INIT(kind, value) T64_STOP_REQUEST_INIT(kind, value)
#define SWEEP_STOP_RESULT_INIT T64_STOP_RESULT_INIT
#else
/* Base generation: positional initializers and the three-argument create. */
#define SWEEP_ALLOCATOR_INIT {sweep_allocate, sweep_release, NULL}
#define SWEEP_STOP_REQUEST_INIT(kind, value) {kind, value}
#define SWEEP_STOP_RESULT_INIT {0u, 0u}
#endif

static const t64_host_allocator allocator = SWEEP_ALLOCATOR_INIT;

static t64_status sweep_create(const t64_profile_descriptor *profile,
                               t64_machine **machine_out) {
#if defined(T64_MACHINE_CREATE_INFO_INIT)
  t64_machine_create_info info = T64_MACHINE_CREATE_INFO_INIT(&allocator,
                                                              profile);

  return t64_machine_create(&info, machine_out);
#else
  return t64_machine_create(&allocator, profile, machine_out);
#endif
}

static void to_hex(const uint8_t *bytes, size_t length, char *hex) {
  static const char digits[] = "0123456789abcdef";
  size_t i;

  for (i = 0u; i < length; i++) {
    hex[2u * i] = digits[bytes[i] >> 4];
    hex[2u * i + 1u] = digits[bytes[i] & 0x0fu];
  }
  hex[2u * length] = '\0';
}

/* Same authored 4 KiB image as core/tests/test_digest_v25.c. */
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

/* Read a fixture file; NULL when it is missing, empty or too large. */
static uint8_t *read_file(const char *directory, const char *name,
                          size_t *length_out) {
  char path[4096];
  FILE *file;
  long length;
  uint8_t *bytes;
  int written;

  written = snprintf(path, sizeof path, "%s/%s", directory, name);
  if (written < 0 || (size_t)written >= sizeof path) return NULL;
  file = fopen(path, "rb");
  if (file == NULL) return NULL;
  if (fseek(file, 0L, SEEK_END) != 0 || (length = ftell(file)) <= 0 ||
      (unsigned long)length > MAX_ROM_BYTES || fseek(file, 0L, SEEK_SET) != 0) {
    (void)fclose(file);
    return NULL;
  }
  bytes = (uint8_t *)malloc((size_t)length);
  if (bytes == NULL ||
      fread(bytes, 1u, (size_t)length, file) != (size_t)length) {
    free(bytes);
    (void)fclose(file);
    return NULL;
  }
  (void)fclose(file);
  *length_out = (size_t)length;
  return bytes;
}

static bool print_digest(const t64_machine *machine, const char *id,
                         const char *rom, uint64_t tick) {
  uint8_t digest[T64_DIGEST_SIZE];
  char hex[2u * T64_DIGEST_SIZE + 1u];

  if (t64_machine_digest(machine, digest) != T64_STATUS_OK) {
    printf("skip %s %s digest-refused\n", id, rom);
    return false;
  }
  to_hex(digest, sizeof digest, hex);
  printf("%s %s %llu %s\n", id, rom, (unsigned long long)tick, hex);
  return true;
}

/* The state container hash of an admitted machine, when its profile supports
 * export. Returns 0 on success, 1 when export is not supported, 2 on a
 * failure. */
static int export_hash(const t64_machine *machine, char hex[65]) {
  size_t length = 0u;
  uint8_t *bytes;
  t64_status status;
  t64_sha256_ctx context;
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  size_t written = 0u;

  status = t64_machine_state_export(machine, NULL, 0u, &length);
  if (status == T64_STATUS_INVALID_ARGUMENT) return 1;
  if (status != T64_STATUS_BUFFER_TOO_SMALL || length == 0u) return 2;
  bytes = (uint8_t *)malloc(length);
  if (bytes == NULL) return 2;
  status = t64_machine_state_export(machine, bytes, length, &written);
  if (status != T64_STATUS_OK || written != length) {
    free(bytes);
    return 2;
  }
  t64_sha256_init(&context);
  t64_sha256_update(&context, bytes, written);
  t64_sha256_final(&context, digest);
  t64_sha256_hex(digest, hex);
  free(bytes);
  return 0;
}

/* One id and one ROM. Returns true when the ROM was admitted. When
 * export_pending is set and the profile supports export, the container hash
 * of this run's last checkpoint is printed and export_pending cleared. */
static bool sweep_one(const t64_profile_descriptor *profile, const char *id,
                      const char *rom, const uint8_t *image, size_t length,
                      bool *export_pending) {
  t64_machine *machine = NULL;
  t64_status status;
  size_t index;

  status = sweep_create(profile, &machine);
  if (status != T64_STATUS_OK || machine == NULL) {
    printf("skip %s %s create-%u\n", id, rom, (unsigned)status);
    return false;
  }
  status = t64_machine_admit_rom(machine, image, length);
  if (status != T64_STATUS_OK) {
    printf("skip %s %s %u\n", id, rom, (unsigned)status);
    t64_machine_destroy(machine);
    return false;
  }
  for (index = 0u; index < CHECKPOINT_COUNT; index++) {
    uint64_t tick = checkpoints[index];
    bool stopped = false;

    if (tick != 0u) {
      t64_stop_request request = SWEEP_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK,
                                                         tick);
      t64_stop_result result = SWEEP_STOP_RESULT_INIT;
      unsigned stalled = 0u;

      /* A resource-limit stop is a yield: the host calls advance again, as
       * the replay player does, until the bound is reached or the machine
       * stops for good or stops making progress. */
      for (;;) {
        uint64_t before = result.tick;

        status = t64_machine_advance(machine, &request, &result);
        if (status != T64_STATUS_OK ||
            result.reason != T64_STOP_REASON_RESOURCE_LIMIT) {
          break;
        }
        stalled = result.tick > before ? 0u : stalled + 1u;
        if (stalled > YIELDS_WITHOUT_PROGRESS) break;
      }
      if (status != T64_STATUS_OK || result.reason != T64_STOP_REASON_BOUND) {
        printf("stop %s %s %llu %u %u %llu\n", id, rom,
               (unsigned long long)tick, (unsigned)status,
               (unsigned)result.reason, (unsigned long long)result.tick);
        stopped = true;
      }
    }
    if (!print_digest(machine, id, rom, tick)) break;
    if (stopped) break;
  }
  if (*export_pending) {
    char hex[65];
    int outcome = export_hash(machine, hex);

    if (outcome == 0) printf("%s export %s\n", id, hex);
    if (outcome == 2) printf("skip %s export export-failed\n", id);
    /* One attempt per id: supported, unsupported or failed. */
    *export_pending = false;
  }
  t64_machine_destroy(machine);
  return true;
}

static bool sweep_id(const char *directory, const char *id) {
  const t64_profile_descriptor *profile = NULL;
  bool export_pending = true;
  size_t candidate;
  t64_status status = t64_profile_lookup(id, &profile);

  if (status != T64_STATUS_OK || profile == NULL) {
    printf("skip %s - lookup-%u\n", id, (unsigned)status);
    return true;
  }
  for (candidate = 0u; candidate < CANDIDATE_COUNT; candidate++) {
    const char *rom = candidates[candidate];
    uint8_t authored[AUTHORED_ROM_BYTES];
    uint8_t *loaded = NULL;
    const uint8_t *image;
    size_t length;

    if (strcmp(rom, "authored-4k") == 0) {
      fill_authored_rom(authored);
      image = authored;
      length = sizeof authored;
    } else {
      loaded = read_file(directory, rom, &length);
      if (loaded == NULL) {
        printf("skip %s %s file-missing\n", id, rom);
        continue;
      }
      image = loaded;
    }
    (void)sweep_one(profile, id, rom, image, length, &export_pending);
    free(loaded);
  }
  return true;
}

int main(int argc, char **argv) {
  const char *directory = NULL;
  const char *ids_path = NULL;
  char line[MAX_ID_BYTES];
  FILE *ids;
  int index;

  for (index = 1; index < argc; index++) {
    if (strcmp(argv[index], "--fixtures") == 0 && index + 1 < argc) {
      directory = argv[++index];
    } else if (strcmp(argv[index], "--ids") == 0 && index + 1 < argc) {
      ids_path = argv[++index];
    } else {
      fprintf(stderr, "usage: %s --fixtures DIR --ids FILE\n", argv[0]);
      return 2;
    }
  }
  if (directory == NULL || ids_path == NULL) {
    fprintf(stderr, "usage: %s --fixtures DIR --ids FILE\n", argv[0]);
    return 2;
  }
  ids = fopen(ids_path, "r");
  if (ids == NULL) {
    fprintf(stderr, "cannot read the id list\n");
    return 2;
  }
  while (fgets(line, sizeof line, ids) != NULL) {
    size_t length = strlen(line);

    while (length != 0u &&
           (line[length - 1u] == '\n' || line[length - 1u] == '\r')) {
      line[--length] = '\0';
    }
    if (length == 0u || line[0] == '#') continue;
    (void)sweep_id(directory, line);
  }
  (void)fclose(ids);
  return 0;
}
