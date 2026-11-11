/* Immutable input descriptor preparation; implementation follows in GREEN. */
#include "t64_verified_input.h"

#include <string.h>

void t64_verified_input_release(t64_verified_input *input) {
  if (input != NULL && input->ownership_tag == 0x74363469u) {
    input->ownership_tag = 0u;
    input->fd = -1;
  }
}

t64_verified_input_status t64_verified_input_prepare(
    const char *source_path, uint64_t expected_size,
    const char *expected_sha256_hex, uint64_t max_bytes,
    t64_verified_input *output) {
  (void)source_path;
  (void)expected_size;
  (void)expected_sha256_hex;
  (void)max_bytes;
  (void)output;
  return T64_VERIFIED_INPUT_ERROR;
}
