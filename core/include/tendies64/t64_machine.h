/*
 * Provisional deterministic machine seam. The public surface is not a frozen
 * ABI. Host callbacks own allocation and release; core execution never calls
 * back into the host.
 */
#ifndef TENDIES64_T64_MACHINE_H
#define TENDIES64_T64_MACHINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define T64_DIGEST_SIZE 32u
#define T64_PROFILE_TEXT_CAPACITY 64u
#define T64_PROFILE_SERIALIZED_MAX_BYTES 2048u
#define T64_PROFILE_SERIALIZATION_VERSION UINT32_C(1)

#define T64_STATUS_OK UINT32_C(0)
#define T64_STATUS_INVALID_ARGUMENT UINT32_C(1)
#define T64_STATUS_UNKNOWN_PROFILE UINT32_C(2)
#define T64_STATUS_ALLOCATION_FAILED UINT32_C(3)
#define T64_STATUS_BAD_STORAGE UINT32_C(4)
#define T64_STATUS_INVALID_ROM UINT32_C(5)
#define T64_STATUS_UNSUPPORTED_ROM_ORDER UINT32_C(6)
#define T64_STATUS_NOT_READY UINT32_C(7)
#define T64_STATUS_TIME_REVERSED UINT32_C(8)
#define T64_STATUS_BUFFER_TOO_SMALL UINT32_C(9)

#define T64_TV_NTSC UINT32_C(1)
#define T64_TV_PAL UINT32_C(2)
#define T64_REGION_US UINT32_C(1)
#define T64_REGION_EUROPE UINT32_C(2)
#define T64_MEDIA_CARTRIDGE UINT32_C(1)
#define T64_SAVE_NONE UINT32_C(0)

#define T64_STOP_GUEST_TICK UINT32_C(1)
#define T64_STOP_REASON_BOUND UINT32_C(1)

typedef uint32_t t64_status;

typedef struct t64_machine t64_machine;

/* Build identity is separate from machine/profile/state identity. */
const char *t64_build_identity(void);
const char *t64_source_revision(void);

typedef struct {
  void *data;
  size_t capacity;
  size_t alignment;
} t64_host_storage;

typedef bool (*t64_host_allocate_fn)(void *context, size_t size,
                                    size_t alignment,
                                    t64_host_storage *storage_out);
typedef void (*t64_host_release_fn)(void *context, t64_host_storage storage);

typedef struct {
  t64_host_allocate_fn allocate;
  t64_host_release_fn release;
  void *context;
} t64_host_allocator;

typedef struct {
  char profile_id[T64_PROFILE_TEXT_CAPACITY];
  char console_target[T64_PROFILE_TEXT_CAPACITY];
  char console_qualification[T64_PROFILE_TEXT_CAPACITY];
  uint32_t tv_standard;
  uint32_t region;
  uint32_t media_kind;
  uint32_t rdram_bytes;
  uint32_t expansion_bytes;
  uint32_t peripheral_flags;
  uint32_t save_kind;
  char boot_route[T64_PROFILE_TEXT_CAPACITY];
  char boot_status[T64_PROFILE_TEXT_CAPACITY];
  char guest_tick_unit[T64_PROFILE_TEXT_CAPACITY];
  uint32_t cpu_rsp_ratio_numerator;
  uint32_t cpu_rsp_ratio_denominator;
  uint32_t synchronization_quantum;
  char timing_tier[T64_PROFILE_TEXT_CAPACITY];
  char scheduler_version[T64_PROFILE_TEXT_CAPACITY];
  char pi_model_version[T64_PROFILE_TEXT_CAPACITY];
  char sp_model_version[T64_PROFILE_TEXT_CAPACITY];
  uint32_t rom_min_bytes;
  uint32_t rom_max_bytes;
  uint32_t event_queue_capacity;
  uint32_t sequence_width_bits;
  uint32_t advance_event_budget;
  uint32_t pi_duration_base_ticks;
  uint32_t pi_duration_bytes_divisor;
  uint32_t sp_duration_base_rsp_cycles;
  uint32_t sp_duration_bytes_divisor;
  char rom_order_policy[T64_PROFILE_TEXT_CAPACITY];
  char event_order_policy[T64_PROFILE_TEXT_CAPACITY];
  char queue_exhaustion_policy[T64_PROFILE_TEXT_CAPACITY];
  char sequence_exhaustion_policy[T64_PROFILE_TEXT_CAPACITY];
  char advance_budget_policy[T64_PROFILE_TEXT_CAPACITY];
  char guest_time_bound_policy[T64_PROFILE_TEXT_CAPACITY];
  char halt_precedence_policy[T64_PROFILE_TEXT_CAPACITY];
  char pi_register_policy[T64_PROFILE_TEXT_CAPACITY];
  char sp_register_policy[T64_PROFILE_TEXT_CAPACITY];
  char device_error_policy[T64_PROFILE_TEXT_CAPACITY];
  char dma_cancel_policy[T64_PROFILE_TEXT_CAPACITY];
  char dma_visibility_policy[T64_PROFILE_TEXT_CAPACITY];
  uint32_t descriptor_version;
  uint32_t machine_model_version;
  uint32_t state_serialization_version;
} t64_profile_descriptor;

typedef struct {
  uint32_t kind;
  uint64_t value;
} t64_stop_request;

typedef struct {
  uint32_t reason;
  uint64_t tick;
} t64_stop_result;

/* The returned descriptor is immutable and remains valid for program life. */
t64_status t64_profile_lookup(const char *profile_id,
                              const t64_profile_descriptor **profile_out);
t64_status t64_profile_hash(const t64_profile_descriptor *profile,
                            uint8_t digest_out[T64_DIGEST_SIZE]);
/* On BUFFER_TOO_SMALL, length_out receives the required size; bytes stay intact. */
t64_status t64_profile_serialize(const t64_profile_descriptor *profile,
                                 uint8_t *bytes_out, size_t capacity,
                                 size_t *length_out);

/* Create/destroy and admission are the only operations that call the host. */
t64_status t64_machine_create(const t64_host_allocator *allocator,
                              const t64_profile_descriptor *profile,
                              t64_machine **machine_out);
void t64_machine_destroy(t64_machine *machine);
t64_status t64_machine_admit_rom(t64_machine *machine, const uint8_t *bytes,
                                 size_t length);
t64_status t64_machine_advance(t64_machine *machine,
                               const t64_stop_request *request,
                               t64_stop_result *result_out);
t64_status t64_machine_content_digest(
    const t64_machine *machine, uint8_t digest_out[T64_DIGEST_SIZE]);
/* The returned descriptor is borrowed and valid until machine destruction. */
t64_status t64_machine_profile(
    const t64_machine *machine, const t64_profile_descriptor **profile_out);
t64_status t64_machine_digest(const t64_machine *machine,
                              uint8_t digest_out[T64_DIGEST_SIZE]);

#endif /* TENDIES64_T64_MACHINE_H */
