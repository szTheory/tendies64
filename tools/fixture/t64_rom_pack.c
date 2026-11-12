#define _POSIX_C_SOURCE 200809L

#include "t64_sha256.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

#define T64_ROM_LIMIT_BYTES ((size_t)1048576)
#define T64_CPU_PROFILE_ROM_LIMIT_BYTES ((size_t)0x00401000)
#define T64_HEADER_BYTES ((size_t)64)
#define T64_IPL3_SOURCE_BYTES ((size_t)4096)
#define T64_IPL3_PAYLOAD_OFFSET ((size_t)64)
#define T64_IPL3_PAYLOAD_BYTES (T64_IPL3_SOURCE_BYTES - T64_IPL3_PAYLOAD_OFFSET)
#define T64_FLAT_PAYLOAD_OFFSET ((size_t)4096)
#define T64_MAX_FLAT_PAYLOAD_BYTES \
  (T64_ROM_LIMIT_BYTES - T64_FLAT_PAYLOAD_OFFSET)
#define T64_CPU_PROFILE_MAX_FLAT_PAYLOAD_BYTES \
  (T64_CPU_PROFILE_ROM_LIMIT_BYTES - T64_FLAT_PAYLOAD_OFFSET)
#define T64_N64_MAGIC UINT32_C(0x80371240)
#define T64_KSEG0_BEGIN UINT32_C(0x80000000)
#define T64_KSEG1_BEGIN UINT32_C(0xa0000000)
#define T64_ROM_CLOCK_NTSC UINT32_C(0x0000000f)
#define T64_ROM_ENTRY UINT32_C(0x80000400)

static const char expected_asset_sha256[] =
    "f522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068";
static const char expected_payload_sha256[] =
    "f716a90b1d36ec4253d096790791256d3543455e54473a755ff8c3d1199b15d5";
static const char rom_title[] = "TENDIES64 HELLO     ";

typedef struct {
  uint8_t *data;
  size_t size;
} byte_buffer;

static bool flat_payload_fits(size_t payload_bytes, bool cpu_profile)
{
  size_t maximum = cpu_profile ? T64_CPU_PROFILE_MAX_FLAT_PAYLOAD_BYTES
                               : T64_MAX_FLAT_PAYLOAD_BYTES;
  return payload_bytes > 0u && payload_bytes <= maximum;
}

static bool entry_range_fits(uint32_t entry, size_t payload_bytes,
                             bool cpu_profile)
{
  uint32_t remaining;

  if (!flat_payload_fits(payload_bytes, cpu_profile) ||
      (entry & UINT32_C(3)) != 0u || entry < T64_KSEG0_BEGIN ||
      entry >= T64_KSEG1_BEGIN) {
    return false;
  }
  if (cpu_profile) {
    uint32_t physical_entry = entry & UINT32_C(0x1fffffff);
    return physical_entry < UINT32_C(0x00400000) &&
           (uint64_t)physical_entry + (uint64_t)payload_bytes <=
               UINT64_C(0x00400000);
  }
  remaining = T64_KSEG1_BEGIN - entry;
  return payload_bytes <= (size_t)remaining;
}

static void put_be32(uint8_t *out, uint32_t value)
{
  out[0] = (uint8_t)(value >> 24);
  out[1] = (uint8_t)(value >> 16);
  out[2] = (uint8_t)(value >> 8);
  out[3] = (uint8_t)value;
}

static uint32_t get_be32(const uint8_t *in)
{
  return ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) |
         ((uint32_t)in[2] << 8) | (uint32_t)in[3];
}

static void make_header(uint8_t header[T64_HEADER_BYTES], uint32_t entry,
                        uint32_t clock, uint32_t crc2, size_t payload_bytes)
{
  (void)memset(header, 0, T64_HEADER_BYTES);
  put_be32(header + 0x00u, T64_N64_MAGIC);
  put_be32(header + 0x04u, clock);
  put_be32(header + 0x08u, entry);
  put_be32(header + 0x0cu, 0u);
  put_be32(header + 0x10u, (uint32_t)payload_bytes);
  /* The pinned flat loader owns 0x10 as its checked load-length field. */
  put_be32(header + 0x14u, crc2);
  (void)memcpy(header + 0x20u, rom_title, sizeof rom_title - 1u);
  header[0x3au] = (uint8_t)'E';
}

