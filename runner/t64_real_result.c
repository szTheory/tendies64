#include "t64_real_result.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "t64_jsonw.h"

static bool write_string(t64_jsonw *writer, const char *value) {
  return value != NULL &&
         t64_jsonw_string(writer, value, strlen(value)) == T64_JSONW_OK;
}

static bool write_key(t64_jsonw *writer, const char *key) {
  return t64_jsonw_key(writer, key, strlen(key)) == T64_JSONW_OK;
}

static bool write_u64_string(t64_jsonw *writer, uint64_t value) {
  char text[20];
  int length = snprintf(text, sizeof text, "%" PRIu64, value);
  return length > 0 && (size_t)length < sizeof text &&
         t64_jsonw_string(writer, text, (size_t)length) == T64_JSONW_OK;
}

static bool write_identity(t64_jsonw *writer, const char *key,
                           const char *value, bool present) {
  if (!write_key(writer, key)) {
    return false;
  }
  if (!present) {
    return t64_jsonw_null(writer) == T64_JSONW_OK;
  }
  return write_string(writer, value);
}

bool t64_real_result_encode(const t64_real_result *result,
                            char *bytes_out, size_t capacity,
                            size_t *length_out) {
  t64_jsonw writer;
  size_t i;

  if (result == NULL || bytes_out == NULL || length_out == NULL ||
      result->milestone_count > T64_REAL_RESULT_MAX_MILESTONES) {
    return false;
  }
  if (result->prelaunch &&
      (result->has_input_digest || result->has_log_sha256 ||
       result->has_state_digest || result->guest_tick != 0u ||
       result->instruction_count != 0u || result->log_bytes != 0u ||
       result->log_bytes_total != 0u || result->milestone_count != 0u)) {
    return false;
  }

  t64_jsonw_init(&writer, bytes_out, capacity);
  if (t64_jsonw_begin_object(&writer) != T64_JSONW_OK ||
      !write_identity(&writer, "build_id", result->build_id,
                      !result->prelaunch) ||
      !write_key(&writer, "counts") ||
      t64_jsonw_begin_object(&writer) != T64_JSONW_OK ||
      !write_key(&writer, "log_bytes") ||
      !write_u64_string(&writer, result->log_bytes) ||
      !write_key(&writer, "log_bytes_total") ||
      !write_u64_string(&writer, result->log_bytes_total) ||
      t64_jsonw_end_object(&writer) != T64_JSONW_OK ||
      !write_key(&writer, "detail") || !write_string(&writer, result->detail) ||
      !write_key(&writer, "evidence_kind") ||
      !write_string(&writer, "guest-execution") ||
      !write_key(&writer, "evidence_tier") ||
      !write_string(&writer, "smoke") ||
      !write_identity(&writer, "fixture_id", result->fixture_id,
                      !result->prelaunch) ||
      !write_identity(&writer, "fixture_sha256", result->fixture_sha256,
                      !result->prelaunch) ||
      !write_key(&writer, "guest_stop") ||
      t64_jsonw_begin_object(&writer) != T64_JSONW_OK ||
      !write_key(&writer, "bound_tick") ||
      !write_u64_string(&writer, result->bound_tick) ||
      !write_key(&writer, "kind") ||
      !write_string(&writer, result->guest_stop_kind) ||
      t64_jsonw_end_object(&writer) != T64_JSONW_OK ||
      !write_key(&writer, "guest_tick") ||
      !write_u64_string(&writer, result->guest_tick) ||
      !write_identity(&writer, "input_digest", result->input_digest,
                      !result->prelaunch && result->has_input_digest) ||
      !write_key(&writer, "instruction_count") ||
      !write_u64_string(&writer, result->instruction_count) ||
      !write_identity(&writer, "ipl3_payload_sha256",
                      result->ipl3_payload_sha256, !result->prelaunch) ||
      !write_identity(&writer, "ipl3_sha256", result->ipl3_sha256,
                      !result->prelaunch) ||
      !write_identity(&writer, "log_sha256", result->log_sha256,
                      !result->prelaunch && result->has_log_sha256) ||
      !write_key(&writer, "milestones") ||
      t64_jsonw_begin_array(&writer) != T64_JSONW_OK) {
    return false;
  }
  for (i = 0u; i < result->milestone_count; i++) {
    const t64_real_milestone *milestone = &result->milestones[i];
    if (t64_jsonw_begin_object(&writer) != T64_JSONW_OK ||
        !write_key(&writer, "guest_tick") ||
        !write_u64_string(&writer, milestone->guest_tick) ||
        !write_key(&writer, "kind") || !write_string(&writer, milestone->kind) ||
        !write_key(&writer, "pc") || !write_string(&writer, milestone->pc) ||
        t64_jsonw_end_object(&writer) != T64_JSONW_OK) {
      return false;
    }
  }
  if (t64_jsonw_end_array(&writer) != T64_JSONW_OK ||
      !write_key(&writer, "outcome") || !write_string(&writer, result->outcome) ||
      !write_identity(&writer, "profile_digest", result->profile_digest,
                      !result->prelaunch) ||
      !write_identity(&writer, "profile_id", result->profile_id,
                      !result->prelaunch) ||
      !write_key(&writer, "schema") ||
      !write_string(&writer, "t64.result") ||
      !write_key(&writer, "schema_version") ||
      t64_jsonw_int(&writer, 2) != T64_JSONW_OK ||
      !write_identity(&writer, "source_revision", result->source_revision,
                      !result->prelaunch) ||
      !write_identity(&writer, "state_digest", result->state_digest,
                      !result->prelaunch && result->has_state_digest) ||
      t64_jsonw_end_object(&writer) != T64_JSONW_OK ||
      t64_jsonw_finish(&writer, length_out) != T64_JSONW_OK) {
    return false;
  }
  return true;
}

bool t64_real_result_write_fd(const t64_real_result *result, int fd) {
  char bytes[T64_REAL_RESULT_MAX_BYTES];
  size_t length = 0u;
  size_t offset = 0u;

  if (fd < 0 || !t64_real_result_encode(result, bytes, sizeof bytes, &length)) {
    return false;
  }
  while (offset < length) {
    ssize_t count = write(fd, bytes + offset, length - offset);
    if (count < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    if (count == 0) {
      return false;
    }
    offset += (size_t)count;
  }
  return true;
}
