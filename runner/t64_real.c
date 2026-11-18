#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "t64_input.h"
#include "t64_audio_observation.h"
#include "t64_raw_capture.h"
#include "t64_real_fixture.h"
#include "t64_real_result.h"
#include "t64_sha256.h"
#include "t64_video_observation.h"
#include "tendies64/t64_machine.h"

#define T64_REAL_FIXTURE_MAX_BYTES (1u << 20)
#define T64_REAL_OUTPUT_CAPACITY 4096u
#define T64_REAL_EXPECTED_LOG_BYTES 30u
#define T64_REAL_TRACE_STEP UINT64_C(16)
#define T64_REAL_TEST_MAX_ADVANCE_STEP UINT64_C(32)
#define T64_REAL_MAX_RESOURCE_YIELDS 64u
#define T64_REAL_VIDEO_MAX_BYTES (320u * 240u * 4u)
_Static_assert(sizeof(t64_video_observation) <= 4096u,
               "VI observation must remain schema-bounded");
_Static_assert(sizeof(t64_audio_observation) <= 4096u,
               "audio observation must remain schema-bounded");
#define T64_REAL_EXPECTED_ASSET_SHA256 \
  "f522db2e31a701f82597f399e76d55c9487760d015aff9d90176b463ae39a068"
#define T64_REAL_EXPECTED_PAYLOAD_SHA256 \
  "f716a90b1d36ec4253d096790791256d3543455e54473a755ff8c3d1199b15d5"
#define T64_REAL_AUDIO_PROFILE_ID \
  "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1"
#define T64_REAL_EXPECTED_FIXTURE_SHA256 \
  "fa276cc175a4e764b6fd5413193c44b51f38fd8135e33e400032827e02f27c7c"
#define T64_REAL_EXPECTED_LOG_SHA256 \
  "86594505bea53c9843f9eaf07e4cf0121cbe0055670324674fd8385c88ffd18e"
#define T64_REAL_HELLO_PC UINT32_C(0x80000478)
#define T64_REAL_HELLO_ENTRY_PC UINT32_C(0x80000400)
#define T64_REAL_BREAK_OPCODE UINT32_C(0x0000000d)
#ifndef T64_RUNNER_BUILD_ID
#define T64_RUNNER_BUILD_ID "t64-runner-v1-unavailable"
#endif

#ifdef T64_REAL_RUNNER_TESTING
static const char *g_test_fault;

static bool test_fault_is(const char *name) {
  return g_test_fault != NULL && strcmp(g_test_fault, name) == 0;
}
#endif

static const uint8_t expected_log[T64_REAL_EXPECTED_LOG_BYTES + 1u] =
    "Tendies64 hello via open IPL3\n";

typedef struct {
  uint8_t *bytes;
  size_t length;
  uint8_t sha256[T64_SHA256_DIGEST_SIZE];
  char sha256_hex[T64_SHA256_HEX_SIZE];
} owned_bytes;

typedef struct {
  t64_real_result *result;
  uint64_t retired;
  uint32_t last_cpu_pc;
  bool have_last_cpu_pc;
  bool saw_ipl3_entry;
  bool saw_loader_dma;
  bool saw_hello_entry;
  bool saw_completion;
  uint64_t trace_records;
  uint64_t completion_tick;
  uint32_t completion_pc;
  uint32_t completion_opcode;
  bool have_video_packet;
  t64_video_observation video;
  t64_audio_observation audio;
} trace_observation;

static bool parse_u64(const char *text, uint64_t *value_out) {
  uint64_t value = 0u;
  size_t i;

  if (text == NULL || text[0] == '\0' ||
      (text[0] == '0' && text[1] != '\0')) {
    return false;
  }
  for (i = 0u; text[i] != '\0'; i++) {
    unsigned digit;
    if (text[i] < '0' || text[i] > '9') {
      return false;
    }
    digit = (unsigned)(text[i] - '0');
    if (value > (UINT64_MAX - digit) / UINT64_C(10)) {
      return false;
    }
    value = value * UINT64_C(10) + digit;
  }
  *value_out = value;
  return true;
}

static bool parse_fd(const char *text, int *fd_out) {
  uint64_t value;
  if (!parse_u64(text, &value) || value > (uint64_t)INT_MAX ||
      value <= (uint64_t)STDERR_FILENO) {
    return false;
  }
  *fd_out = (int)value;
  return true;
}

static bool read_snapshot(int fd, size_t max_bytes, size_t exact_size,
                          owned_bytes *output) {
  struct stat st;
  int file_flags;
  off_t offset;
  size_t total = 0u;
  t64_sha256_ctx hash;
  uint8_t extra;

  if (fd < 0 || output == NULL || fstat(fd, &st) != 0 ||
      !S_ISREG(st.st_mode) || st.st_size < 0 ||
      (uint64_t)st.st_size > (uint64_t)max_bytes ||
      (exact_size != 0u && (size_t)st.st_size != exact_size) ||
      st.st_nlink != 0) {
    return false;
  }
  file_flags = fcntl(fd, F_GETFL);
  offset = lseek(fd, 0, SEEK_CUR);
  if (file_flags < 0 || (file_flags & O_ACCMODE) != O_RDONLY || offset != 0) {
    return false;
  }
  output->length = (size_t)st.st_size;
  if (output->length == 0u) {
    return false;
  }
  output->bytes = (uint8_t *)malloc(output->length);
  if (output->bytes == NULL) {
    return false;
  }
  t64_sha256_init(&hash);
  while (total < output->length) {
    ssize_t count = read(fd, output->bytes + total, output->length - total);
    if (count < 0 && errno == EINTR) {
      continue;
    }
    if (count <= 0) {
      free(output->bytes);
      output->bytes = NULL;
      return false;
    }
    t64_sha256_update(&hash, output->bytes + total, (size_t)count);
    total += (size_t)count;
  }
  for (;;) {
    ssize_t count = read(fd, &extra, 1u);
    if (count < 0 && errno == EINTR) {
      continue;
    }
    if (count != 0) {
      free(output->bytes);
      output->bytes = NULL;
      return false;
    }
    break;
  }
  t64_sha256_final(&hash, output->sha256);
  t64_sha256_hex(output->sha256, output->sha256_hex);
  return true;
}

static bool digest_hex(const uint8_t *bytes, size_t length,
                       char output[T64_SHA256_HEX_SIZE]) {
  uint8_t digest[T64_SHA256_DIGEST_SIZE];
  t64_sha256_ctx hash;
  if (bytes == NULL || output == NULL) {
    return false;
  }
  t64_sha256_init(&hash);
  t64_sha256_update(&hash, bytes, length);
  t64_sha256_final(&hash, digest);
  t64_sha256_hex(digest, output);
  return true;
}

static bool bytes_equal_hex(const uint8_t *bytes, size_t length,
                            const char *expected) {
  char actual[T64_SHA256_HEX_SIZE];
  return digest_hex(bytes, length, actual) &&
         strcmp(actual, expected) == 0;
}

static bool allocator_allocate(void *context, size_t size, size_t alignment,
                               t64_host_storage *storage_out) {
  void *data = NULL;
  size_t host_alignment = alignment;
  (void)context;
  if (alignment < sizeof(void *)) {
    host_alignment = sizeof(void *);
  }
  if (storage_out == NULL || size == 0u || alignment == 0u ||
      posix_memalign(&data, host_alignment, size) != 0) {
    return false;
  }
  storage_out->data = data;
  storage_out->capacity = size;
  storage_out->alignment = host_alignment;
  return true;
}

