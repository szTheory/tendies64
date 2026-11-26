/*
 * Shared helpers of the public-API unit tests (Phase 11, plan 11-20, review
 * item S-09). test_api_poison.c, test_api_lifecycle.c and test_api_struct_size.c
 * each carried their own copy of these; a public struct that gained a member had
 * to be poisoned in every copy, or an "untouched" check became vacuous for the
 * new member. They now share one definition:
 *
 *   the counting allocator   remembers every live block and its size, so a test
 *                            can state "allocation count equals release count,
 *                            zero outstanding" exactly; an optional hook runs
 *                            first in every callback (the reentrancy probe of
 *                            test_api_poison.c uses it)
 *   the fixture readers      read_file (a whole file, at most capacity bytes,
 *                            not empty) and read_fixture (a file of the boot
 *                            fixtures directory named by T64_BOOT_FIXTURES_DIR)
 *   the poison helpers       one per in/out record of the public header: the
 *                            INIT form first, so a leading struct_size stays
 *                            valid, then a non-zero value in every other member
 *                            by name
 *
 * The header includes only the public header and system headers, so the tests
 * that use it still see exactly what an installed consumer sees. Every function
 * is static inline: a test that uses only some of them compiles clean.
 */
#ifndef T64_API_TEST_SUPPORT_H
#define T64_API_TEST_SUPPORT_H

#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tendies64/t64_machine.h"

/* ---- the counting allocator -------------------------------------------------- */

#define API_MAX_LIVE 16u

typedef struct {
  size_t allocations;
  size_t releases;
  size_t unknown_releases;
  size_t live_count;
  void *live[API_MAX_LIVE];
  size_t live_size[API_MAX_LIVE];
  size_t last_release_size;
  /* Optional: called first in every callback once the arguments are accepted. */
  void (*hook)(void *hook_context, bool is_allocate, size_t size);
  void *hook_context;
} counting_allocator;

static inline bool counting_allocate(void *context, size_t size, size_t alignment,
                                     t64_host_storage *storage_out) {
  counting_allocator *state = (counting_allocator *)context;
  void *memory;

  if (state == NULL || storage_out == NULL || size == 0u || alignment == 0u ||
      (alignment & (alignment - 1u)) != 0u || state->live_count >= API_MAX_LIVE) {
    return false;
  }
  if (state->hook != NULL) {
    state->hook(state->hook_context, true, size);
  }
  if (alignment <= alignof(max_align_t)) {
    memory = malloc(size);
  } else {
    memory = aligned_alloc(alignment, (size + alignment - 1u) / alignment * alignment);
  }
  if (memory == NULL || (uintptr_t)memory % alignment != 0u) {
    free(memory);
    return false;
  }
  state->allocations++;
  state->live[state->live_count] = memory;
  state->live_size[state->live_count] = size;
  state->live_count++;
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = alignment;
  return true;
}

static inline void counting_release(void *context, t64_host_storage storage) {
  counting_allocator *state = (counting_allocator *)context;
  size_t index;

  if (state == NULL) {
    return;
  }
  if (state->hook != NULL) {
    state->hook(state->hook_context, false, storage.capacity);
  }
  state->releases++;
  for (index = 0u; index < state->live_count; index++) {
    if (state->live[index] == storage.data) {
      state->last_release_size = state->live_size[index];
      state->live[index] = state->live[state->live_count - 1u];
      state->live_size[index] = state->live_size[state->live_count - 1u];
      state->live_count--;
      free(storage.data);
      return;
    }
  }
  state->unknown_releases++;
}

/* ---- the fixture readers ------------------------------------------------------- */

/* Read a whole file of at most capacity bytes; false on any error or an empty
 * file or one that is longer than capacity. */
static inline bool read_file(const char *path, uint8_t *bytes, size_t capacity,
                             size_t *length_out) {
  FILE *file;
  size_t length;
  int extra;

  file = fopen(path, "rb");
  if (file == NULL) return false;
  length = fread(bytes, 1u, capacity, file);
  extra = fgetc(file);
  if (ferror(file) != 0 || extra != EOF || length == 0u) {
    (void)fclose(file);
    return false;
  }
  if (fclose(file) != 0) return false;
  *length_out = length;
  return true;
}

/* A file of the directory named by T64_BOOT_FIXTURES_DIR. */
static inline bool read_fixture(const char *name, uint8_t *bytes, size_t capacity,
                                size_t *length_out) {
  const char *directory = getenv("T64_BOOT_FIXTURES_DIR");
  char path[512];
  int written;

  if (directory == NULL || directory[0] == '\0') return false;
  written = snprintf(path, sizeof path, "%s/%s", directory, name);
  if (written < 0 || (size_t)written >= sizeof path) return false;
  return read_file(path, bytes, capacity, length_out);
}

/* ---- sentinels for the in/out records ------------------------------------------
 * A poisoned record is the INIT form of its type first, so a leading
 * struct_size stays valid, and then a non-zero value in every other member by
 * name (the values a byte fill of 0xA5 gave). A byte fill would also clobber a
 * leading struct_size, and a plain INIT assignment would let a callee that
 * writes zeros, or leaves a member alone, pass the checks of the fields it must
 * overwrite. */