static void sha256_hex(const uint8_t *data, size_t size,
                       char out[T64_SHA256_HEX_SIZE])
{
  t64_sha256_ctx ctx;
  uint8_t digest[T64_SHA256_DIGEST_SIZE];

  t64_sha256_init(&ctx);
  t64_sha256_update(&ctx, data, size);
  t64_sha256_final(&ctx, digest);
  t64_sha256_hex(digest, out);
}

static bool read_bounded_file(const char *path, size_t maximum,
                              byte_buffer *out)
{
  struct stat path_stat;
  struct stat file_stat;
  uint8_t *bytes = NULL;
  size_t size;
  size_t offset = 0u;
  int fd = -1;
  bool ok = false;

  out->data = NULL;
  out->size = 0u;
  if (lstat(path, &path_stat) != 0 || !S_ISREG(path_stat.st_mode) ||
      path_stat.st_size < 0 || (uintmax_t)path_stat.st_size > (uintmax_t)maximum) {
    return false;
  }
  fd = open(path, O_RDONLY | O_NOFOLLOW);
  if (fd < 0 || fstat(fd, &file_stat) != 0 || !S_ISREG(file_stat.st_mode) ||
      file_stat.st_dev != path_stat.st_dev || file_stat.st_ino != path_stat.st_ino ||
      file_stat.st_size != path_stat.st_size || file_stat.st_size < 0 ||
      (uintmax_t)file_stat.st_size > (uintmax_t)maximum) {
    goto done;
  }

  size = (size_t)file_stat.st_size;
  bytes = (uint8_t *)malloc(size == 0u ? 1u : size);
  if (bytes == NULL) goto done;

  while (offset < size) {
    ssize_t count = read(fd, bytes + offset, size - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) goto done;
    offset += (size_t)count;
  }
  {
    uint8_t extra;
    ssize_t count;
    do {
      count = read(fd, &extra, 1u);
    } while (count < 0 && errno == EINTR);
    if (count != 0) goto done;
  }

  out->data = bytes;
  out->size = size;
  bytes = NULL;
  ok = true;

done:
  free(bytes);
  if (fd >= 0) (void)close(fd);
  return ok;
}

static bool expected_boot_bytes(const byte_buffer *asset)
{
  char asset_hash[T64_SHA256_HEX_SIZE];
  char payload_hash[T64_SHA256_HEX_SIZE];

  if (asset->size != T64_IPL3_SOURCE_BYTES) return false;
  sha256_hex(asset->data, asset->size, asset_hash);
  sha256_hex(asset->data + T64_IPL3_PAYLOAD_OFFSET, T64_IPL3_PAYLOAD_BYTES,
             payload_hash);
  return strcmp(asset_hash, expected_asset_sha256) == 0 &&
         strcmp(payload_hash, expected_payload_sha256) == 0;
}

static bool write_all(int fd, const uint8_t *bytes, size_t size)
{
  size_t offset = 0u;
  while (offset < size) {
    ssize_t count = write(fd, bytes + offset, size - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) return false;
    offset += (size_t)count;
  }
  return true;
}

