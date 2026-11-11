/* Immutable, unlinked host-side snapshots for untrusted runner inputs. */
#ifndef T64_VERIFIED_INPUT_H
#define T64_VERIFIED_INPUT_H

#include <stdint.h>

#define T64_VERIFIED_INPUT_SHA256_BYTES 32u
#define T64_VERIFIED_INPUT_SHA256_HEX_SIZE 65u
#define T64_VERIFIED_FIXTURE_MAX_BYTES (1u << 20)
#define T64_VERIFIED_SCHEDULE_MAX_BYTES 65536u
#define T64_VERIFIED_HARNESS_MAX_BYTES (64ull << 20)

typedef enum {
  T64_VERIFIED_INPUT_OK = 0,
  T64_VERIFIED_INPUT_MISSING,
  T64_VERIFIED_INPUT_ERROR
} t64_verified_input_status;

typedef struct {
  int fd;
  uint64_t size;
  uint8_t sha256[T64_VERIFIED_INPUT_SHA256_BYTES];
  char sha256_hex[T64_VERIFIED_INPUT_SHA256_HEX_SIZE];
  uint32_t ownership_tag;
} t64_verified_input;

/*
 * Copy SOURCE_PATH into an exclusive private snapshot, verify its exact raw
 * length and SHA-256, reopen it read-only, unlink it, and rewind it. MAX_BYTES
 * is profile-selected and may never exceed the legacy harness ceiling.
 * OUTPUT is modified only on success. A returned descriptor is owned by the
 * output and must be released with t64_verified_input_release.
 */
t64_verified_input_status t64_verified_input_prepare(
    const char *source_path, uint64_t expected_size,
    const char *expected_sha256_hex, uint64_t max_bytes,
    t64_verified_input *output);

/* Closes only an output carrying this module's ownership tag. */
void t64_verified_input_release(t64_verified_input *input);

#endif /* T64_VERIFIED_INPUT_H */
