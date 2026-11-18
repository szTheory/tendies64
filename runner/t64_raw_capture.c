#define _POSIX_C_SOURCE 200809L
#if defined(__linux__)
#define _GNU_SOURCE
#endif

#include "t64_raw_capture.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <sys/stdio.h>
#elif defined(__linux__)
#include <linux/fs.h>
#include <sys/syscall.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#ifndef NAME_MAX
#define NAME_MAX 255
#endif

#define RAW_CAPTURE_MAX_PACKETS 4096u
#define RAW_CAPTURE_NAME_CAPACITY 48u
#define RAW_CAPTURE_STAGE_CAPACITY 96u
#define RAW_CAPTURE_MANIFEST_MAX (8u * 1024u * 1024u)

typedef struct {
  char name[RAW_CAPTURE_NAME_CAPACITY];
  t64_video_packet_info info;
  uint64_t byte_length;
  dev_t device;
  ino_t inode;
} raw_video_entry;

typedef struct {
  char name[RAW_CAPTURE_NAME_CAPACITY];
  t64_audio_packet_info info;
  uint64_t byte_length;
  dev_t device;
  ino_t inode;
} raw_audio_entry;

typedef struct {
  char *bytes;
  size_t capacity;
  size_t length;
  bool failed;
} raw_json;

struct t64_raw_capture {
  int parent_fd;
  int stage_fd;
  int video_fd;
  int audio_fd;
  char target_name[NAME_MAX + 1u];
  char stage_name[RAW_CAPTURE_STAGE_CAPACITY];
  dev_t stage_device;
  ino_t stage_inode;
  dev_t video_device;
  ino_t video_inode;
  dev_t audio_device;
  ino_t audio_inode;
  dev_t manifest_device;
  ino_t manifest_inode;
  uint64_t byte_cap;
  uint64_t stream_bytes;
  size_t video_count;
  size_t audio_count;
  raw_video_entry video[RAW_CAPTURE_MAX_PACKETS];
  raw_audio_entry audio[RAW_CAPTURE_MAX_PACKETS];
  char profile_id[64];
  char profile_sha256[65];
  uint32_t descriptor_version;
  uint32_t machine_model_version;
  uint32_t state_serialization_version;
  char core_build_id[128];
  char runner_build_id[128];
  char source_revision[41];
  char content_sha256[65];
  uint64_t content_size;
  char fixture_id[64];
  char fixture_sha256[65];
  uint64_t fixture_size;
  char input_file_sha256[65];
  char input_digest[65];
  uint64_t input_size;
  const char *error;
  bool committed;
#ifdef T64_REAL_RUNNER_TESTING
  bool preserve_stage_for_test;
#endif
};

static bool checked_add_u64(uint64_t left, uint64_t right,
                            uint64_t *sum_out) {
  if (sum_out == NULL || right > UINT64_MAX - left) return false;
  *sum_out = left + right;
  return true;
}

static bool copy_identity(char *destination, size_t capacity,
                          const char *source) {
  size_t length;
  if (destination == NULL || capacity == 0u || source == NULL) return false;
  length = strnlen(source, capacity);
  if (length == 0u || length >= capacity) return false;
  memcpy(destination, source, length + 1u);
  return true;
}