static bool write_image(const char *output, const byte_buffer *asset,
                        const byte_buffer *payload, uint32_t entry,
                        uint32_t clock, uint32_t crc2, bool cpu_profile)
{
  uint8_t *image = NULL;
  size_t total_size;
  int fd = -1;
  bool ok = false;

  if (!expected_boot_bytes(asset) ||
      !flat_payload_fits(payload->size, cpu_profile) ||
      !entry_range_fits(entry, payload->size, cpu_profile) ||
      clock != T64_ROM_CLOCK_NTSC ||
      crc2 != 0u) {
    return false;
  }
  if (T64_FLAT_PAYLOAD_OFFSET > SIZE_MAX - payload->size) return false;
  total_size = T64_FLAT_PAYLOAD_OFFSET + payload->size;
  if (total_size > (cpu_profile ? T64_CPU_PROFILE_ROM_LIMIT_BYTES
                               : T64_ROM_LIMIT_BYTES) ||
      total_size < T64_FLAT_PAYLOAD_OFFSET) {
    return false;
  }

  image = (uint8_t *)calloc(total_size, 1u);
  if (image == NULL) return false;
  make_header(image, entry, clock, crc2, payload->size);
  (void)memcpy(image + T64_IPL3_PAYLOAD_OFFSET,
               asset->data + T64_IPL3_PAYLOAD_OFFSET, T64_IPL3_PAYLOAD_BYTES);
  (void)memcpy(image + T64_FLAT_PAYLOAD_OFFSET, payload->data, payload->size);

  fd = open(output, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
  if (fd < 0) goto done;
  ok = write_all(fd, image, total_size);
  if (close(fd) != 0) ok = false;
  fd = -1;
  if (!ok) (void)unlink(output);

done:
  if (fd >= 0) {
    (void)close(fd);
    (void)unlink(output);
  }
  free(image);
  return ok;
}

static bool parse_u32(const char *text, uint32_t *out)
{
  char *end = NULL;
  unsigned long long value;

  if (text == NULL || text[0] == '\0' || text[0] == '-' || text[0] == '+') {
    return false;
  }
  errno = 0;
  value = strtoull(text, &end, 0);
  if (errno != 0 || end == text || *end != '\0' || value > UINT32_MAX) {
    return false;
  }
  *out = (uint32_t)value;
  return true;
}

static int pack_command(int argc, char **argv)
{
  const char *boot_path = NULL;
  const char *payload_path = NULL;
  const char *output_path = NULL;
  const char *entry_text = NULL;
  const char *clock_text = NULL;
  const char *crc2_text = NULL;
  uint32_t entry;
  uint32_t clock;
  uint32_t crc2;
  byte_buffer asset = {NULL, 0u};
  byte_buffer payload = {NULL, 0u};
  size_t i;
  bool ok;
  bool cpu_profile = false;
  size_t option_start = 2u;

  if (argc >= 3 && strcmp(argv[2], "--cpu-profile") == 0) {
    cpu_profile = true;
    option_start = 3u;
  }
  if (argc != (cpu_profile ? 15 : 14)) return EXIT_FAILURE;
  for (i = option_start; i < (size_t)argc; i += 2u) {
    if (i + 1u >= (size_t)argc) return EXIT_FAILURE;
    if (strcmp(argv[i], "--boot") == 0 && boot_path == NULL) {
      boot_path = argv[i + 1u];
    } else if (strcmp(argv[i], "--payload") == 0 && payload_path == NULL) {
      payload_path = argv[i + 1u];
    } else if (strcmp(argv[i], "--output") == 0 && output_path == NULL) {
      output_path = argv[i + 1u];
    } else if (strcmp(argv[i], "--entry") == 0 && entry_text == NULL) {
      entry_text = argv[i + 1u];
    } else if (strcmp(argv[i], "--clock") == 0 && clock_text == NULL) {
      clock_text = argv[i + 1u];
    } else if (strcmp(argv[i], "--crc2") == 0 && crc2_text == NULL) {
      crc2_text = argv[i + 1u];
    } else {
      return EXIT_FAILURE;
    }
  }
  if (boot_path == NULL || payload_path == NULL || output_path == NULL ||
      entry_text == NULL || clock_text == NULL ||
      crc2_text == NULL || !parse_u32(entry_text, &entry) ||
      !parse_u32(clock_text, &clock) || !parse_u32(crc2_text, &crc2)) {
    return EXIT_FAILURE;
  }

  if (!read_bounded_file(boot_path, T64_IPL3_SOURCE_BYTES, &asset)) {
    fputs("refused boot asset size or file type\n", stderr);
    return EXIT_FAILURE;
  }
  if (!expected_boot_bytes(&asset)) {
    free(asset.data);
    fputs("refused boot asset identity\n", stderr);
    return EXIT_FAILURE;
  }
  if (!read_bounded_file(payload_path,
                         cpu_profile ? T64_CPU_PROFILE_MAX_FLAT_PAYLOAD_BYTES
                                     : T64_MAX_FLAT_PAYLOAD_BYTES,
                         &payload)) {
    free(asset.data);
    fputs("refused flat payload size or file type\n", stderr);
    return EXIT_FAILURE;
  }
  ok = write_image(output_path, &asset, &payload, entry, clock, crc2,
                   cpu_profile);
  free(asset.data);
  free(payload.data);
  if (!ok) {
    fputs("refused cartridge layout or write failure\n", stderr);
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

static void report_test(unsigned number, const char *name, bool passed)
{
  (void)printf("%s %u - %s\n", passed ? "ok" : "not ok", number, name);
}

static int run_selftest(void)
{
  uint8_t header[T64_HEADER_BYTES];
  unsigned failures = 0u;
  bool passed;

  (void)puts("TAP version 13");
  (void)puts("1..15");

  passed = flat_payload_fits(4096u, false);
  report_test(1u, "flat payload range accepts valid payload", passed);
  failures += passed ? 0u : 1u;

  passed = !flat_payload_fits(0u, false);
  report_test(2u, "flat payload range rejects zero bytes", passed);
  failures += passed ? 0u : 1u;

  passed = flat_payload_fits(T64_MAX_FLAT_PAYLOAD_BYTES, false);
  report_test(3u, "flat payload range accepts exact cartridge limit", passed);
  failures += passed ? 0u : 1u;

  passed = !flat_payload_fits(T64_MAX_FLAT_PAYLOAD_BYTES + 1u, false);
  report_test(4u, "flat payload range rejects cartridge overflow", passed);
  failures += passed ? 0u : 1u;

  passed = !flat_payload_fits(SIZE_MAX, false);
  report_test(5u, "flat payload range rejects size_t overflow", passed);
  failures += passed ? 0u : 1u;

  passed = entry_range_fits(T64_ROM_ENTRY, 4096u, false);
  report_test(6u, "flat payload accepts linked KSEG0 entry", passed);
  failures += passed ? 0u : 1u;

  passed = !entry_range_fits(T64_ROM_ENTRY + 1u, 4096u, false);
  report_test(7u, "flat payload range rejects unaligned entry", passed);
  failures += passed ? 0u : 1u;

  passed = !entry_range_fits(UINT32_C(0x7ffff000), 4096u, false) &&
           !entry_range_fits(UINT32_C(0x9ffffffc), 4096u, false);
  report_test(8u, "flat payload range rejects invalid segment and wrap", passed);
  failures += passed ? 0u : 1u;

  make_header(header, T64_ROM_ENTRY, T64_ROM_CLOCK_NTSC, 0u, 4096u);
  passed = get_be32(header + 0x00u) == T64_N64_MAGIC &&
           get_be32(header + 0x04u) == T64_ROM_CLOCK_NTSC &&
           get_be32(header + 0x08u) == T64_ROM_ENTRY &&
           get_be32(header + 0x10u) == 4096u &&
           get_be32(header + 0x14u) == 0u &&
           memcmp(header + 0x20u, rom_title, sizeof rom_title - 1u) == 0;
  report_test(9u, "header fields use linked entry and checked load size", passed);
  failures += passed ? 0u : 1u;

  passed = flat_payload_fits(T64_CPU_PROFILE_MAX_FLAT_PAYLOAD_BYTES, true);
  report_test(10u, "CPU profile accepts exact four-MiB payload", passed);
  failures += passed ? 0u : 1u;

  passed = !flat_payload_fits(T64_CPU_PROFILE_MAX_FLAT_PAYLOAD_BYTES + 1u,
                              true);
  report_test(11u, "CPU profile rejects payload beyond four MiB", passed);
  failures += passed ? 0u : 1u;

  passed = entry_range_fits(UINT32_C(0x80000000),
                            T64_CPU_PROFILE_MAX_FLAT_PAYLOAD_BYTES, true);
  report_test(12u, "CPU profile accepts exact KSEG0 physical boundary", passed);
  failures += passed ? 0u : 1u;

  passed = !entry_range_fits(UINT32_C(0x80001000),
                             T64_CPU_PROFILE_MAX_FLAT_PAYLOAD_BYTES, true);
  report_test(13u, "CPU profile rejects KSEG0 entry plus payload overflow", passed);
  failures += passed ? 0u : 1u;

  passed = !entry_range_fits(UINT32_C(0xa0000000), 4096u, true) &&
           !entry_range_fits(UINT32_C(0x80000001), 4096u, true);
  report_test(14u, "CPU profile rejects non-KSEG0 or unaligned entry", passed);
  failures += passed ? 0u : 1u;

  passed = T64_FLAT_PAYLOAD_OFFSET + T64_CPU_PROFILE_MAX_FLAT_PAYLOAD_BYTES ==
           T64_CPU_PROFILE_ROM_LIMIT_BYTES;
  report_test(15u, "CPU profile image boundary is payload plus 0x1000", passed);
  failures += passed ? 0u : 1u;

  return failures == 0u ? EXIT_SUCCESS : EXIT_FAILURE;
}

int main(int argc, char **argv)
{
  if (argc == 2 && strcmp(argv[1], "--selftest") == 0) {
    return run_selftest();
  }
  if (argc >= 2 && strcmp(argv[1], "--pack") == 0) {
    return pack_command(argc, argv);
  }
  (void)fputs("usage: t64_rom_pack --selftest | --pack [--cpu-profile] "
              "--boot FILE --payload FILE "
              "--output FILE --entry HEX --clock HEX --crc2 HEX\n",
              stderr);
  return EXIT_FAILURE;
}