static void allocator_release(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

static void format_pc(uint32_t pc, char output[17]) {
  uint64_t address = (uint64_t)(int64_t)(int32_t)pc;
  (void)snprintf(output, 17u, "%016" PRIx64, address);
}

static bool add_milestone(t64_real_result *result, const char *kind,
                          uint64_t tick, uint32_t pc) {
  t64_real_milestone *milestone;
  size_t length;
  if (result->milestone_count >= T64_REAL_RESULT_MAX_MILESTONES) {
    return false;
  }
  milestone = &result->milestones[result->milestone_count];
  length = strlen(kind);
  if (length >= sizeof milestone->kind) {
    return false;
  }
  (void)memcpy(milestone->kind, kind, length + 1u);
  milestone->guest_tick = tick;
  format_pc(pc, milestone->pc);
  result->milestone_count++;
  return true;
}

static bool observe_trace_record(trace_observation *observation,
                                 const t64_trace_record *record) {
  uint32_t pc = (uint32_t)(record->value >> 32);
  uint32_t opcode = (uint32_t)record->value;

  observation->trace_records++;
  if (observation->audio.trace_records != UINT64_MAX) {
    observation->audio.trace_records++;
  } else {
    observation->audio.trace_overflow = 1u;
  }

  if (record->device == T64_TRACE_DEVICE_BUS &&
      record->kind == T64_TRACE_KIND_BUS_WRITE32) {
    t64_audio_observation *audio = &observation->audio;
    uint32_t address = (uint32_t)(record->value >> 32);
    const bool pcm_write = address >= UINT32_C(0x00003400) &&
                           address < UINT32_C(0x00003420);
    const bool guest_mi_snapshot = address == UINT32_C(0x00003210);
    const bool ai_write =
        address == UINT32_C(0x04500000) ||
        address == UINT32_C(0x04500004) ||
        address == UINT32_C(0x04500008) ||
        address == UINT32_C(0x04500010) ||
        address == UINT32_C(0x04500014);
    if (guest_mi_snapshot) {
      audio->guest_mi_snapshot_seen = 1u;
      audio->guest_mi_snapshot = (uint32_t)record->value;
    }
    if (pcm_write || ai_write || guest_mi_snapshot) {
      if (audio->bus_write_count >= T64_AUDIO_OBSERVATION_MAX_BUS_WRITES) {
        audio->trace_overflow = 1u;
      } else {
        t64_audio_bus_write *write =
            &audio->bus_writes[audio->bus_write_count++];
        write->tick = record->tick;
        write->address = address;
        write->value = (uint32_t)record->value;
        write->kind = record->kind;
      }
    }
  }
  if (record->device == T64_TRACE_DEVICE_BUS &&
      record->kind == T64_TRACE_KIND_BUS_READ32 &&
      (uint32_t)(record->value >> 32) == UINT32_C(0x04300008) &&
      ((uint32_t)record->value & UINT32_C(0x04)) != 0u) {
    observation->audio.mi_ai_read_seen = 1u;
    observation->audio.mi_ai_read_tick = record->tick;
  }
  if (record->device == T64_TRACE_DEVICE_MACHINE &&
      observation->saw_hello_entry &&
      record->kind == T64_TRACE_KIND_CPU_EXCEPTION &&
      observation->audio.guest_first_exception_seen == 0u) {
    t64_audio_observation *audio = &observation->audio;
    audio->guest_first_exception_seen = 1u;
    audio->guest_first_exception_tick = record->tick;
    audio->guest_first_exception_epc = record->value;
    audio->guest_first_exception_preceding_tick =
        audio->guest_last_cpu_tick;
    audio->guest_first_exception_preceding_pc = audio->guest_last_cpu_pc;
  } else if (record->device == T64_TRACE_DEVICE_MACHINE &&
             observation->audio.guest_first_exception_seen != 0u &&
             record->tick == observation->audio.guest_first_exception_tick &&
             record->kind == T64_TRACE_KIND_CPU_EXCEPTION_STATE) {
    observation->audio.guest_first_exception_state = record->value;
  } else if (record->device == T64_TRACE_DEVICE_MACHINE &&
             observation->audio.guest_first_exception_seen != 0u &&
             record->tick == observation->audio.guest_first_exception_tick &&
             record->kind == T64_TRACE_KIND_CPU_EXCEPTION_MI) {
    observation->audio.guest_first_exception_mi = record->value;
  }

  if (record->device == T64_TRACE_DEVICE_MACHINE &&
      record->kind == T64_TRACE_KIND_CPU_ENTRY &&
      (uint32_t)record->value == UINT32_C(0xa4000040) &&
      !observation->saw_ipl3_entry) {
    if (!add_milestone(observation->result, "ipl3-entry", record->tick,
                       UINT32_C(0xa4000040))) {
      return false;
    }
    observation->saw_ipl3_entry = true;
  }
  if (record->device == T64_TRACE_DEVICE_MACHINE &&
      record->kind == T64_TRACE_KIND_CPU_ENTRY &&
      (uint32_t)record->value == T64_REAL_HELLO_ENTRY_PC &&
      !observation->saw_hello_entry) {
    if (!add_milestone(observation->result, "hello-entry", record->tick,
                       T64_REAL_HELLO_ENTRY_PC)) {
      return false;
    }
    observation->saw_hello_entry = true;
  }
  if (record->device == T64_TRACE_DEVICE_MACHINE &&
      record->kind == T64_TRACE_KIND_CPU_RETIREMENT) {
    observation->retired++;
    observation->last_cpu_pc = pc;
    observation->have_last_cpu_pc = true;
    observation->audio.guest_last_cpu_tick = record->tick;
    observation->audio.guest_last_cpu_pc = pc;
    if (observation->saw_hello_entry &&
        (pc < UINT32_C(0x80000400) || pc >= UINT32_C(0x80000600)) &&
        observation->audio.guest_first_escape_pc == 0u) {
      observation->audio.guest_first_escape_tick = record->tick;
      observation->audio.guest_first_escape_pc = pc;
    }
    if ((opcode >> 26) == UINT32_C(0x2b) &&
        (pc == UINT32_C(0x8000045c) ||
         (pc >= UINT32_C(0x8000047c) &&
          pc <= UINT32_C(0x800004ac)))) {
      t64_audio_observation *audio = &observation->audio;
      if (audio->sw_retirement_count >=
          T64_AUDIO_OBSERVATION_MAX_SW_RETIREMENTS) {
        audio->trace_overflow = 1u;
      } else {
        t64_audio_sw_retirement *retirement =
            &audio->sw_retirements[audio->sw_retirement_count++];
        retirement->tick = record->tick;
        retirement->pc = pc;
        retirement->opcode = opcode;
      }
    }
    if (pc == UINT32_C(0xa4000040) && !observation->saw_ipl3_entry) {
      if (!add_milestone(observation->result, "ipl3-entry", UINT64_C(0),
                         pc)) {
        return false;
      }
      observation->saw_ipl3_entry = true;
    }
    if (pc == T64_REAL_HELLO_ENTRY_PC && !observation->saw_hello_entry) {
      if (!add_milestone(observation->result, "hello-entry", record->tick,
                         pc)) {
        return false;
      }
      observation->saw_hello_entry = true;
    }
  }
  if (record->device == T64_TRACE_DEVICE_SP &&
      record->kind == T64_TRACE_KIND_SP_COMPLETE &&
      !observation->saw_loader_dma && observation->saw_ipl3_entry &&
      observation->have_last_cpu_pc) {
    if (!add_milestone(observation->result, "loader-dma", record->tick,
                       observation->last_cpu_pc)) {
      return false;
    }
    observation->saw_loader_dma = true;
  }
  if (record->device == T64_TRACE_DEVICE_PI &&
      record->kind == T64_TRACE_KIND_PI_COMPLETE &&
      !observation->saw_loader_dma && observation->saw_ipl3_entry &&
      observation->have_last_cpu_pc) {
    if (!add_milestone(observation->result, "loader-dma", record->tick,
                       observation->last_cpu_pc)) {
      return false;
    }
    observation->saw_loader_dma = true;
  }
  if (record->device == T64_TRACE_DEVICE_MACHINE &&
      record->kind == T64_TRACE_KIND_CPU_COMPLETION) {
    observation->saw_completion = true;
    observation->completion_tick = record->tick;
    observation->completion_pc = pc;
    observation->completion_opcode = opcode;
  }
  if (record->device == T64_TRACE_DEVICE_RDP &&
      record->kind == T64_TRACE_KIND_RDP_COMMAND) {
    uint8_t command_byte = (uint8_t)(record->value >> 56);
    uint32_t decoded_opcode =
        (uint32_t)(command_byte & UINT8_C(0x3f));
    if (decoded_opcode == UINT32_C(0x37)) {
      observation->video.fill_command_word = record->value;
      observation->video.fill_command_byte = command_byte;
      observation->video.fill_opcode_field = decoded_opcode;
    }
  }
  if (record->device == T64_TRACE_DEVICE_MI) {
    uint32_t pending = (uint32_t)record->value & UINT32_C(0xff);
    uint32_t mask = (uint32_t)(record->value >> 8) & UINT32_C(0xff);
    if (record->kind == T64_TRACE_KIND_MI_MASK &&
        (mask & UINT32_C(0x20)) != 0u) {
      observation->video.semantic_flags |=
          T64_VIDEO_OBSERVATION_FLAG_DP_MASK;
      observation->video.mi_mask_before_clear = mask;
    } else if (record->kind == T64_TRACE_KIND_MI_PENDING) {
      if ((pending & UINT32_C(0x20)) != 0u) {
        observation->video.semantic_flags |=
            T64_VIDEO_OBSERVATION_FLAG_DP_PENDING;
        observation->video.mi_pending_before_clear = pending;
        observation->video.mi_mask_before_clear = mask;
      } else if ((observation->video.semantic_flags &
                  T64_VIDEO_OBSERVATION_FLAG_DP_PENDING) != 0u) {
        observation->video.semantic_flags |=
            T64_VIDEO_OBSERVATION_FLAG_DP_CLEARED;
        observation->video.mi_pending_after_clear = pending;
      }
    }
  }
  return true;
}

static bool drain_trace(t64_machine *machine,
                        trace_observation *observation) {
  t64_trace_record records[T64_TRACE_RECORD_CAPACITY];
  size_t count = 0u;
  uint64_t dropped = 0u;
  bool saturated = false;
  size_t i;

  if (t64_machine_trace_drain(machine, records,
                              sizeof records / sizeof records[0], &count,
                              &dropped, &saturated) != T64_STATUS_OK ||
      dropped != 0u || saturated) {
    return false;
  }
  for (i = 0u; i < count; i++) {
    if (!observe_trace_record(observation, &records[i])) {
      return false;
    }
  }
  return true;
}

static bool copy_observation_text(char *destination, size_t capacity,
                                  const char *source) {
  int length;
  if (destination == NULL || capacity == 0u || source == NULL) return false;
  length = snprintf(destination, capacity, "%s", source);
  return length >= 0 && (size_t)length < capacity;
}

static bool drain_video(t64_machine *machine,
                        trace_observation *observation,
                        uint8_t *packet_scratch, size_t packet_capacity,
                        t64_raw_capture *raw_capture) {
  for (;;) {
    t64_video_packet_info info;
    const bool audio_profile =
        observation->result != NULL &&
        strcmp(observation->result->profile_id,
               T64_REAL_AUDIO_PROFILE_ID) == 0;
    size_t copied = 0u;
    uint8_t *dynamic_packet = NULL;
    uint8_t *destination = packet_scratch;
    size_t destination_capacity = packet_capacity;
    t64_status status = t64_video_info(machine, &info);
    if (status == T64_STATUS_NOT_READY) return true;
    if (status != T64_STATUS_OK || info.width == 0u || info.height == 0u) {
      return false;
    }
    if (raw_capture == NULL &&
        (packet_scratch == NULL || info.packet_bytes > packet_capacity ||
         info.packet_bytes > T64_VIDEO_OBSERVATION_PACKET_CAPACITY ||
         info.source_bytes > T64_VIDEO_OBSERVATION_SOURCE_CAPACITY ||
         info.source_bytes > UINT32_MAX ||
        (!audio_profile &&
         (info.width != UINT32_C(4) || info.height != UINT32_C(3) ||
          info.packet_bytes != T64_VIDEO_OBSERVATION_PACKET_BYTES ||
          info.source_bytes != T64_VIDEO_OBSERVATION_SOURCE_BYTES)) ||
        (audio_profile &&
         (info.width > UINT32_C(12) || info.height > UINT32_C(7))))) {
      return false;
    }
    if (raw_capture != NULL) {
      if (info.packet_bytes == 0u ||
          (uint64_t)info.packet_bytes >
              t64_raw_capture_remaining_bytes(raw_capture) ||
          (uint64_t)info.packet_bytes > T64_RAW_CAPTURE_MAX_BYTES) {
        return false;
      }
      dynamic_packet = (uint8_t *)malloc(info.packet_bytes);
      if (dynamic_packet == NULL) return false;
      destination = dynamic_packet;
      destination_capacity = info.packet_bytes;
    } else if (destination == NULL) {
      return false;
    }
    status = t64_video_copy(machine, destination, destination_capacity,
                            &copied);
    if (status != T64_STATUS_OK || copied != info.packet_bytes) {
      free(dynamic_packet);
      return false;
    }
    if (raw_capture != NULL &&
        !t64_raw_capture_write_video(raw_capture, &info, destination,
                                     copied)) {
      free(dynamic_packet);
      return false;
    }
    if (observation->video.packets_drained != UINT32_MAX) {
      observation->video.packets_drained++;
    } else {
      free(dynamic_packet);
      return false;
    }
    if (!observation->have_video_packet) {
      t64_video_observation *video = &observation->video;
      uint32_t offset;
      uint32_t value;
      bool can_observe = info.packet_bytes <= sizeof video->packet &&
                         info.source_bytes <= sizeof video->source &&
                         info.source_bytes <= UINT32_MAX;
      if (raw_capture != NULL && !can_observe) {
        free(dynamic_packet);
        continue;
      }
      video->magic = T64_VIDEO_OBSERVATION_MAGIC;
      video->version = T64_VIDEO_OBSERVATION_VERSION;
      video->width = info.width;
      video->height = info.height;
      video->stride_bytes = info.stride_bytes;
      video->source_address = info.source_address;
      video->source_format = info.source_format;
      video->output_encoding = info.output_encoding;
      video->field = info.field;
      video->parity = info.parity;
      video->control = info.control;
      video->burst = info.burst;
      video->h_sync = info.h_sync;
      video->v_sync = info.v_sync;
      video->h_start = info.h_start;
      video->v_start = info.v_start;
      video->x_scale = info.x_scale;
      video->y_scale = info.y_scale;
      video->source_bytes = (uint32_t)info.source_bytes;
      video->packet_bytes = (uint32_t)info.packet_bytes;
      video->start_tick = info.start_tick;
      video->end_tick = info.end_tick;
      video->sequence = info.sequence;
      video->commands_completed = info.commands_completed;
      video->fullsync_completed = info.fullsync_completed;
      video->last_command_word = info.last_command_word;
      video->last_command_byte = info.last_command_byte;
      video->last_opcode_field = info.last_opcode_field;
      if (!copy_observation_text(video->profile_id,
                                 sizeof video->profile_id, info.profile_id) ||
          !copy_observation_text(video->build_id,
                                 sizeof video->build_id, info.build_id) ||
          !copy_observation_text(video->vi_clock_policy,
                                 sizeof video->vi_clock_policy,
                                 info.vi_clock_policy)) {
        return false;
      }
      memcpy(video->packet, destination, copied);
      for (offset = 0u; offset < video->source_bytes; offset += 2u) {
        if (t64_machine_bus_read(machine,
                                 video->source_address + offset, 2u,
                                 &value) != T64_STATUS_OK) {
          return false;
        }
        video->source[offset] = (uint8_t)(value >> 8);
        video->source[offset + 1u] = (uint8_t)value;
      }
      observation->have_video_packet = true;
    }
    free(dynamic_packet);
  }
}

static bool drain_audio(t64_machine *machine,
                        trace_observation *observation,
                        t64_raw_capture *raw_capture) {
  for (;;) {
    t64_audio_packet_observation *packet;
    size_t copied = 0u;
    uint8_t *dynamic_packet = NULL;
    t64_audio_packet_info info;
    t64_status status = t64_audio_info(machine, &info);
    if (status == T64_STATUS_NOT_READY) return true;
    if (status != T64_STATUS_OK || info.byte_length == 0u) {
      return false;
    }
    if (raw_capture != NULL) {
      if ((uint64_t)info.byte_length >
              t64_raw_capture_remaining_bytes(raw_capture) ||
          (uint64_t)info.byte_length > T64_RAW_CAPTURE_MAX_BYTES) {
        return false;
      }
      dynamic_packet = (uint8_t *)malloc(info.byte_length);
      if (dynamic_packet == NULL) return false;
      status = t64_audio_copy(machine, dynamic_packet, info.byte_length,
                              &copied);
      if (status != T64_STATUS_OK || copied != info.byte_length ||
          !t64_raw_capture_write_audio(raw_capture, &info, dynamic_packet,
                                       copied)) {
        free(dynamic_packet);
        return false;
      }
      if (info.byte_length > T64_AUDIO_OBSERVATION_PACKET_BYTES ||
          observation->audio.packet_count >=
              T64_AUDIO_OBSERVATION_PACKET_COUNT) {
        free(dynamic_packet);
        continue;
      }
    } else if (observation->audio.packet_count >=
               T64_AUDIO_OBSERVATION_PACKET_COUNT ||
               info.struct_size != (uint32_t)sizeof info ||
               info.byte_length != T64_AUDIO_OBSERVATION_PACKET_BYTES) {
      return false;
    }
    packet = &observation->audio.packets[observation->audio.packet_count];
    packet->info = info;
    if (raw_capture == NULL) {
      status = t64_audio_copy(machine, packet->bytes, sizeof packet->bytes,
                              &copied);
      if (status != T64_STATUS_OK || copied != info.byte_length) return false;
    } else {
      memcpy(packet->bytes, dynamic_packet, info.byte_length);
      free(dynamic_packet);
    }
    observation->audio.packet_count++;
  }
}

static bool finish_video_observation(t64_machine *machine,
                                    trace_observation *observation) {
  t64_video_observation *video = &observation->video;
  uint32_t value;
  if (!observation->have_video_packet) return true;
  if (t64_machine_bus_read(machine, UINT32_C(0x04100008), 4u, &value) !=
      T64_STATUS_OK) {
    return false;
  }
  video->dpc_current = value;
  if (t64_machine_bus_read(machine, UINT32_C(0x04100004), 4u, &value) !=
      T64_STATUS_OK) {
    return false;
  }
  video->dpc_end = value;
  return true;
}

static bool video_observation_valid(const trace_observation *observation) {
  const t64_video_observation *video = &observation->video;
  const bool audio_profile =
      observation->result != NULL &&
      strcmp(observation->result->profile_id, T64_REAL_AUDIO_PROFILE_ID) == 0;
  const uint32_t all_dp_flags = T64_VIDEO_OBSERVATION_FLAG_DP_MASK |
                                T64_VIDEO_OBSERVATION_FLAG_DP_PENDING |
                                T64_VIDEO_OBSERVATION_FLAG_DP_CLEARED;
  if (audio_profile) {
    return observation->have_video_packet && video->width == 12u &&
           video->height == 7u && video->stride_bytes == 48u &&
           video->source_address == UINT32_C(0x3000) &&
           video->source_bytes == 168u && video->packet_bytes == 336u &&
           video->packets_drained >= 2u &&
           video->source_format == T64_VIDEO_SOURCE_RGBA5551 &&
           video->output_encoding == T64_VIDEO_ENCODING_RGBA8 &&
           video->end_tick - video->start_tick == UINT64_C(411) &&
           video->commands_completed == UINT64_C(6) &&
           video->fullsync_completed == UINT64_C(1) &&
           video->fill_command_word == UINT64_C(0x37000000f80107c1) &&
           video->fill_command_byte == UINT32_C(0x37) &&
           video->fill_opcode_field == UINT32_C(0x37) &&
           video->dpc_current == UINT32_C(0x3130) &&
           video->dpc_end == UINT32_C(0x3130) &&
           (video->semantic_flags & all_dp_flags) == all_dp_flags &&
           video->mi_pending_before_clear == UINT32_C(0x24) &&
           video->mi_mask_before_clear == UINT32_C(0x20) &&
           video->mi_pending_after_clear == UINT32_C(0x04);
  }
  return observation->have_video_packet && video->width == 4u &&
         video->height == 3u && video->stride_bytes == 16u &&
         video->source_address == UINT32_C(0x3000) &&
         video->source_bytes == T64_VIDEO_OBSERVATION_SOURCE_BYTES &&
         video->packet_bytes == T64_VIDEO_OBSERVATION_PACKET_BYTES &&
         video->source_format == T64_VIDEO_SOURCE_RGBA5551 &&
         video->output_encoding == T64_VIDEO_ENCODING_RGBA8 &&
         video->end_tick - video->start_tick == UINT64_C(20) &&
         video->commands_completed == UINT64_C(6) &&
         video->fullsync_completed == UINT64_C(1) &&
         video->fill_command_word == UINT64_C(0x37000000f80107c1) &&
         video->fill_command_byte == UINT32_C(0x37) &&
         video->fill_opcode_field == UINT32_C(0x37) &&
         video->dpc_current == UINT32_C(0x3130) &&
         video->dpc_end == UINT32_C(0x3130) &&
         (video->semantic_flags & all_dp_flags) == all_dp_flags &&
         video->mi_pending_before_clear == UINT32_C(0x20) &&
         video->mi_mask_before_clear == UINT32_C(0x20) &&
         video->mi_pending_after_clear == 0u;
}

static bool audio_observation_valid(const trace_observation *observation) {
  const t64_audio_observation *audio = &observation->audio;
  size_t i;
  if (audio->magic != T64_AUDIO_OBSERVATION_MAGIC ||
      audio->version != T64_AUDIO_OBSERVATION_VERSION ||
      audio->packet_count != T64_AUDIO_OBSERVATION_PACKET_COUNT ||
      audio->bus_write_count == 0u || audio->sw_retirement_count == 0u ||
      audio->trace_overflow != 0u || audio->mi_ai_read_seen == 0u ||
      audio->guest_mi_snapshot_seen == 0u ||
      (audio->guest_mi_snapshot & UINT32_C(0x04)) == 0u) {
    return false;
  }
  for (i = 0u; i < T64_AUDIO_OBSERVATION_PACKET_COUNT; i++) {
    const t64_audio_packet_info *info = &audio->packets[i].info;
    if (info->sequence != (uint64_t)i + UINT64_C(1) ||
        info->frame_count != 4u || info->byte_length != 16u ||
        info->rate_numerator != UINT64_C(48681812) ||
        info->rate_denominator != UINT64_C(1104) || info->channels != 2u ||
        info->bits_per_sample != 16u ||
        info->encoding != T64_AUDIO_ENCODING_S16BE ||
        strcmp(info->profile_id, T64_REAL_AUDIO_PROFILE_ID) != 0) {
      return false;
    }
  }
  return true;
}

static bool profile_observations_valid(
    const t64_profile_descriptor *profile,
    const trace_observation *observation) {
  const bool audio_profile =
      strcmp(profile->profile_id, T64_REAL_AUDIO_PROFILE_ID) == 0;
  const bool rdp_vi_profile =
      profile->machine_model_version == 11u ||
      profile->machine_model_version == 12u ||
      profile->machine_model_version == 13u ||
      profile->machine_model_version == 14u;
  if (audio_profile) {
    return video_observation_valid(observation) &&
           audio_observation_valid(observation);
  }
  return !rdp_vi_profile || video_observation_valid(observation);
}

static bool drain_output(t64_machine *machine, uint8_t output[4096],
                         size_t *length_io, uint64_t *total_out,
                         bool *lost_out, bool *total_exact_out) {
  t64_output_drain_result result;
  size_t capacity;
  t64_status status;

  if (length_io == NULL || total_out == NULL || lost_out == NULL ||
      total_exact_out == NULL ||
      *length_io > T64_REAL_OUTPUT_CAPACITY) {
    return false;
  }
  capacity = *length_io < T64_REAL_OUTPUT_CAPACITY
                 ? T64_REAL_OUTPUT_CAPACITY - *length_io
                 : 0u;
  status = t64_machine_output_drain(machine, output + *length_io, capacity,
                                    &result);
  if (status != T64_STATUS_OK || result.copied_bytes > capacity ||
      result.copied_bytes > SIZE_MAX - *length_io) {
    return false;
  }
  *length_io += result.copied_bytes;
  *total_exact_out = result.total_attempted_bytes_exact;
  *total_out = result.total_attempted_bytes_exact
                   ? result.total_attempted_bytes
                   : 0u;
  *lost_out = result.lost_output;
  return true;
}

static bool write_all(int fd, const uint8_t *bytes, size_t length) {
  size_t offset = 0u;
  while (offset < length) {
    ssize_t count = write(fd, bytes + offset, length - offset);
    if (count < 0 && errno == EINTR) {
      continue;
    }
    if (count <= 0) {
      return false;
    }
    offset += (size_t)count;
  }
  return true;
}

#ifdef T64_REAL_RUNNER_TESTING
static bool write_report_missing_log_sha256(const t64_real_result *result,
                                            int fd) {
  static const char field_name[] = "\"log_sha256\":";
  char report[T64_REAL_RESULT_MAX_BYTES + 1u];
  size_t length = 0u;
  char *field;
  char *value_end;
  size_t field_offset;
  size_t removed_bytes;

  if (!t64_real_result_encode(result, report, sizeof report - 1u, &length) ||
      length >= sizeof report) {
    return false;
  }
  report[length] = '\0';
  field = strstr(report, field_name);
  if (field == NULL) {
    return false;
  }
  value_end = strchr(field + sizeof field_name - 1u, ',');
  if (value_end == NULL) {
    return false;
  }
  field_offset = (size_t)(field - report);
  removed_bytes = (size_t)(value_end + 1u - field);
  memmove(field, value_end + 1u, length - field_offset - removed_bytes);
  length -= removed_bytes;
  return write_all(fd, (const uint8_t *)report, length);
}
#endif

static void set_prelaunch_result(t64_real_result *result, const char *detail,
                                 const char *stop_kind) {
  (void)memset(result, 0, sizeof *result);
  result->prelaunch = true;
  result->guest_stop_kind[0] = '\0';
  (void)snprintf(result->guest_stop_kind, sizeof result->guest_stop_kind,
                 "%s", stop_kind);
  (void)snprintf(result->outcome, sizeof result->outcome, "harness-error");
  (void)snprintf(result->detail, sizeof result->detail, "%s", detail);
}

static int emit_prelaunch(const char *detail, const char *stop_kind,
                          uint64_t bound_tick, int exit_code) {
  t64_real_result result;
  set_prelaunch_result(&result, detail, stop_kind);
  result.bound_tick = bound_tick;
  if (!t64_real_result_write_fd(&result, STDOUT_FILENO)) {
    return 2;
  }
  return exit_code;
}

static bool valid_build_identity(const char *build_id) {
  const char prefix[] = "t64-build-v1-";
  size_t i;
  if (build_id == NULL || strlen(build_id) != sizeof prefix - 1u + 64u ||
      memcmp(build_id, prefix, sizeof prefix - 1u) != 0) {
    return false;
  }
  for (i = sizeof prefix - 1u; build_id[i] != '\0'; i++) {
    if (!((build_id[i] >= '0' && build_id[i] <= '9') ||
          (build_id[i] >= 'a' && build_id[i] <= 'f'))) {
      return false;
    }
  }
  return true;
}

static bool valid_source_revision(const char *revision) {
  size_t i;
  if (revision == NULL || strlen(revision) != 40u) {
    return false;
  }
  for (i = 0u; revision[i] != '\0'; i++) {
    if (!((revision[i] >= '0' && revision[i] <= '9') ||
          (revision[i] >= 'a' && revision[i] <= 'f'))) {
      return false;
    }
  }
  return true;
}

static bool initialize_launched_result(t64_real_result *result,
                                       const t64_profile_descriptor *profile,
                                       const t64_real_fixture *fixture,
                                       const char *fixture_sha,
                                       const char *input_sha) {
  uint8_t profile_digest[T64_DIGEST_SIZE];
  char profile_hex[T64_SHA256_HEX_SIZE];
  const char *build_id = t64_build_identity();
  const char *source_revision = t64_source_revision();
  char asset_hex[T64_SHA256_HEX_SIZE];
  char payload_hex[T64_SHA256_HEX_SIZE];

  if (profile == NULL || fixture == NULL || !valid_build_identity(build_id) ||
      !valid_source_revision(source_revision) ||
      t64_profile_hash(profile, profile_digest) != T64_STATUS_OK) {
    return false;
  }
  t64_sha256_hex(profile_digest, profile_hex);
  t64_sha256_hex(profile->asset_sha256, asset_hex);
  t64_sha256_hex(profile->payload_sha256, payload_hex);
  if (strcmp(profile->profile_id, fixture->profile_id) != 0 ||
      strcmp(profile_hex, fixture->profile_sha256) != 0 ||
      strcmp(asset_hex, T64_REAL_EXPECTED_ASSET_SHA256) != 0 ||
      strcmp(payload_hex, T64_REAL_EXPECTED_PAYLOAD_SHA256) != 0) {
    return false;
  }

  (void)memset(result, 0, sizeof *result);
  (void)snprintf(result->build_id, sizeof result->build_id, "%s", build_id);
  (void)snprintf(result->source_revision, sizeof result->source_revision, "%s",
                 source_revision);
  (void)snprintf(result->profile_id, sizeof result->profile_id, "%s",
                 profile->profile_id);
  (void)snprintf(result->profile_digest, sizeof result->profile_digest, "%s",
                 profile_hex);
  (void)snprintf(result->fixture_id, sizeof result->fixture_id, "%s",
                 fixture->id);
  (void)snprintf(result->fixture_sha256, sizeof result->fixture_sha256, "%s",
                 fixture_sha);
  (void)snprintf(result->ipl3_sha256, sizeof result->ipl3_sha256, "%s",
                 asset_hex);
  (void)snprintf(result->ipl3_payload_sha256,
                 sizeof result->ipl3_payload_sha256, "%s", payload_hex);
  (void)snprintf(result->input_digest, sizeof result->input_digest, "%s",
                 input_sha);
  result->has_input_digest = true;
  result->has_log_sha256 = false;
  result->has_state_digest = false;
  result->bound_tick = 0u;
  (void)snprintf(result->guest_stop_kind, sizeof result->guest_stop_kind,
                 "host-error");
  (void)snprintf(result->outcome, sizeof result->outcome, "harness-error");
  (void)snprintf(result->detail, sizeof result->detail, "host-error");
  return true;
}

static void set_digest(char output[T64_REAL_RESULT_HEX_SIZE],
                       const uint8_t digest[T64_DIGEST_SIZE]) {
  t64_sha256_hex(digest, output);
}

static int run_real(int fixture_fd, int input_fd, const char *requested_profile,
                    uint64_t guest_ticks, uint64_t advance_step,
                    int video_observation_fd, int audio_observation_fd,
                    const char *raw_capture_dir,
                    uint64_t raw_capture_max_bytes) {
  static const t64_host_allocator allocator = {
      allocator_allocate, allocator_release, NULL};
  owned_bytes fixture = {0};
  owned_bytes input = {0};
  t64_input_schedule *schedule = NULL;
  t64_input_error input_error;
  const t64_profile_descriptor *profile = NULL;
  const t64_real_fixture *fixture_identity = NULL;
  t64_machine *machine = NULL;
  t64_raw_capture *raw_capture = NULL;
  t64_real_result result;
  trace_observation observations = {0};
  uint8_t output[T64_REAL_OUTPUT_CAPACITY] = {0};
  uint8_t *video_scratch = NULL;
  size_t output_length = 0u;
  uint64_t output_total = 0u;
  bool output_lost = false;
  bool output_total_exact = true;
  uint64_t current_tick = 0u;
  uint64_t resource_yields = 0u;
  t64_stop_result stop = {UINT32_MAX, UINT64_MAX};
  t64_cpu_stop_detail stop_detail = {UINT64_MAX, UINT32_MAX, UINT32_MAX};
  t64_status status;
  int exit_code = 0;
  char input_digest[T64_SHA256_HEX_SIZE];
  uint8_t state_digest[T64_DIGEST_SIZE];
  uint8_t content_digest[T64_DIGEST_SIZE];
  char output_digest[T64_SHA256_HEX_SIZE];
  char content_digest_hex[T64_SHA256_HEX_SIZE];
  bool qualified_completion;

  if (!read_snapshot(fixture_fd, T64_REAL_FIXTURE_MAX_BYTES, 0u, &fixture)) {
    exit_code = emit_prelaunch("refusal", "refused", guest_ticks, 0);
    goto cleanup;
  }
  fixture_identity = t64_real_fixture_find_bytes(fixture.length,
                                                 fixture.sha256_hex);
  if (fixture_identity == NULL) {
    exit_code = emit_prelaunch("refusal", "refused", guest_ticks, 0);
    goto cleanup;
  }
  if (!read_snapshot(input_fd, T64_INPUT_MAX_BYTES, 0u, &input)) {
    exit_code = emit_prelaunch("refusal", "refused", guest_ticks, 0);
    goto cleanup;
  }
  if (t64_input_parse(input.bytes, input.length, &schedule, &input_error) !=
      T64_INPUT_OK) {
    exit_code = emit_prelaunch("refusal", "refused", guest_ticks, 0);
    goto cleanup;
  }
  {
    size_t canonical_length = 0u;
    const uint8_t *canonical =
        t64_input_schedule_canonical_json(schedule, &canonical_length);
    if (canonical == NULL ||
        !digest_hex(canonical, canonical_length, input_digest)) {
      exit_code = emit_prelaunch("refusal", "refused", guest_ticks, 0);
      goto cleanup;
    }
  }
  if (strcmp(requested_profile, fixture_identity->profile_id) != 0 ||
      t64_profile_lookup(fixture_identity->profile_id, &profile) !=
          T64_STATUS_OK ||
      profile == NULL ||
      !bytes_equal_hex(fixture.bytes + 0x40u, 4032u,
                       T64_REAL_EXPECTED_PAYLOAD_SHA256)) {
    exit_code = emit_prelaunch("refusal", "refused", guest_ticks, 0);
    goto cleanup;
  }
  {
    size_t i;
    t64_input_event event;
    for (i = 0u; i < t64_input_schedule_event_count(schedule); i++) {
      if (!t64_input_schedule_event(schedule, i, &event) ||
          event.tick > guest_ticks) {
        exit_code = emit_prelaunch("refusal", "refused", guest_ticks, 0);
        goto cleanup;
      }
    }
  }
  if (!initialize_launched_result(&result, profile, fixture_identity,
                                  fixture.sha256_hex,
                                  input_digest)) {
    exit_code = emit_prelaunch("refusal", "refused", guest_ticks, 0);
    goto cleanup;
  }
  result.bound_tick = guest_ticks;
  status = t64_machine_create(&allocator, profile, &machine);
  if (status != T64_STATUS_OK || machine == NULL) {
    exit_code = emit_prelaunch("refusal", "refused", guest_ticks, 0);
    goto cleanup;
  }
  status = t64_machine_trace_set_enabled(machine, true);
  if (status != T64_STATUS_OK) {
    exit_code = emit_prelaunch("refusal", "refused", guest_ticks, 0);
    goto cleanup;
  }
  video_scratch = (uint8_t *)malloc(T64_REAL_VIDEO_MAX_BYTES);
  if (video_scratch == NULL) {
    exit_code = emit_prelaunch("refusal", "refused", guest_ticks, 0);
    goto cleanup;
  }
  status = t64_machine_admit_rom(machine, fixture.bytes, fixture.length);
  if (status != T64_STATUS_OK) {
    exit_code = emit_prelaunch("refusal", "refused", guest_ticks, 0);
    goto cleanup;
  }
  if (t64_machine_content_digest(machine, content_digest) != T64_STATUS_OK) {
    (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                   "host-error");
    (void)snprintf(result.outcome, sizeof result.outcome, "harness-error");
    (void)snprintf(result.detail, sizeof result.detail, "host-error");
    goto finalize;
  }
  t64_sha256_hex(content_digest, content_digest_hex);
  if (strcmp(content_digest_hex, fixture.sha256_hex) != 0) {
    (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                   "host-error");
    (void)snprintf(result.outcome, sizeof result.outcome, "harness-error");
    (void)snprintf(result.detail, sizeof result.detail, "host-error");
    goto finalize;
  }
  if (raw_capture_dir != NULL) {
    t64_raw_capture_identity identity;
    const char *capture_error = NULL;
    size_t content_size = 0u;
    memset(&identity, 0, sizeof identity);
    if (t64_machine_rom_size(machine, &content_size) != T64_STATUS_OK) {
      (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                     "host-error");
      (void)snprintf(result.outcome, sizeof result.outcome, "harness-error");
      (void)snprintf(result.detail, sizeof result.detail, "capture-content-size");
      goto finalize;
    }
    identity.profile_id = profile->profile_id;
    identity.profile_sha256 = result.profile_digest;
    identity.descriptor_version = profile->descriptor_version;
    identity.machine_model_version = profile->machine_model_version;
    identity.state_serialization_version =
        profile->state_serialization_version;
    identity.core_build_id = t64_build_identity();
    identity.runner_build_id = T64_RUNNER_BUILD_ID;
    identity.source_revision = t64_source_revision();
    identity.content_sha256 = content_digest_hex;
    identity.content_size = (uint64_t)content_size;
    identity.fixture_id = fixture_identity->id;
    identity.fixture_sha256 = fixture.sha256_hex;
    identity.fixture_size = (uint64_t)fixture.length;
    identity.input_file_sha256 = input.sha256_hex;
    identity.input_digest = input_digest;
    identity.input_size = (uint64_t)input.length;
    raw_capture = t64_raw_capture_open(raw_capture_dir,
                                       raw_capture_max_bytes, &identity,
                                       &capture_error);
    if (raw_capture == NULL) {
      (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                     "host-error");
      (void)snprintf(result.outcome, sizeof result.outcome, "harness-error");
      (void)snprintf(result.detail, sizeof result.detail, "%s",
                     capture_error == NULL ? "capture-open" : capture_error);
      goto finalize;
    }
  }
  observations.result = &result;
  observations.audio.magic = T64_AUDIO_OBSERVATION_MAGIC;
  observations.audio.version = T64_AUDIO_OBSERVATION_VERSION;
  if (!drain_trace(machine, &observations)) {
    (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                   "host-error");
    (void)snprintf(result.outcome, sizeof result.outcome, "harness-error");
    (void)snprintf(result.detail, sizeof result.detail, "host-error");
    goto finalize;
  }
  {
    size_t i;
    t64_input_event event;
    for (i = 0u; i < t64_input_schedule_event_count(schedule); i++) {
      if (!t64_input_schedule_event(schedule, i, &event) ||
          t64_machine_queue_input(machine, event.tick, event.port,
                                  event.buttons, event.axis_x,
                                  event.axis_y) != T64_STATUS_OK) {
        (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                       "host-error");
        (void)snprintf(result.outcome, sizeof result.outcome,
                       "harness-error");
        (void)snprintf(result.detail, sizeof result.detail, "host-error");
        goto finalize;
      }
    }
  }

  while (current_tick < guest_ticks) {
    uint64_t target =
        guest_ticks - current_tick > advance_step
            ? current_tick + advance_step
            : guest_ticks;
    for (;;) {
      uint64_t before_records = observations.trace_records;
      uint64_t before_tick = current_tick;
      uint32_t before_audio_packets =
          raw_capture == NULL
              ? observations.audio.packet_count
              : (uint32_t)t64_raw_capture_audio_count(raw_capture);
      uint32_t before_video_packets = observations.video.packets_drained;
      status = t64_machine_advance(
          machine,
          &(t64_stop_request){T64_STOP_GUEST_TICK, target}, &stop);
      if (status != T64_STATUS_OK || stop.tick < current_tick ||
          stop.tick > target) {
        (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                       "host-error");
        (void)snprintf(result.outcome, sizeof result.outcome,
                       "harness-error");
        (void)snprintf(result.detail, sizeof result.detail,
                       "advance-invalid");
        goto finalize;
      }
      if (!drain_trace(machine, &observations)) {
        (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                       "host-error");
        (void)snprintf(result.outcome, sizeof result.outcome,
                       "harness-error");
        (void)snprintf(result.detail, sizeof result.detail, "trace-drain-error");
        goto finalize;
      }
      if (!drain_output(machine, output, &output_length, &output_total,
                        &output_lost, &output_total_exact)) {
        (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                       "host-error");
        (void)snprintf(result.outcome, sizeof result.outcome,
                       "harness-error");
        (void)snprintf(result.detail, sizeof result.detail, "output-drain-error");
        goto finalize;
      }
      if (!drain_audio(machine, &observations, raw_capture)) {
        (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                       "host-error");
        (void)snprintf(result.outcome, sizeof result.outcome,
                       "harness-error");
        (void)snprintf(result.detail, sizeof result.detail, "%s",
                       raw_capture == NULL
                           ? "audio-drain-error"
                           : t64_raw_capture_error(raw_capture));
        goto finalize;
      }
      if (!drain_video(machine, &observations, video_scratch,
                       T64_REAL_VIDEO_MAX_BYTES, raw_capture)) {
        (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                       "host-error");
        (void)snprintf(result.outcome, sizeof result.outcome,
                       "harness-error");
        (void)snprintf(result.detail, sizeof result.detail, "%s",
                       raw_capture == NULL
                           ? "video-drain-error"
                           : t64_raw_capture_error(raw_capture));
        goto finalize;
      }
      current_tick = stop.tick;
      if (!output_total_exact) {
        (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                       "host-error");
        (void)snprintf(result.outcome, sizeof result.outcome, "harness-error");
        (void)snprintf(result.detail, sizeof result.detail, "host-error");
        goto finalize;
      }
      if (output_lost) {
        (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                       "output-overflow");
        (void)snprintf(result.outcome, sizeof result.outcome,
                       "harness-error");
        (void)snprintf(result.detail, sizeof result.detail, "output-overflow");
        goto finalize;
      }
      if (stop.reason == T64_STOP_REASON_RESOURCE_LIMIT) {
        const bool made_progress =
            stop.tick > before_tick ||
            observations.trace_records > before_records ||
            observations.audio.packet_count > before_audio_packets ||
            observations.video.packets_drained > before_video_packets;
        resource_yields++;
        if (observations.audio.yield_count <
            T64_AUDIO_OBSERVATION_MAX_YIELDS) {
          t64_audio_yield_observation *yield =
              &observations.audio.yields[
                  observations.audio.yield_count++];
          yield->target_tick = target;
          yield->before_tick = before_tick;
          yield->reached_tick = stop.tick;
          yield->video_before = before_video_packets;
          yield->video_after = observations.video.packets_drained;
          yield->audio_before = before_audio_packets;
          yield->audio_after = observations.audio.packet_count;
        } else {
          observations.audio.trace_overflow = 1u;
        }
        if (resource_yields > T64_REAL_MAX_RESOURCE_YIELDS || !made_progress) {
          (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                         "host-error");
          (void)snprintf(result.outcome, sizeof result.outcome,
                         "harness-error");
          (void)snprintf(result.detail, sizeof result.detail, "%s",
                         resource_yields > T64_REAL_MAX_RESOURCE_YIELDS
                             ? "resource-yield-limit"
                             : "resource-yield-no-progress");
          goto finalize;
        }
        continue;
      }
      break;
    }
    if (stop.reason == T64_STOP_REASON_BOUND) {
      if (current_tick != target) {
        (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                       "host-error");
        (void)snprintf(result.outcome, sizeof result.outcome,
                       "harness-error");
        (void)snprintf(result.detail, sizeof result.detail, "host-error");
        goto finalize;
      }
      if (current_tick == guest_ticks) {
        (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                       "guest-bound");
        if (output_length == 0u) {
          (void)snprintf(result.outcome, sizeof result.outcome, "boot-fail");
          (void)snprintf(result.detail, sizeof result.detail, "empty-output");
        } else {
          (void)snprintf(result.outcome, sizeof result.outcome, "timeout");
          (void)snprintf(result.detail, sizeof result.detail, "guest-bound");
        }
        break;
      }
      continue;
    }
    if (stop.reason == T64_STOP_REASON_GUEST_COMPLETION ||
        stop.reason == T64_STOP_REASON_CPU_UNSUPPORTED) {
      if (t64_machine_cpu_stop_detail(machine, &stop_detail) !=
          T64_STATUS_OK ||
          stop_detail.retired_instructions != observations.retired) {
        (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                       "host-error");
        (void)snprintf(result.outcome, sizeof result.outcome,
                       "harness-error");
        (void)snprintf(result.detail, sizeof result.detail, "host-error");
        goto finalize;
      }
      break;
    }
    (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                   "host-error");
    (void)snprintf(result.outcome, sizeof result.outcome, "harness-error");
    (void)snprintf(result.detail, sizeof result.detail,
                   "stop-reason-unexpected");
    goto finalize;
  }

  if (observations.have_video_packet &&
      !finish_video_observation(machine, &observations)) {
    (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                   "host-error");
    (void)snprintf(result.outcome, sizeof result.outcome, "harness-error");
    (void)snprintf(result.detail, sizeof result.detail, "host-error");
    goto finalize;
  }

  if ((stop.reason == T64_STOP_REASON_GUEST_COMPLETION ||
       stop.reason == T64_STOP_REASON_CPU_UNSUPPORTED) &&
      !observations.saw_hello_entry &&
      stop_detail.pc == T64_REAL_HELLO_ENTRY_PC) {
    if (!add_milestone(&result, "hello-entry", stop.tick,
                       T64_REAL_HELLO_ENTRY_PC)) {
      (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                     "host-error");
      (void)snprintf(result.outcome, sizeof result.outcome, "harness-error");
      (void)snprintf(result.detail, sizeof result.detail, "host-error");
      goto finalize;
    }
    observations.saw_hello_entry = true;
  }

  if (stop.reason == T64_STOP_REASON_GUEST_COMPLETION) {
    qualified_completion =
        fixture_identity != NULL && fixture_identity->completion_pc != 0u &&
        observations.saw_completion &&
        observations.completion_pc == fixture_identity->completion_pc &&
        observations.completion_opcode == T64_REAL_BREAK_OPCODE &&
        stop_detail.pc == fixture_identity->completion_pc &&
        stop_detail.opcode == T64_REAL_BREAK_OPCODE &&
        observations.saw_ipl3_entry && observations.saw_loader_dma &&
        observations.saw_hello_entry;
    if (qualified_completion &&
        !add_milestone(&result, "hello-completion",
                       observations.completion_tick,
                       observations.completion_pc)) {
      (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                     "host-error");
      (void)snprintf(result.outcome, sizeof result.outcome, "harness-error");
      (void)snprintf(result.detail, sizeof result.detail, "host-error");
      goto finalize;
    }
    if (qualified_completion) {
      (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                     "authored-completion");
      if (output_length == 0u) {
        (void)snprintf(result.outcome, sizeof result.outcome, "boot-fail");
        (void)snprintf(result.detail, sizeof result.detail, "empty-output");
      } else if (fixture_identity->pass_eligible &&
                 profile_observations_valid(profile, &observations) &&
                 fixture.sha256_hex[0] != '\0' &&
                 output_length == sizeof expected_log - 1u &&
                 memcmp(output, expected_log, sizeof expected_log - 1u) == 0 &&
                 bytes_equal_hex(output, output_length,
                                 T64_REAL_EXPECTED_LOG_SHA256)) {
        (void)snprintf(result.outcome, sizeof result.outcome, "pass");
        (void)snprintf(result.detail, sizeof result.detail, "positive-match");
      } else {
        (void)snprintf(result.outcome, sizeof result.outcome, "guest-fail");
        (void)snprintf(result.detail, sizeof result.detail, "wrong-transcript");
      }
    } else {
      (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                     "unsupported-execution");
      (void)snprintf(result.outcome, sizeof result.outcome, "boot-fail");
      (void)snprintf(result.detail, sizeof result.detail,
                     "unsupported-execution");
    }
  } else if (stop.reason == T64_STOP_REASON_CPU_UNSUPPORTED) {
    (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                   "unsupported-execution");
    (void)snprintf(result.outcome, sizeof result.outcome, "boot-fail");
    (void)snprintf(result.detail, sizeof result.detail,
                   "unsupported-execution");
  }

finalize:
  observations.audio.guest_stop_tick = stop.tick;
  observations.audio.guest_stop_reason = stop.reason;
  if (observations.have_video_packet &&
      !finish_video_observation(machine, &observations)) {
    (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                   "host-error");
    (void)snprintf(result.outcome, sizeof result.outcome, "harness-error");
    (void)snprintf(result.detail, sizeof result.detail, "host-error");
  }
  result.guest_tick = stop.tick <= guest_ticks ? stop.tick : current_tick;
  result.instruction_count = observations.retired;
  if (!output_total_exact) {
    /* A saturated attempted-byte total is a lower bound, not an exact count.
     * Do not publish either the saturated value or a partial capture as a
     * verified log. */
    output_length = 0u;
    output_total = 0u;
    result.has_log_sha256 = false;
    result.log_bytes = 0u;
    result.log_bytes_total = 0u;
    (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                   "host-error");
    (void)snprintf(result.outcome, sizeof result.outcome, "harness-error");
    (void)snprintf(result.detail, sizeof result.detail, "host-error");
  } else {
    result.log_bytes = (uint64_t)output_length;
    result.log_bytes_total = output_total;
    if (result.log_bytes == result.log_bytes_total) {
      if (digest_hex(output, output_length, output_digest)) {
        (void)snprintf(result.log_sha256, sizeof result.log_sha256, "%s",
                       output_digest);
        result.has_log_sha256 = true;
      }
    } else if (strcmp(result.detail, "output-overflow") == 0) {
      if (digest_hex(output, output_length, output_digest)) {
        (void)snprintf(result.log_sha256, sizeof result.log_sha256, "%s",
                       output_digest);
        result.has_log_sha256 = true;
      }
    }
  }
  if (machine != NULL &&
      t64_machine_digest(machine, state_digest) == T64_STATUS_OK) {
    set_digest(result.state_digest, state_digest);
    result.has_state_digest = true;
  }
  if (video_observation_fd >= 0) {
    const bool observation_valid = video_observation_valid(&observations);
    const bool record_written = write_all(
        video_observation_fd, (const uint8_t *)&observations.video,
        sizeof observations.video);
    if (!observation_valid || !record_written) {
      const bool had_host_error =
          strcmp(result.guest_stop_kind, "host-error") == 0;
      (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                     "host-error");
      (void)snprintf(result.outcome, sizeof result.outcome, "harness-error");
      if (!had_host_error) {
        (void)snprintf(result.detail, sizeof result.detail, "%s",
                       record_written ? "video-observation-invalid"
                                      : "video-observation-write-error");
      }
    }
  }
  if (audio_observation_fd >= 0) {
    const bool observation_valid = audio_observation_valid(&observations);
    const bool record_written = write_all(
        audio_observation_fd, (const uint8_t *)&observations.audio,
        sizeof observations.audio);
    if (!observation_valid || !record_written) {
      const bool had_host_error =
          strcmp(result.guest_stop_kind, "host-error") == 0;
      (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                     "host-error");
      (void)snprintf(result.outcome, sizeof result.outcome, "harness-error");
      if (!had_host_error) {
        (void)snprintf(result.detail, sizeof result.detail, "%s",
                       record_written ? "audio-observation-invalid"
                                      : "audio-observation-write-error");
      }
    }
  }
#ifdef T64_REAL_RUNNER_TESTING
  if (test_fault_is("capture-closed")) {
    (void)close(STDERR_FILENO);
  } else
#endif
  if (!write_all(STDERR_FILENO, output, output_length)) {
    result.has_log_sha256 = false;
    result.log_bytes = 0u;
    result.log_bytes_total = 0u;
    result.has_state_digest = false;
    (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                   "host-error");
    (void)snprintf(result.outcome, sizeof result.outcome, "harness-error");
    (void)snprintf(result.detail, sizeof result.detail, "host-error");
  }
  if (raw_capture != NULL) {
    bool capture_ok = false;
    if (stop.reason == T64_STOP_REASON_GUEST_COMPLETION &&
        strcmp(result.guest_stop_kind, "host-error") != 0) {
      capture_ok = t64_raw_capture_finalize(
          raw_capture, result.guest_tick, result.outcome,
          result.guest_stop_kind);
    } else {
      (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                     "host-error");
      (void)snprintf(result.outcome, sizeof result.outcome, "harness-error");
      (void)snprintf(result.detail, sizeof result.detail,
                     "capture-incomplete-terminal");
    }
    if (!capture_ok) {
      (void)snprintf(result.guest_stop_kind, sizeof result.guest_stop_kind,
                     "host-error");
      (void)snprintf(result.outcome, sizeof result.outcome, "harness-error");
      (void)snprintf(result.detail, sizeof result.detail, "%s",
                     t64_raw_capture_error(raw_capture));
#ifdef T64_REAL_RUNNER_TESTING
      if (test_fault_is("capture-retain-incomplete-stage") &&
          strcmp(t64_raw_capture_error(raw_capture),
                 "capture-incomplete-streams") == 0) {
        t64_raw_capture_preserve_stage_for_test(raw_capture);
      } else
#endif
      t64_raw_capture_abort(raw_capture);
    }
  }
#ifdef T64_REAL_RUNNER_TESTING
  if (test_fault_is("report-missing-log-sha256")) {
    if (!write_report_missing_log_sha256(&result, STDOUT_FILENO)) {
      exit_code = 2;
    }
  } else
#endif
  if (!t64_real_result_write_fd(&result, STDOUT_FILENO)) {
    exit_code = 2;
  }

cleanup:
  t64_raw_capture_destroy(raw_capture);
  if (machine != NULL) {
    t64_machine_destroy(machine);
  }
  free(video_scratch);
  t64_input_schedule_destroy(schedule);
  free(fixture.bytes);
  free(input.bytes);
  return exit_code;
}

int main(int argc, char **argv) {
  int fixture_fd = -1;
  int input_fd = -1;
  int video_observation_fd = -1;
  int audio_observation_fd = -1;
  const char *profile_id = NULL;
  const char *raw_capture_dir = NULL;
  uint64_t guest_ticks = 0u;
  uint64_t advance_step = T64_REAL_TRACE_STEP;
  uint64_t raw_capture_max_bytes = 0u;
  bool seen_fixture = false;
  bool seen_input = false;
  bool seen_profile = false;
  bool seen_ticks = false;
  bool seen_video_observation = false;
  bool seen_audio_observation = false;
  bool seen_raw_capture_dir = false;
  bool seen_raw_capture_max_bytes = false;
#ifdef T64_REAL_RUNNER_TESTING
  bool seen_advance_step = false;
#endif
  int i;

  if (argc < 9 || (argc % 2) == 0) {
    return emit_prelaunch("usage", "not-started", 0u, 2);
  }
  for (i = 1; i < argc; i += 2) {
    const char *option = argv[i];
    const char *value = argv[i + 1];
    if (strcmp(option, "--fixture-fd") == 0 && !seen_fixture &&
        parse_fd(value, &fixture_fd)) {
      seen_fixture = true;
    } else if (strcmp(option, "--input-fd") == 0 && !seen_input &&
               parse_fd(value, &input_fd)) {
      seen_input = true;
    } else if (strcmp(option, "--profile") == 0 && !seen_profile &&
               t64_real_fixture_profile_allowed(value)) {
      seen_profile = true;
      profile_id = value;
    } else if (strcmp(option, "--guest-ticks") == 0 && !seen_ticks &&
               parse_u64(value, &guest_ticks) && guest_ticks > 0u &&
               guest_ticks <= UINT64_C(600000)) {
      seen_ticks = true;
    } else if (strcmp(option, "--video-observation-fd") == 0 &&
               !seen_video_observation && parse_fd(value,
                                                   &video_observation_fd)) {
      struct stat observation_stat;
      int observation_flags = fcntl(video_observation_fd, F_GETFL);
      if (fstat(video_observation_fd, &observation_stat) != 0 ||
          !S_ISFIFO(observation_stat.st_mode) || observation_flags < 0 ||
          (observation_flags & O_ACCMODE) != O_WRONLY) {
        return emit_prelaunch("usage", "not-started", guest_ticks, 2);
      }
      seen_video_observation = true;
    } else if (strcmp(option, "--audio-observation-fd") == 0 &&
               !seen_audio_observation &&
               parse_fd(value, &audio_observation_fd)) {
      struct stat observation_stat;
      int observation_flags = fcntl(audio_observation_fd, F_GETFL);
      if (fstat(audio_observation_fd, &observation_stat) != 0 ||
          !S_ISFIFO(observation_stat.st_mode) || observation_flags < 0 ||
          (observation_flags & O_ACCMODE) != O_WRONLY) {
        return emit_prelaunch("usage", "not-started", guest_ticks, 2);
      }
      seen_audio_observation = true;
    } else if (strcmp(option, "--raw-capture-dir") == 0 &&
               !seen_raw_capture_dir && value[0] != '\0') {
      raw_capture_dir = value;
      seen_raw_capture_dir = true;
    } else if (strcmp(option, "--raw-capture-max-bytes") == 0 &&
               !seen_raw_capture_max_bytes &&
               parse_u64(value, &raw_capture_max_bytes)) {
      seen_raw_capture_max_bytes = true;
#ifdef T64_REAL_RUNNER_TESTING
    } else if (strcmp(option, "--advance-step") == 0 &&
               !seen_advance_step && parse_u64(value, &advance_step) &&
               advance_step > 0u &&
               advance_step <= T64_REAL_TEST_MAX_ADVANCE_STEP) {
      seen_advance_step = true;
#endif
    } else {
      return emit_prelaunch("usage", "not-started", guest_ticks, 2);
    }
  }
  if (!seen_fixture || !seen_input || !seen_profile || !seen_ticks) {
    return emit_prelaunch("usage", "not-started", guest_ticks, 2);
  }
  if ((seen_raw_capture_max_bytes && !seen_raw_capture_dir) ||
      (seen_raw_capture_dir && seen_raw_capture_max_bytes &&
       (raw_capture_max_bytes == 0u ||
        raw_capture_max_bytes > T64_RAW_CAPTURE_MAX_BYTES))) {
    return emit_prelaunch("usage", "not-started", guest_ticks, 2);
  }
  if (seen_raw_capture_dir && !seen_raw_capture_max_bytes) {
    raw_capture_max_bytes = T64_RAW_CAPTURE_DEFAULT_MAX_BYTES;
  }
#ifdef T64_REAL_RUNNER_TESTING
  g_test_fault = getenv("T64_REAL_RUNNER_TEST_FAULT");
  if (g_test_fault != NULL &&
      strcmp(g_test_fault, "exit-nonzero") != 0 &&
      strcmp(g_test_fault, "capture-closed") != 0 &&
      strcmp(g_test_fault, "crash-after-output") != 0 &&
      strcmp(g_test_fault, "delay-after-output") != 0 &&
      strcmp(g_test_fault, "report-missing") != 0 &&
      strcmp(g_test_fault, "report-truncated") != 0 &&
      strcmp(g_test_fault, "report-oversize") != 0 &&
      strcmp(g_test_fault, "report-malformed") != 0 &&
      strcmp(g_test_fault, "report-unknown") != 0 &&
      strcmp(g_test_fault, "report-missing-log-sha256") != 0 &&
      strcmp(g_test_fault, "capture-retain-incomplete-stage") != 0) {
    return emit_prelaunch("usage", "not-started", guest_ticks, 2);
  }
#endif
  {
    int status = run_real(fixture_fd, input_fd, profile_id, guest_ticks,
                          advance_step, video_observation_fd,
                          audio_observation_fd, raw_capture_dir,
                          raw_capture_max_bytes);
#ifdef T64_REAL_RUNNER_TESTING
    if (status == 0 && test_fault_is("exit-nonzero")) return 7;
    if (status == 0 && test_fault_is("crash-after-output")) {
      (void)raise(SIGKILL);
    }
    if (status == 0 && test_fault_is("delay-after-output")) {
      (void)sleep(3u);
    }
    if (status == 0 && test_fault_is("report-missing")) {
      if (ftruncate(STDOUT_FILENO, 0) != 0) return 1;
    } else if (status == 0 && test_fault_is("report-truncated")) {
      if (ftruncate(STDOUT_FILENO, (off_t)8) != 0) return 1;
    } else if (status == 0 && test_fault_is("report-oversize")) {
      uint8_t extra[T64_REAL_RESULT_MAX_BYTES];
      memset(extra, 'x', sizeof extra);
      if (!write_all(STDOUT_FILENO, extra, sizeof extra)) return 1;
    } else if (status == 0 && test_fault_is("report-malformed")) {
      static const uint8_t malformed[] = "{not-json}\n";
      if (ftruncate(STDOUT_FILENO, 0) != 0 ||
          lseek(STDOUT_FILENO, 0, SEEK_SET) < 0 ||
          !write_all(STDOUT_FILENO, malformed, sizeof malformed - 1u)) {
        return 1;
      }
    } else if (status == 0 && test_fault_is("report-unknown")) {
      static const uint8_t unknown[] =
          "{\"schema\":\"t64.result\",\"schema_version\":2,"
          "\"unknown\":true}\n";
      if (ftruncate(STDOUT_FILENO, 0) != 0 ||
          lseek(STDOUT_FILENO, 0, SEEK_SET) < 0 ||
          !write_all(STDOUT_FILENO, unknown, sizeof unknown - 1u)) {
        return 1;
      }
    }
#endif
    return status;
  }
}
