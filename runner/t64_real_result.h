/* Canonical writer for real t64.result v2 guest observations. */
#ifndef T64_REAL_RESULT_H
#define T64_REAL_RESULT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define T64_REAL_RESULT_MAX_BYTES 4096u
#define T64_REAL_RESULT_MAX_MILESTONES 4u
#define T64_REAL_RESULT_HEX_SIZE 65u
#define T64_REAL_RESULT_SOURCE_REVISION_SIZE 41u

typedef struct {
  uint64_t guest_tick;
  char kind[32];
  char pc[17];
} t64_real_milestone;

typedef struct {
  bool prelaunch;
  bool has_input_digest;
  bool has_log_sha256;
  bool has_state_digest;
  char build_id[80];
  char source_revision[T64_REAL_RESULT_SOURCE_REVISION_SIZE];
  char profile_id[64];
  char profile_digest[T64_REAL_RESULT_HEX_SIZE];
  char fixture_id[64];
  char fixture_sha256[T64_REAL_RESULT_HEX_SIZE];
  char ipl3_sha256[T64_REAL_RESULT_HEX_SIZE];
  char ipl3_payload_sha256[T64_REAL_RESULT_HEX_SIZE];
  char input_digest[T64_REAL_RESULT_HEX_SIZE];
  char log_sha256[T64_REAL_RESULT_HEX_SIZE];
  char state_digest[T64_REAL_RESULT_HEX_SIZE];
  char guest_stop_kind[32];
  char outcome[24];
  char detail[32];
  uint64_t bound_tick;
  uint64_t guest_tick;
  uint64_t instruction_count;
  uint64_t log_bytes;
  uint64_t log_bytes_total;
  t64_real_milestone milestones[T64_REAL_RESULT_MAX_MILESTONES];
  size_t milestone_count;
} t64_real_result;

/* Produces one canonical UTF-8 object and one final LF. */
bool t64_real_result_encode(const t64_real_result *result,
                            char *bytes_out, size_t capacity,
                            size_t *length_out);

/* Writes the canonical object completely, retrying interrupted writes. */
bool t64_real_result_write_fd(const t64_real_result *result, int fd);

#endif /* T64_REAL_RESULT_H */
