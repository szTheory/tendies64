#ifndef T64_RAW_CAPTURE_H
#define T64_RAW_CAPTURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tendies64/t64_machine.h"

#define T64_RAW_CAPTURE_DEFAULT_MAX_BYTES UINT64_C(67108864)
#define T64_RAW_CAPTURE_MAX_BYTES UINT64_C(268435456)

typedef struct t64_raw_capture t64_raw_capture;

typedef struct {
  const char *profile_id;
  const char *profile_sha256;
  uint32_t descriptor_version;
  uint32_t machine_model_version;
  uint32_t state_serialization_version;
  const char *core_build_id;
  const char *runner_build_id;
  const char *source_revision;
  const char *content_sha256;
  uint64_t content_size;
  const char *fixture_id;
  const char *fixture_sha256;
  uint64_t fixture_size;
  const char *input_file_sha256;
  const char *input_digest;
  uint64_t input_size;
} t64_raw_capture_identity;

/* Opens the parent by descriptor, creates a private sibling stage, and probes
 * the host's actual atomic no-replace directory rename before guest execution.
 * Error names are stable, short host-side result details. */
t64_raw_capture *t64_raw_capture_open(
    const char *target_path, uint64_t byte_cap,
    const t64_raw_capture_identity *identity, const char **error_out);

bool t64_raw_capture_write_video(t64_raw_capture *capture,
                                 const t64_video_packet_info *info,
                                 const uint8_t *bytes, size_t length);
bool t64_raw_capture_write_audio(t64_raw_capture *capture,
                                 const t64_audio_packet_info *info,
                                 const uint8_t *bytes, size_t length);

/* Manifest is written and flushed last. A successful return means the only
 * remaining operation is atomic no-replace publication into the target name. */
bool t64_raw_capture_finalize(t64_raw_capture *capture, uint64_t guest_tick,
                              const char *outcome,
                              const char *guest_stop_kind);
void t64_raw_capture_abort(t64_raw_capture *capture);
void t64_raw_capture_destroy(t64_raw_capture *capture);
const char *t64_raw_capture_error(const t64_raw_capture *capture);
uint64_t t64_raw_capture_remaining_bytes(const t64_raw_capture *capture);
size_t t64_raw_capture_audio_count(const t64_raw_capture *capture);

#ifdef T64_REAL_RUNNER_TESTING
/* The test-only interrupted-stage witness may retain its private directory
 * until process exit so the harness can inspect exact pre-finalize bytes. */
void t64_raw_capture_preserve_stage_for_test(t64_raw_capture *capture);
#endif

#endif /* T64_RAW_CAPTURE_H */