#define POISON_U32 UINT32_C(0xa5a5a5a5)
#define POISON_U64 UINT64_C(0xa5a5a5a5a5a5a5a5)
#define POISON_SIZE ((size_t)POISON_U64)

static inline void poison_stop_result(t64_stop_result *value) {
  *value = (t64_stop_result)T64_STOP_RESULT_INIT_WITH(POISON_U32, POISON_U64);
}

static inline void poison_cpu_stop_detail(t64_cpu_stop_detail *value) {
  *value = (t64_cpu_stop_detail)T64_CPU_STOP_DETAIL_INIT;
  value->retired_instructions = POISON_U64;
  value->pc = POISON_U32;
  value->opcode = POISON_U32;
}

static inline void poison_drain_result(t64_output_drain_result *value) {
  *value = (t64_output_drain_result)T64_OUTPUT_DRAIN_RESULT_INIT;
  value->copied_bytes = POISON_SIZE;
  value->required_bytes = POISON_SIZE;
  value->total_attempted_bytes = POISON_U64;
  (void)memset(value->captured_sha256, 0xa5, sizeof value->captured_sha256);
  value->lost_output = true;
  value->total_attempted_bytes_exact = true;
}

static inline void poison_import_report(t64_state_import_report *value) {
  *value = (t64_state_import_report)T64_STATE_IMPORT_REPORT_INIT;
  value->status = POISON_U32;
  value->reason = POISON_U32;
  value->section_id = POISON_U32;
  value->rule = POISON_U32;
  value->position = POISON_U64;
}

static inline void poison_save_info(t64_save_info *value) {
  *value = (t64_save_info)T64_SAVE_INFO_INIT;
  value->kind = POISON_U32;
  value->size_bytes = POISON_U32;
  value->flags = POISON_U32;
  value->write_generation = POISON_U64;
}

static inline void poison_output_status(t64_output_status *value) {
  *value = (t64_output_status)T64_OUTPUT_STATUS_INIT;
  value->video_queued = POISON_U32;
  value->video_free = POISON_U32;
  value->reserved0 = POISON_U32;
  value->video_next_bytes = POISON_U64;
  value->audio_queued = POISON_U32;
  value->audio_free = POISON_U32;
  value->audio_next_bytes = POISON_U64;
  value->text_pending_bytes = POISON_U64;
}

static inline void poison_audio_info(t64_audio_packet_info *value) {
  *value = (t64_audio_packet_info)T64_AUDIO_PACKET_INFO_INIT;
  value->sequence = POISON_U64;
  value->first_sample_index = POISON_U64;
  value->first_tick = POISON_U64;
  value->last_tick = POISON_U64;
  value->frame_count = POISON_U32;
  value->rate_numerator = POISON_U64;
  value->rate_denominator = POISON_U64;
  value->channels = POISON_U32;
  value->bits_per_sample = POISON_U32;
  value->encoding = POISON_U32;
  value->byte_length = POISON_SIZE;
  (void)memset(value->profile_id, 0xa5, sizeof value->profile_id);
  (void)memset(value->build_id, 0xa5, sizeof value->build_id);
}

static inline void poison_video_info(t64_video_packet_info *value) {
  *value = (t64_video_packet_info)T64_VIDEO_PACKET_INFO_INIT;
  value->width = POISON_U32;
  value->height = POISON_U32;
  value->stride_bytes = POISON_U32;
  value->source_address = POISON_U32;
  value->source_format = POISON_U32;
  value->output_encoding = POISON_U32;
  value->field = POISON_U32;
  value->parity = POISON_U32;
  value->control = POISON_U32;
  value->burst = POISON_U32;
  value->h_sync = POISON_U32;
  value->v_sync = POISON_U32;
  value->h_start = POISON_U32;
  value->h_end = POISON_U32;
  value->v_start = POISON_U32;
  value->v_end = POISON_U32;
  value->source_width = POISON_U32;
  value->source_pitch_bytes = POISON_U32;
  value->source_x_offset = POISON_U32;
  value->source_y_offset = POISON_U32;
  value->x_scale = POISON_U32;
  value->y_scale = POISON_U32;
  value->filter = POISON_U32;
  value->start_tick = POISON_U64;
  value->end_tick = POISON_U64;
  value->sequence = POISON_U64;
  value->commands_completed = POISON_U64;
  value->fullsync_completed = POISON_U64;
  value->last_command_word = POISON_U64;
  value->last_command_byte = POISON_U32;
  value->last_opcode_field = POISON_U32;
  value->source_bytes = POISON_SIZE;
  value->packet_bytes = POISON_SIZE;
  (void)memset(value->profile_id, 0xa5, sizeof value->profile_id);
  (void)memset(value->build_id, 0xa5, sizeof value->build_id);
  (void)memset(value->vi_clock_policy, 0xa5, sizeof value->vi_clock_policy);
  (void)memset(value->vi_output_policy, 0xa5, sizeof value->vi_output_policy);
}

#endif /* T64_API_TEST_SUPPORT_H */
