#include "tendies64/t64_machine.h"

/* The RED commit keeps the public seam linkable before behavior is added. */
t64_status t64_profile_lookup(const char *profile_id,
                              const t64_profile_descriptor **profile_out) {
  (void)profile_id;
  (void)profile_out;
  return T64_STATUS_UNKNOWN_PROFILE;
}

t64_status t64_profile_hash(const t64_profile_descriptor *profile,
                            uint8_t digest_out[T64_DIGEST_SIZE]) {
  (void)profile;
  (void)digest_out;
  return T64_STATUS_INVALID_ARGUMENT;
}

t64_status t64_machine_create(const t64_host_allocator *allocator,
                              const t64_profile_descriptor *profile,
                              t64_machine **machine_out) {
  (void)allocator;
  (void)profile;
  if (machine_out != NULL) {
    *machine_out = NULL;
  }
  return T64_STATUS_INVALID_ARGUMENT;
}

void t64_machine_destroy(t64_machine *machine) { (void)machine; }

t64_status t64_machine_admit_rom(t64_machine *machine, const uint8_t *bytes,
                                 size_t length) {
  (void)machine;
  (void)bytes;
  (void)length;
  return T64_STATUS_NOT_READY;
}

t64_status t64_machine_advance(t64_machine *machine,
                               const t64_stop_request *request,
                               t64_stop_result *result_out) {
  (void)machine;
  (void)request;
  (void)result_out;
  return T64_STATUS_NOT_READY;
}

t64_status t64_machine_content_digest(
    const t64_machine *machine, uint8_t digest_out[T64_DIGEST_SIZE]) {
  (void)machine;
  (void)digest_out;
  return T64_STATUS_NOT_READY;
}

t64_status t64_machine_digest(const t64_machine *machine,
                              uint8_t digest_out[T64_DIGEST_SIZE]) {
  (void)machine;
  (void)digest_out;
  return T64_STATUS_NOT_READY;
}