static int open_directory_nofollow(int parent_fd, const char *name) {
  int fd = openat(parent_fd, name,
                  O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
  if (fd < 0) return -1;
  {
    struct stat status;
    if (fstat(fd, &status) != 0 || !S_ISDIR(status.st_mode)) {
      int saved_errno = errno == 0 ? ENOTDIR : errno;
      (void)close(fd);
      errno = saved_errno;
      return -1;
    }
  }
  return fd;
}

static int open_directory_path(const char *path) {
  char copy[PATH_MAX];
  char *cursor;
  int current_fd;
  size_t length;
  bool absolute;

  if (path == NULL || path[0] == '\0') {
    errno = EINVAL;
    return -1;
  }
  length = strnlen(path, sizeof copy);
  if (length == 0u || length >= sizeof copy) {
    errno = ENAMETOOLONG;
    return -1;
  }
  memcpy(copy, path, length + 1u);
  absolute = copy[0] == '/';
  current_fd = open(absolute ? "/" : ".",
                    O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
  if (current_fd < 0) return -1;
  cursor = copy + (absolute ? 1 : 0);
  while (*cursor != '\0') {
    char *component = cursor;
    char *slash = strchr(cursor, '/');
    size_t component_length;
    int next_fd;
    if (slash != NULL) {
      *slash = '\0';
      cursor = slash + 1;
    } else {
      cursor += strlen(cursor);
    }
    component_length = strlen(component);
    if (component_length == 0u) continue;
    if ((component_length == 1u && component[0] == '.') ||
        (component_length == 2u && component[0] == '.' &&
         component[1] == '.')) {
      (void)close(current_fd);
      errno = EINVAL;
      return -1;
    }
    next_fd = open_directory_nofollow(current_fd, component);
    if (next_fd < 0) {
      int saved_errno = errno;
      (void)close(current_fd);
      errno = saved_errno;
      return -1;
    }
    (void)close(current_fd);
    current_fd = next_fd;
  }
  return current_fd;
}

static bool split_target(const char *path, int *parent_fd_out,
                         char target_name_out[NAME_MAX + 1u]) {
  char copy[PATH_MAX];
  char *slash;
  const char *parent_path;
  const char *base;
  size_t length;
  int parent_fd;

  if (path == NULL || parent_fd_out == NULL || target_name_out == NULL) {
    errno = EINVAL;
    return false;
  }
  length = strnlen(path, sizeof copy);
  if (length == 0u || length >= sizeof copy) {
    errno = ENAMETOOLONG;
    return false;
  }
  memcpy(copy, path, length + 1u);
  slash = strrchr(copy, '/');
  if (slash == NULL) {
    parent_path = ".";
    base = copy;
  } else {
    base = slash + 1;
    if (slash == copy) {
      parent_path = "/";
    } else {
      *slash = '\0';
      parent_path = copy;
    }
  }
  length = strlen(base);
  if (length == 0u || length > NAME_MAX ||
      (length == 1u && base[0] == '.') ||
      (length == 2u && base[0] == '.' && base[1] == '.')) {
    errno = EINVAL;
    return false;
  }
  memcpy(target_name_out, base, length + 1u);
  parent_fd = open_directory_path(parent_path);
  if (parent_fd < 0) return false;
  *parent_fd_out = parent_fd;
  return true;
}

static int rename_noreplace(int old_directory_fd, const char *old_name,
                            int new_directory_fd, const char *new_name) {
#if defined(__APPLE__)
  return renameatx_np(old_directory_fd, old_name, new_directory_fd, new_name,
                      RENAME_EXCL);
#elif defined(__linux__) && defined(SYS_renameat2) && defined(RENAME_NOREPLACE)
  return (int)syscall(SYS_renameat2, old_directory_fd, old_name,
                      new_directory_fd, new_name, RENAME_NOREPLACE);
#else
  (void)old_directory_fd;
  (void)old_name;
  (void)new_directory_fd;
  (void)new_name;
  errno = ENOTSUP;
  return -1;
#endif
}

static bool make_stage_name(t64_raw_capture *capture, const char *kind,
                            unsigned attempt,
                            char name_out[RAW_CAPTURE_STAGE_CAPACITY]) {
  struct timespec now;
  int length;
  if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return false;
  length = snprintf(name_out, RAW_CAPTURE_STAGE_CAPACITY,
                    ".t64-%s-%ld-%llx-%u", kind, (long)getpid(),
                    (unsigned long long)now.tv_nsec, attempt);
  (void)capture;
  return length > 0 && (size_t)length < RAW_CAPTURE_STAGE_CAPACITY;
}

static bool same_directory_entry(int directory_fd, const char *name,
                                 dev_t device, ino_t inode) {
  struct stat status;
  return fstatat(directory_fd, name, &status, AT_SYMLINK_NOFOLLOW) == 0 &&
         S_ISDIR(status.st_mode) && status.st_dev == device &&
         status.st_ino == inode;
}

static bool create_stage(t64_raw_capture *capture) {
  unsigned attempt;
  for (attempt = 0u; attempt < 32u; attempt++) {
    struct stat created_status;
    struct stat opened_status;
    if (!make_stage_name(capture, "stage", attempt, capture->stage_name)) {
      capture->error = "capture-stage-name";
      return false;
    }
    if (mkdirat(capture->parent_fd, capture->stage_name, 0700) != 0) {
      if (errno == EEXIST) continue;
      capture->error = "capture-stage-create";
      return false;
    }
    if (fstatat(capture->parent_fd, capture->stage_name, &created_status,
                AT_SYMLINK_NOFOLLOW) != 0 || !S_ISDIR(created_status.st_mode) ||
        created_status.st_uid != geteuid()) {
      capture->error = "capture-stage-identity";
      return false;
    }
    capture->stage_device = created_status.st_dev;
    capture->stage_inode = created_status.st_ino;
    capture->stage_fd = open_directory_nofollow(capture->parent_fd,
                                                 capture->stage_name);
    if (capture->stage_fd < 0 || fchmod(capture->stage_fd, 0700) != 0 ||
        fstat(capture->stage_fd, &opened_status) != 0 ||
        !S_ISDIR(opened_status.st_mode) ||
        opened_status.st_uid != geteuid() ||
        opened_status.st_dev != capture->stage_device ||
        opened_status.st_ino != capture->stage_inode ||
        !same_directory_entry(capture->parent_fd, capture->stage_name,
                              opened_status.st_dev, opened_status.st_ino)) {
      capture->error = "capture-stage-open";
      return false;
    }
    return true;
  }
  capture->error = "capture-stage-collision";
  return false;
}

static bool probe_noreplace(t64_raw_capture *capture) {
  unsigned attempt;
  for (attempt = 0u; attempt < 32u; attempt++) {
    char probe_name[RAW_CAPTURE_STAGE_CAPACITY];
    if (!make_stage_name(capture, "probe", attempt, probe_name)) {
      capture->error = "capture-probe-name";
      return false;
    }
    if (rename_noreplace(capture->parent_fd, capture->stage_name,
                         capture->parent_fd, probe_name) == 0) {
      int length = snprintf(capture->stage_name, sizeof capture->stage_name,
                            "%s", probe_name);
      if (length <= 0 || (size_t)length >= sizeof capture->stage_name ||
          !same_directory_entry(capture->parent_fd, capture->stage_name,
                                capture->stage_device,
                                capture->stage_inode)) {
        capture->error = "capture-probe-identity";
        return false;
      }
      return true;
    }
    if (errno != EEXIST) {
      capture->error = "capture-no-replace-unsupported";
      return false;
    }
  }
  capture->error = "capture-probe-collision";
  return false;
}

static bool create_child_directory(int parent_fd, const char *name,
                                   int *child_fd_out, dev_t *device_out,
                                   ino_t *inode_out) {
  struct stat created_status;
  struct stat status;
  int child_fd;
  if (mkdirat(parent_fd, name, 0700) != 0) return false;
  if (fstatat(parent_fd, name, &created_status, AT_SYMLINK_NOFOLLOW) != 0 ||
      !S_ISDIR(created_status.st_mode)) {
    return false;
  }
  *device_out = created_status.st_dev;
  *inode_out = created_status.st_ino;
  child_fd = open_directory_nofollow(parent_fd, name);
  if (child_fd < 0 || fstat(child_fd, &status) != 0 ||
      !S_ISDIR(status.st_mode) || status.st_dev != *device_out ||
      status.st_ino != *inode_out ||
      !same_directory_entry(parent_fd, name, status.st_dev, status.st_ino)) {
    if (child_fd >= 0) (void)close(child_fd);
    return false;
  }
  *child_fd_out = child_fd;
  if (fchmod(child_fd, 0700) != 0) return false;
  return true;
}

static bool regular_file_matches(int directory_fd, const char *name,
                                 uint64_t expected_length,
                                 dev_t expected_device,
                                 ino_t expected_inode) {
  struct stat status;
  if (expected_length > (uint64_t)INT64_MAX ||
      fstatat(directory_fd, name, &status, AT_SYMLINK_NOFOLLOW) != 0 ||
      !S_ISREG(status.st_mode) || status.st_nlink != 1 ||
      status.st_dev != expected_device || status.st_ino != expected_inode ||
      status.st_size < 0 || (uint64_t)status.st_size != expected_length) {
    return false;
  }
  return true;
}

static bool write_all(int fd, const uint8_t *bytes, size_t length) {
  size_t offset = 0u;
  while (offset < length) {
    size_t remaining = length - offset;
    ssize_t count = write(fd, bytes + offset, remaining);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) return false;
    offset += (size_t)count;
  }
  return true;
}

static bool write_packet_file(t64_raw_capture *capture, int directory_fd,
                              const char *name, const uint8_t *bytes,
                              size_t length, dev_t *device_out,
                              ino_t *inode_out) {
  int fd;
  struct stat status;
  uint64_t next_total;
  if (capture == NULL || name == NULL || bytes == NULL || length == 0u ||
      !checked_add_u64(capture->stream_bytes, (uint64_t)length,
                       &next_total) || next_total > capture->byte_cap) {
    if (capture != NULL) capture->error = "capture-cap";
    return false;
  }
  fd = openat(directory_fd, name,
              O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
  if (fd < 0) {
    capture->error = "capture-file-create";
    return false;
  }
  if (fstat(fd, &status) != 0 || !S_ISREG(status.st_mode) ||
      status.st_nlink != 1 || fchmod(fd, 0600) != 0) {
    if (fstatat(directory_fd, name, &status, AT_SYMLINK_NOFOLLOW) == 0 &&
        S_ISREG(status.st_mode) && status.st_nlink == 1) {
      *device_out = status.st_dev;
      *inode_out = status.st_ino;
    }
    capture->error = "capture-file-open";
    (void)close(fd);
    return false;
  }
  *device_out = status.st_dev;
  *inode_out = status.st_ino;
  if (!write_all(fd, bytes, length)) {
    capture->error = "capture-write";
    (void)close(fd);
    return false;
  }
  if (fstat(fd, &status) != 0 || status.st_size < 0 ||
      (uint64_t)status.st_size != (uint64_t)length ||
      fsync(fd) != 0) {
    capture->error = "capture-file-flush";
    (void)close(fd);
    return false;
  }
  if (close(fd) != 0) {
    capture->error = "capture-close";
    return false;
  }
  if (!regular_file_matches(directory_fd, name, (uint64_t)length,
                            *device_out, *inode_out)) {
    capture->error = "capture-file-length";
    return false;
  }
  capture->stream_bytes = next_total;
  return true;
}

static void json_append(raw_json *json, const char *bytes, size_t length) {
  if (json->failed || bytes == NULL || length > json->capacity - json->length) {
    json->failed = true;
    return;
  }
  memcpy(json->bytes + json->length, bytes, length);
  json->length += length;
}

static void json_literal(raw_json *json, const char *literal) {
  json_append(json, literal, strlen(literal));
}

static void json_string(raw_json *json, const char *value) {
  const unsigned char *cursor = (const unsigned char *)value;
  if (value == NULL) {
    json->failed = true;
    return;
  }
  json_literal(json, "\"");
  while (*cursor != 0u && !json->failed) {
    char escaped[7];
    if (*cursor == '"' || *cursor == '\\') {
      escaped[0] = '\\';
      escaped[1] = (char)*cursor;
      json_append(json, escaped, 2u);
    } else if (*cursor < 0x20u) {
      int length = snprintf(escaped, sizeof escaped, "\\u%04x",
                            (unsigned)*cursor);
      if (length != 6) json->failed = true;
      else json_append(json, escaped, 6u);
    } else {
      escaped[0] = (char)*cursor;
      json_append(json, escaped, 1u);
    }
    cursor++;
  }
  json_literal(json, "\"");
}

static void json_u64_string(raw_json *json, uint64_t value) {
  char number[32];
  int length = snprintf(number, sizeof number, "%" PRIu64, value);
  if (length <= 0 || (size_t)length >= sizeof number) {
    json->failed = true;
    return;
  }
  json_string(json, number);
}

static void json_text_field(raw_json *json, const char *key,
                            const char *value) {
  json_string(json, key);
  json_literal(json, ":");
  json_string(json, value);
}

static void json_u64_field(raw_json *json, const char *key, uint64_t value) {
  json_string(json, key);
  json_literal(json, ":");
  json_u64_string(json, value);
}

static void json_u32_field(raw_json *json, const char *key, uint32_t value) {
  json_u64_field(json, key, (uint64_t)value);
}

static bool append_manifest(t64_raw_capture *capture, uint64_t guest_tick,
                            const char *outcome, const char *guest_stop_kind,
                            uint64_t manifest_length,
                            uint8_t **bytes_out, size_t *length_out) {
  char *buffer = NULL;
  raw_json json;
  uint64_t total_bytes;
  size_t i;
  if (!checked_add_u64(capture->stream_bytes, manifest_length,
                       &total_bytes) || total_bytes > capture->byte_cap) {
    capture->error = "capture-cap";
    return false;
  }
  buffer = (char *)malloc(RAW_CAPTURE_MANIFEST_MAX);
  if (buffer == NULL) {
    capture->error = "capture-manifest-memory";
    return false;
  }
  json.bytes = buffer;
  json.capacity = RAW_CAPTURE_MANIFEST_MAX;
  json.length = 0u;
  json.failed = false;
  json_literal(&json, "{\"schema\":\"t64.raw-av-capture\",\"schema_version\":1,");
  json_text_field(&json, "profile_id", capture->profile_id);
  json_literal(&json, ","); json_text_field(&json, "profile_sha256", capture->profile_sha256);
  json_literal(&json, ","); json_u32_field(&json, "descriptor_version", capture->descriptor_version);
  json_literal(&json, ","); json_u32_field(&json, "machine_model_version", capture->machine_model_version);
  json_literal(&json, ","); json_u32_field(&json, "state_serialization_version", capture->state_serialization_version);
  json_literal(&json, ","); json_text_field(&json, "core_build_id", capture->core_build_id);
  json_literal(&json, ","); json_text_field(&json, "runner_build_id", capture->runner_build_id);
  json_literal(&json, ","); json_text_field(&json, "source_revision", capture->source_revision);
  json_literal(&json, ","); json_text_field(&json, "content_sha256", capture->content_sha256);
  json_literal(&json, ","); json_u64_field(&json, "content_size", capture->content_size);
  json_literal(&json, ","); json_text_field(&json, "fixture_id", capture->fixture_id);
  json_literal(&json, ","); json_text_field(&json, "fixture_sha256", capture->fixture_sha256);
  json_literal(&json, ","); json_u64_field(&json, "fixture_size", capture->fixture_size);
  json_literal(&json, ","); json_text_field(&json, "input_file_sha256", capture->input_file_sha256);
  json_literal(&json, ","); json_text_field(&json, "input_digest", capture->input_digest);
  json_literal(&json, ","); json_u64_field(&json, "input_size", capture->input_size);
  json_literal(&json, ","); json_u64_field(&json, "terminal_guest_tick", guest_tick);
  json_literal(&json, ","); json_text_field(&json, "terminal_outcome", outcome);
  json_literal(&json, ","); json_text_field(&json, "terminal_stop_kind", guest_stop_kind);
  json_literal(&json, ","); json_u64_field(&json, "video_count", (uint64_t)capture->video_count);
  json_literal(&json, ","); json_u64_field(&json, "audio_count", (uint64_t)capture->audio_count);
  json_literal(&json, ","); json_u64_field(&json, "data_bytes", capture->stream_bytes);
  json_literal(&json, ","); json_u64_field(&json, "manifest_bytes", manifest_length);
  json_literal(&json, ","); json_u64_field(&json, "total_bytes", total_bytes);
  json_literal(&json, ",\"video\":[");
  for (i = 0u; i < capture->video_count; i++) {
    const raw_video_entry *entry = &capture->video[i];
    const t64_video_packet_info *info = &entry->info;
    if (i != 0u) json_literal(&json, ",");
    json_literal(&json, "{");
    json_text_field(&json, "file", entry->name);
    json_literal(&json, ","); json_u64_field(&json, "sequence", info->sequence);
    json_literal(&json, ","); json_u64_field(&json, "first_tick", info->start_tick);
    json_literal(&json, ","); json_u64_field(&json, "last_tick", info->end_tick);
    json_literal(&json, ","); json_u32_field(&json, "width", info->width);
    json_literal(&json, ","); json_u32_field(&json, "height", info->height);
    json_literal(&json, ","); json_u32_field(&json, "stride_bytes", info->stride_bytes);
    json_literal(&json, ",\"encoding\":\"rgba8\",");
    json_u64_field(&json, "byte_length", entry->byte_length);
    json_literal(&json, "}");
  }
  json_literal(&json, "],\"audio\":[");
  for (i = 0u; i < capture->audio_count; i++) {
    const raw_audio_entry *entry = &capture->audio[i];
    const t64_audio_packet_info *info = &entry->info;
    if (i != 0u) json_literal(&json, ",");
    json_literal(&json, "{");
    json_text_field(&json, "file", entry->name);
    json_literal(&json, ","); json_u64_field(&json, "sequence", info->sequence);
    json_literal(&json, ","); json_u64_field(&json, "first_sample_index", info->first_sample_index);
    json_literal(&json, ","); json_u32_field(&json, "frame_count", info->frame_count);
    json_literal(&json, ","); json_u64_field(&json, "rate_numerator", info->rate_numerator);
    json_literal(&json, ","); json_u64_field(&json, "rate_denominator", info->rate_denominator);
    json_literal(&json, ","); json_u64_field(&json, "first_tick", info->first_tick);
    json_literal(&json, ","); json_u64_field(&json, "last_tick", info->last_tick);
    json_literal(&json, ",\"encoding\":\"s16be-interleaved-stereo\",");
    json_u32_field(&json, "channels", info->channels);
    json_literal(&json, ","); json_u32_field(&json, "bits_per_sample", info->bits_per_sample);
    json_literal(&json, ","); json_u64_field(&json, "byte_length", entry->byte_length);
    json_literal(&json, "}");
  }
  json_literal(&json, "]}\n");
  if (json.failed || json.length > RAW_CAPTURE_MANIFEST_MAX) {
    free(buffer);
    capture->error = "capture-manifest-size";
    return false;
  }
  *bytes_out = (uint8_t *)buffer;
  *length_out = json.length;
  return true;
}

static bool valid_video_info(const t64_raw_capture *capture,
                             const t64_video_packet_info *info,
                             size_t length, const char **error_out) {
  uint64_t minimum_stride;
  uint64_t expected_bytes;
  if (info == NULL) {
    if (error_out != NULL) *error_out = "capture-video-metadata-missing";
    return false;
  }
  if (info->width == 0u || info->height == 0u || info->stride_bytes == 0u ||
      info->width > UINT32_MAX / 4u ||
      info->stride_bytes < info->width * 4u ||
      info->output_encoding != T64_VIDEO_ENCODING_RGBA8 ||
      info->packet_bytes != length || length == 0u ||
      info->start_tick > info->end_tick) {
    if (error_out != NULL) *error_out = "capture-video-metadata-shape";
    return false;
  }
  if ((uint64_t)info->stride_bytes >
      UINT64_MAX / (uint64_t)info->height) {
    if (error_out != NULL) *error_out = "capture-video-overflow";
    return false;
  }
  minimum_stride = (uint64_t)info->width * UINT64_C(4);
  expected_bytes = (uint64_t)info->stride_bytes * (uint64_t)info->height;
  if (minimum_stride > info->stride_bytes || expected_bytes != length) {
    if (error_out != NULL) *error_out = "capture-video-length";
    return false;
  }
  if (strcmp(info->profile_id, capture->profile_id) != 0) {
    if (error_out != NULL) *error_out = "capture-video-profile";
    return false;
  }
  if (strcmp(info->build_id, capture->core_build_id) != 0) {
    if (error_out != NULL) *error_out = "capture-video-build";
    return false;
  }
  return true;
}

static bool valid_audio_info(const t64_raw_capture *capture,
                             const t64_audio_packet_info *info,
                             size_t length) {
  uint64_t expected_bytes;
  if (info == NULL || info->struct_size != (uint32_t)sizeof *info ||
      info->sequence == 0u || info->frame_count == 0u ||
      info->rate_numerator == 0u || info->rate_denominator == 0u ||
      info->channels != 2u || info->bits_per_sample != 16u ||
      info->encoding != T64_AUDIO_ENCODING_S16BE ||
      info->byte_length != length || info->first_tick > info->last_tick ||
      (uint64_t)info->frame_count > UINT64_MAX / UINT64_C(4)) {
    return false;
  }
  expected_bytes = (uint64_t)info->frame_count * UINT64_C(4);
  return expected_bytes == length &&
         strcmp(info->profile_id, capture->profile_id) == 0 &&
         strcmp(info->build_id, capture->core_build_id) == 0;
}

static bool write_manifest_file(t64_raw_capture *capture, uint64_t guest_tick,
                                const char *outcome,
                                const char *guest_stop_kind) {
  uint64_t guess = 0u;
  unsigned attempt;
  uint8_t *manifest = NULL;
  size_t manifest_length = 0u;
  for (attempt = 0u; attempt < 8u; attempt++) {
    uint64_t current_guess = guess;
    if (!append_manifest(capture, guest_tick, outcome, guest_stop_kind,
                         current_guess, &manifest, &manifest_length)) {
      return false;
    }
    if ((uint64_t)manifest_length == current_guess) break;
    free(manifest);
    manifest = NULL;
    guess = (uint64_t)manifest_length;
  }
  if (manifest == NULL || (uint64_t)manifest_length != guess) {
    free(manifest);
    capture->error = "capture-manifest-accounting";
    return false;
  }
  {
    int fd = openat(capture->stage_fd, "manifest.json",
                    O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
                    0600);
    struct stat status;
    if (fd < 0) {
      free(manifest);
      capture->error = "capture-manifest-create";
      return false;
    }
    if (fstat(fd, &status) != 0 || !S_ISREG(status.st_mode) ||
        status.st_nlink != 1) {
      if (fstatat(capture->stage_fd, "manifest.json", &status,
                  AT_SYMLINK_NOFOLLOW) == 0 && S_ISREG(status.st_mode) &&
          status.st_nlink == 1) {
        capture->manifest_device = status.st_dev;
        capture->manifest_inode = status.st_ino;
      }
      free(manifest);
      capture->error = "capture-manifest-open";
      (void)close(fd);
      return false;
    }
    capture->manifest_device = status.st_dev;
    capture->manifest_inode = status.st_ino;
    if (fchmod(fd, 0600) != 0 || !write_all(fd, manifest, manifest_length) ||
        fstat(fd, &status) != 0 || status.st_size < 0 ||
        (uint64_t)status.st_size != (uint64_t)manifest_length ||
        fsync(fd) != 0) {
      free(manifest);
      capture->error = "capture-manifest-write";
      (void)close(fd);
      return false;
    }
    free(manifest);
    if (close(fd) != 0) {
      capture->error = "capture-close";
      return false;
    }
  }
  if (!regular_file_matches(capture->stage_fd, "manifest.json",
                            (uint64_t)manifest_length,
                            capture->manifest_device,
                            capture->manifest_inode)) {
    capture->error = "capture-manifest-length";
    return false;
  }
  return true;
}

static bool validate_stream_files(t64_raw_capture *capture) {
  size_t i;
  if (capture->video_count == 0u || capture->audio_count == 0u) {
    capture->error = "capture-incomplete-streams";
    return false;
  }
  for (i = 0u; i < capture->video_count; i++) {
    const raw_video_entry *entry = &capture->video[i];
    if (entry->byte_length == 0u ||
        !regular_file_matches(capture->video_fd, entry->name,
                              entry->byte_length, entry->device,
                              entry->inode)) {
      capture->error = "capture-video-incomplete";
      return false;
    }
  }
  for (i = 0u; i < capture->audio_count; i++) {
    const raw_audio_entry *entry = &capture->audio[i];
    if (entry->byte_length == 0u ||
        !regular_file_matches(capture->audio_fd, entry->name,
                              entry->byte_length, entry->device,
                              entry->inode)) {
      capture->error = "capture-audio-incomplete";
      return false;
    }
  }
  return true;
}

static bool remove_owned_file(int directory_fd, const char *name,
                              dev_t device, ino_t inode) {
  struct stat status;
  if (directory_fd < 0 || name == NULL ||
      fstatat(directory_fd, name, &status, AT_SYMLINK_NOFOLLOW) != 0) {
    return false;
  }
  if (status.st_dev != device || status.st_ino != inode ||
      !S_ISREG(status.st_mode)) {
    return false;
  }
  return unlinkat(directory_fd, name, 0) == 0;
}

void t64_raw_capture_abort(t64_raw_capture *capture) {
  size_t i;
  struct stat stage_status;
  if (capture == NULL || capture->committed) return;
  for (i = 0u; i < capture->video_count; i++) {
    (void)remove_owned_file(capture->video_fd, capture->video[i].name,
                            capture->video[i].device,
                            capture->video[i].inode);
  }
  for (i = 0u; i < capture->audio_count; i++) {
    (void)remove_owned_file(capture->audio_fd, capture->audio[i].name,
                            capture->audio[i].device,
                            capture->audio[i].inode);
  }
  if (capture->stage_fd >= 0) {
    struct stat status;
    if (capture->video_inode != 0u &&
        fstatat(capture->stage_fd, "video", &status,
                AT_SYMLINK_NOFOLLOW) == 0 &&
        S_ISDIR(status.st_mode) && status.st_dev == capture->video_device &&
        status.st_ino == capture->video_inode) {
      (void)unlinkat(capture->stage_fd, "video", AT_REMOVEDIR);
    }
    if (capture->audio_inode != 0u &&
        fstatat(capture->stage_fd, "audio", &status,
                AT_SYMLINK_NOFOLLOW) == 0 &&
        S_ISDIR(status.st_mode) && status.st_dev == capture->audio_device &&
        status.st_ino == capture->audio_inode) {
      (void)unlinkat(capture->stage_fd, "audio", AT_REMOVEDIR);
    }
    if (capture->manifest_inode != 0u)
      (void)remove_owned_file(capture->stage_fd, "manifest.json",
                              capture->manifest_device,
                              capture->manifest_inode);
  }
  if (capture->parent_fd >= 0 && capture->stage_name[0] != '\0' &&
      fstatat(capture->parent_fd, capture->stage_name, &stage_status,
              AT_SYMLINK_NOFOLLOW) == 0 &&
      S_ISDIR(stage_status.st_mode) &&
      stage_status.st_dev == capture->stage_device &&
      stage_status.st_ino == capture->stage_inode) {
    (void)unlinkat(capture->parent_fd, capture->stage_name, AT_REMOVEDIR);
  }
}

void t64_raw_capture_destroy(t64_raw_capture *capture) {
  if (capture == NULL) return;
#ifdef T64_REAL_RUNNER_TESTING
  if (!capture->committed && !capture->preserve_stage_for_test)
#else
  if (!capture->committed)
#endif
    t64_raw_capture_abort(capture);
  if (capture->video_fd >= 0) (void)close(capture->video_fd);
  if (capture->audio_fd >= 0) (void)close(capture->audio_fd);
  if (capture->stage_fd >= 0) (void)close(capture->stage_fd);
  if (capture->parent_fd >= 0) (void)close(capture->parent_fd);
  free(capture);
}

const char *t64_raw_capture_error(const t64_raw_capture *capture) {
  return capture == NULL || capture->error == NULL ? "capture-error"
                                                   : capture->error;
}

uint64_t t64_raw_capture_remaining_bytes(const t64_raw_capture *capture) {
  return capture == NULL || capture->stream_bytes >= capture->byte_cap
             ? 0u
             : capture->byte_cap - capture->stream_bytes;
}

size_t t64_raw_capture_audio_count(const t64_raw_capture *capture) {
  return capture == NULL ? 0u : capture->audio_count;
}

#ifdef T64_REAL_RUNNER_TESTING
void t64_raw_capture_preserve_stage_for_test(t64_raw_capture *capture) {
  if (capture != NULL && !capture->committed) {
    capture->preserve_stage_for_test = true;
  }
}
#endif

t64_raw_capture *t64_raw_capture_open(
    const char *target_path, uint64_t byte_cap,
    const t64_raw_capture_identity *identity, const char **error_out) {
  t64_raw_capture *capture;
  struct stat target_status;
  if (error_out != NULL) *error_out = "capture-invalid";
  if (target_path == NULL || identity == NULL || byte_cap == 0u ||
      byte_cap > T64_RAW_CAPTURE_MAX_BYTES) {
    return NULL;
  }
  capture = (t64_raw_capture *)calloc(1u, sizeof *capture);
  if (capture == NULL) {
    if (error_out != NULL) *error_out = "capture-memory";
    return NULL;
  }
  capture->parent_fd = -1;
  capture->stage_fd = -1;
  capture->video_fd = -1;
  capture->audio_fd = -1;
  capture->byte_cap = byte_cap;
  if (!copy_identity(capture->profile_id, sizeof capture->profile_id,
                     identity->profile_id) ||
      !copy_identity(capture->profile_sha256,
                     sizeof capture->profile_sha256,
                     identity->profile_sha256) ||
      !copy_identity(capture->core_build_id,
                     sizeof capture->core_build_id,
                     identity->core_build_id) ||
      !copy_identity(capture->runner_build_id,
                     sizeof capture->runner_build_id,
                     identity->runner_build_id) ||
      !copy_identity(capture->source_revision,
                     sizeof capture->source_revision,
                     identity->source_revision) ||
      !copy_identity(capture->content_sha256,
                     sizeof capture->content_sha256,
                     identity->content_sha256) ||
      !copy_identity(capture->fixture_id, sizeof capture->fixture_id,
                     identity->fixture_id) ||
      !copy_identity(capture->fixture_sha256,
                     sizeof capture->fixture_sha256,
                     identity->fixture_sha256) ||
      !copy_identity(capture->input_file_sha256,
                     sizeof capture->input_file_sha256,
                     identity->input_file_sha256) ||
      !copy_identity(capture->input_digest, sizeof capture->input_digest,
                     identity->input_digest)) {
    capture->error = "capture-identity";
    goto fail;
  }
  capture->descriptor_version = identity->descriptor_version;
  capture->machine_model_version = identity->machine_model_version;
  capture->state_serialization_version = identity->state_serialization_version;
  capture->content_size = identity->content_size;
  capture->fixture_size = identity->fixture_size;
  capture->input_size = identity->input_size;
  if (!split_target(target_path, &capture->parent_fd,
                    capture->target_name)) {
    capture->error = "capture-target-parent";
    goto fail;
  }
  if (fstatat(capture->parent_fd, capture->target_name, &target_status,
              AT_SYMLINK_NOFOLLOW) == 0) {
    capture->error = "capture-target-exists";
    goto fail;
  }
  if (errno != ENOENT) {
    capture->error = "capture-target-stat";
    goto fail;
  }
  if (!create_stage(capture) || !probe_noreplace(capture)) goto fail;
  if (!create_child_directory(capture->stage_fd, "video",
                              &capture->video_fd, &capture->video_device,
                              &capture->video_inode) ||
      !create_child_directory(capture->stage_fd, "audio",
                              &capture->audio_fd, &capture->audio_device,
                              &capture->audio_inode)) {
    capture->error = "capture-stage-subdirectory";
    goto fail;
  }
  if (error_out != NULL) *error_out = NULL;
  return capture;

fail:
  if (error_out != NULL) *error_out = t64_raw_capture_error(capture);
  t64_raw_capture_destroy(capture);
  return NULL;
}

bool t64_raw_capture_write_video(t64_raw_capture *capture,
                                 const t64_video_packet_info *info,
                                 const uint8_t *bytes, size_t length) {
  raw_video_entry *entry;
  int name_length;
  const char *metadata_error = NULL;
  if (capture == NULL || capture->committed ||
      capture->video_count >= RAW_CAPTURE_MAX_PACKETS ||
      !valid_video_info(capture, info, length, &metadata_error) ||
      bytes == NULL) {
    if (capture != NULL) {
      capture->error = metadata_error == NULL ? "capture-video-metadata"
                                              : metadata_error;
    }
    return false;
  }
  if (capture->video_count != 0u &&
      info->sequence <= capture->video[capture->video_count - 1u]
                            .info.sequence) {
    capture->error = "capture-video-sequence";
    return false;
  }
  entry = &capture->video[capture->video_count++];
  name_length = snprintf(entry->name, sizeof entry->name,
                         "frame-%06zu.rgba8", capture->video_count - 1u);
  if (name_length <= 0 || (size_t)name_length >= sizeof entry->name) {
    capture->error = "capture-video-name";
    return false;
  }
  entry->info = *info;
  entry->byte_length = (uint64_t)length;
  if (!write_packet_file(capture, capture->video_fd, entry->name, bytes,
                         length, &entry->device, &entry->inode)) {
    return false;
  }
  return true;
}

bool t64_raw_capture_write_audio(t64_raw_capture *capture,
                                 const t64_audio_packet_info *info,
                                 const uint8_t *bytes, size_t length) {
  raw_audio_entry *entry;
  int name_length;
  if (capture == NULL || capture->committed ||
      capture->audio_count >= RAW_CAPTURE_MAX_PACKETS ||
      !valid_audio_info(capture, info, length) || bytes == NULL) {
    if (capture != NULL) capture->error = "capture-audio-metadata";
    return false;
  }
  if (capture->audio_count != 0u &&
      info->sequence <= capture->audio[capture->audio_count - 1u]
                            .info.sequence) {
    capture->error = "capture-audio-sequence";
    return false;
  }
  entry = &capture->audio[capture->audio_count++];
  name_length = snprintf(entry->name, sizeof entry->name,
                         "packet-%06zu.s16be", capture->audio_count - 1u);
  if (name_length <= 0 || (size_t)name_length >= sizeof entry->name) {
    capture->error = "capture-audio-name";
    return false;
  }
  entry->info = *info;
  entry->byte_length = (uint64_t)length;
  if (!write_packet_file(capture, capture->audio_fd, entry->name, bytes,
                         length, &entry->device, &entry->inode)) {
    return false;
  }
  return true;
}

bool t64_raw_capture_finalize(t64_raw_capture *capture, uint64_t guest_tick,
                              const char *outcome,
                              const char *guest_stop_kind) {
  if (capture == NULL || capture->committed || outcome == NULL ||
      guest_stop_kind == NULL || capture->video_count == 0u ||
      capture->audio_count == 0u) {
    if (capture != NULL) capture->error = "capture-incomplete-streams";
    return false;
  }
  if (!validate_stream_files(capture) ||
      !write_manifest_file(capture, guest_tick, outcome, guest_stop_kind) ||
      fsync(capture->video_fd) != 0 || fsync(capture->audio_fd) != 0 ||
      fsync(capture->stage_fd) != 0 || fsync(capture->parent_fd) != 0) {
    if (capture->error == NULL) capture->error = "capture-directory-flush";
    return false;
  }
  if (!same_directory_entry(capture->parent_fd, capture->stage_name,
                            capture->stage_device, capture->stage_inode)) {
    capture->error = "capture-stage-replaced";
    return false;
  }
  if (rename_noreplace(capture->parent_fd, capture->stage_name,
                       capture->parent_fd, capture->target_name) != 0) {
    capture->error = "capture-no-replace-commit";
    return false;
  }
  capture->committed = true;
  return true;
}
