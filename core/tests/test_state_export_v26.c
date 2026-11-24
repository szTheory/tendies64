/*
 * Phase 10 state container export tests for state 26 (plan 10-11).
 *
 * The version marker of the state 26 bump is the container version word at
 * 0x008: 2 for state 26, 1 for state 25, paired with the state word at 0x00C.
 * The exporter writes it, the importer's header check compares it with the
 * state word of the same header, and the independent decoder
 * fixtures/state/decode_container_v2.py (a separate script, because the
 * version 1 decoder is a pinned Phase 9 oracle) decodes both versions.
 *
 * Offsets below are literal copies of docs/spec/state-container.md; the
 * expected sizes are derived by hand in the comments (the version 25 size of
 * the same builder on the matching si-v1 id plus the state 26 additions), never
 * read back from a production constant. Blobs are private artifacts: they are
 * written 0600 under the evidence directory of this test and removed again,
 * and nothing is committed.
 *
 * Cases:
 *   export-v26-idle    header, table, derived idle sizes, decoder acceptance,
 *                      the version/state pairing in the decoder and the importer
 *   export-v26-bounds  mid Z-triangle and retained-packet exports, the derived
 *                      v26 upper bound, truncation and extension, exact depth
 *                      scalars and hidden table
 *
 * usage: test_state_export_v26 [--list-cases|--case NAME]
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#if defined(__APPLE__)
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif
#elif !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "state_import_helpers.h"

#define V1_STEM SI_STEM_ID
#define V2_STEM "ntsc-cart-4m-openipl3-rdram-b-t1-si-v2"
#define FAMILY_COUNT 4u

/* State 26 adds to the state 25 container, all inside section 8: the five
 * depth scalars (5 x 4 = 20 bytes), the 4-byte hidden table length field and
 * the hidden table itself (RDRAM 4,194,304 / 2 = 2,097,152 bytes). */
#define V26_EXTRA_BYTES (20u + 4u + 2097152u)
#define HIDDEN_BYTES 2097152u
#define COVERAGE_BYTES 2097152u
/* Section 8 body offsets of the version 2 layout (decode_container_v2.py). */
#define S8_PENDING_COUNT 196u
#define S8_PENDING_WORDS 200u
#define S8_TRIANGLE_TAIL 28u
#define S8_TRIANGLE_COVERAGE 2097152u
/* v26 upper bound: idle maximum + 2 video packets of 320 x 240 x 4 = 307,200
 * + 2 audio packets of 262,136, rounded up to a whole MiB. */
#define VIDEO_PACKET_MAX 307200u
#define AUDIO_PACKET_MAX 262136u
#define V26_UPPER_BOUND 12582912u
#define MIB 1048576u

typedef struct {
  const char *v25_id;
  const char *v26_id;
  size_t table_count;
  const uint32_t *ids;
  /* The v25 idle size of the builder (docs/spec/state-container.md "Measured
   * sizes") and the hand-derived v26 idle size: v25 + 2,097,176. */
  size_t v25_idle;
  size_t v26_idle;
} family_row;

static const uint32_t stem_ids[11] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u,
                                      9u, 10u, 16u};
static const uint32_t save_ids[12] = {1u, 2u, 3u, 4u, 5u,  6u,
                                      7u, 8u, 9u, 10u, 11u, 16u};

/* v25 idle 8,448,795 / 8,449,387 / 8,450,923 / 8,481,643; each plus 2,097,176
 * is 10,545,971 / 10,546,563 / 10,548,099 / 10,578,819. */
static const family_row family[FAMILY_COUNT] = {
    {V1_STEM, V2_STEM, 11u, stem_ids, 8448795u, 10545971u},
    {V1_STEM "-e4k", V2_STEM "-e4k", 12u, save_ids, 8449387u, 10546563u},
    {V1_STEM "-e16k", V2_STEM "-e16k", 12u, save_ids, 8450923u, 10548099u},
    {V1_STEM "-s32k", V2_STEM "-s32k", 12u, save_ids, 8481643u, 10578819u},
};

/* ---- builders ----------------------------------------------------------- */

#define COMMAND_ADDRESS UINT32_C(0x100)
#define FRAMEBUFFER_ADDRESS UINT32_C(0x5000)
#define DEPTH_IMAGE_ADDRESS UINT32_C(0x6000)
#define DP_START UINT32_C(0x04100000)
#define DP_END (DP_START + UINT32_C(4))
#define AI_DRAM_ADDR UINT32_C(0x04500000)
#define AI_LEN UINT32_C(0x04500004)
#define AI_CONTROL UINT32_C(0x04500008)
#define AI_DACRATE UINT32_C(0x04500010)
#define AI_BITRATE UINT32_C(0x04500014)
#define VI_BASE UINT32_C(0x04400000)
#define VI_ORIGIN UINT32_C(0x3000)

static void install_spin_loop(t64_machine *machine) {
  machine->ram[UINT32_C(0x1000)] = 0x08u;
  machine->ram[UINT32_C(0x1001)] = 0x00u;
  machine->ram[UINT32_C(0x1002)] = 0x04u;
  machine->ram[UINT32_C(0x1003)] = 0x00u;
  (void)memset(machine->ram + UINT32_C(0x1004), 0, 4u);
  machine->cpu.pc = UINT32_C(0x80001000);
  machine->cpu.pc_full = t64_cpu_canonical_address(machine->cpu.pc);
}

/* Idle: admitted, a guest spin loop retiring in RDRAM, tick 64. The same
 * builder runs on the si-v1 id and on its si-v2 twin. */
static t64_machine *build_idle(const char *profile_id) {
  t64_machine *machine = create_admitted(profile_id);

  if (machine == NULL) return NULL;
  install_spin_loop(machine);
  T64_CHECK(advance_to(machine, 64u));
  T64_CHECK_INT(machine->tick, 64u);
  return machine;
}

static void write_be64(uint8_t *bytes, uint64_t value) {
  uint32_t i;

  for (i = 0u; i < 8u; i++) bytes[i] = (uint8_t)(value >> (56u - i * 8u));
}

/* The Z-triangle command list of core/tests/test_rdp_depth_composite.c
 * first_pass (copied, never edited there): RGBA16 color image at 0x5000 width
 * 8, scissor [0,8) x [0,8), primitive Z compare and update, SetZImage 0x6000,
 * SetPrimDepth Z=0x1357 dZ=9, green primitive color, the literal combiner, and
 * the Z triangle opcode 0x09 with a zero Z plane. */
static const uint64_t z_triangle_words[] = {
    UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
    UINT64_C(0x2f00000000000034), UINT64_C(0x3e00000000006000),
    UINT64_C(0x2e00000013570009), UINT64_C(0x3a00000000ff00ff),
    UINT64_C(0xfc00000000018600), UINT64_C(0x09000014000c0004),
    UINT64_C(0x00010000fffe8000), UINT64_C(0x0001000000000000),
    UINT64_C(0x0004000000018000), UINT64_C(0x0000000000000000),
    UINT64_C(0x0000000000000000),
};
#define Z_TRIANGLE_COUNT (sizeof z_triangle_words / sizeof z_triangle_words[0])

static size_t hidden_nonzero(const t64_machine *machine) {
  size_t i;
  size_t count = 0u;

  for (i = 0u; i < HIDDEN_BYTES; i++) {
    if (machine->rdp_depth_hidden[i] != 0u) count++;
  }
  return count;
}

/* Mid Z-triangle: the list above submitted through the DP registers and the
 * machine advanced one tick at a time until the triangle is rendering and the
 * first row is behind it (the reached condition of the importer's B17 row). */
static t64_machine *build_mid_z_triangle(const char *profile_id) {
  t64_machine *machine = create_admitted(profile_id);
  size_t i;

  if (machine == NULL) return NULL;
  install_spin_loop(machine);
  (void)memset(machine->ram + FRAMEBUFFER_ADDRESS, 0xa5, 8u * 8u * 2u);
  for (i = 0u; i < 8u * 8u; i++) {
    machine->ram[DEPTH_IMAGE_ADDRESS + i * 2u] = 0xffu;
    machine->ram[DEPTH_IMAGE_ADDRESS + i * 2u + 1u] = 0xfcu;
  }
  for (i = 0u; i < Z_TRIANGLE_COUNT; i++) {
    write_be64(machine->ram + COMMAND_ADDRESS + i * 8u, z_triangle_words[i]);
  }
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_START, 4u, COMMAND_ADDRESS),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_END, 4u,
                                      COMMAND_ADDRESS +
                                          (uint32_t)(Z_TRIANGLE_COUNT * 8u)),
                T64_STATUS_OK);
  for (i = 0u; i < 3000u && !(machine->rdp.render_active &&
                              machine->rdp.render_triangle &&
                              machine->rdp.render_y >
                                  machine->rdp.render_y_begin);
       i++) {
    T64_CHECK(advance_to(machine, machine->tick + 1u));
  }
  /* Reached: a Z triangle in flight with every depth member live. */
  T64_CHECK(machine->rdp.render_active && machine->rdp.render_triangle);
  T64_CHECK(machine->rdp.render_y > machine->rdp.render_y_begin);
  T64_CHECK(machine->rdp.depth_image_valid);
  T64_CHECK(machine->rdp.primitive_depth_valid);
  T64_CHECK_INT(machine->rdp.depth_image_address, DEPTH_IMAGE_ADDRESS);
  T64_CHECK(machine->rdp.pending_triangle_word_count != 0u);
  T64_CHECK(hidden_nonzero(machine) != 0u);
  return machine;
}

static void write_vi_register(t64_machine *machine, uint32_t offset,
                              uint32_t value) {
  T64_CHECK_INT(t64_machine_bus_write(machine, VI_BASE + offset, 4u, value),
                T64_STATUS_OK);
}

/* The tick of the earliest queued event of one device and kind (a VI field of
 * the current generation, or an AI sample). */
static bool next_event_tick(const t64_machine *machine, bool video,
                            uint64_t *tick) {
  size_t q;
  bool found = false;

  for (q = 0u; q < machine->queue_count; q++) {
    const size_t index = (machine->queue_head + q) % T64_EVENT_QUEUE_CAPACITY;
    const t64_event *event = &machine->events[index];
    const bool match =
        video ? (event->device == T64_VI_DEVICE &&
                 event->kind == T64_VI_EVENT_FIELD &&
                 event->token == machine->vi.event_generation)
              : (event->device == T64_AI_DEVICE &&
                 event->kind == T64_AI_EVENT_SAMPLE);

    if (match && (!found || event->tick < *tick)) {
      *tick = event->tick;
      found = true;
    }
  }
  return found;
}

/* Both video slots and both audio slots occupied. A full ring stalls the
 * machine at the next event of its device, so the order matters: two 16-byte
 * DMAs of literal PCM (four samples each) play out first, which fills the audio
 * ring and leaves the AI with no more samples; then the VI is enabled over
 * authored 5551 source pixels and scans out two fields, which fills the video
 * ring, and the machine is stopped before the blocked third field. */
static t64_machine *build_retained_packets(const char *profile_id) {
  static const uint16_t source[4] = {UINT16_C(0xf801), UINT16_C(0x07c1),
                                     UINT16_C(0x003f), UINT16_C(0xffff)};
  static const uint8_t pcm[32] = {
      0x00u, 0x00u, 0x7fu, 0xffu, 0x80u, 0x00u, 0xffu, 0xffu,
      0x12u, 0x34u, 0xedu, 0xccu, 0x00u, 0x01u, 0xffu, 0xfeu,
      0x80u, 0x00u, 0x7fu, 0xffu, 0xffu, 0xffu, 0x00u, 0x00u,
      0x55u, 0xaau, 0xaau, 0x55u, 0x00u, 0x00u, 0x80u, 0x00u};
  t64_machine *machine = create_admitted(profile_id);
  size_t i;

  if (machine == NULL) return NULL;
  (void)memcpy(machine->ram + UINT32_C(0x100), pcm, sizeof pcm);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_DACRATE, 4u, 1103u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_BITRATE, 4u, 15u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_DRAM_ADDR, 4u, 0x100u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_LEN, 4u, 16u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_DRAM_ADDR, 4u, 0x110u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_LEN, 4u, 16u),
                T64_STATUS_OK);
  install_spin_loop(machine);
  for (i = 0u; i < 64u && machine->ai.packet_count < 2u; i++) {
    uint64_t tick = 0u;

    if (!next_event_tick(machine, false, &tick) || !advance_to(machine, tick)) {
      break;
    }
  }
  T64_CHECK_INT(machine->ai.packet_count, 2u);
  for (i = 0u; i < sizeof source / sizeof source[0]; i++) {
    const uint32_t address = VI_ORIGIN + (uint32_t)(i * 2u);

    T64_CHECK_INT(t64_machine_bus_write(machine, address, 2u, source[i]),
                  T64_STATUS_OK);
    t64_rdp_memory_coverage_write(machine, address, 2u, 8u);
  }
  write_vi_register(machine, UINT32_C(0x04), VI_ORIGIN);
  write_vi_register(machine, UINT32_C(0x08), UINT32_C(4));
  write_vi_register(machine, UINT32_C(0x18), UINT32_C(8));
  write_vi_register(machine, UINT32_C(0x1c), UINT32_C(15));
  write_vi_register(machine, UINT32_C(0x24), UINT32_C(4));
  write_vi_register(machine, UINT32_C(0x28), UINT32_C(2));
  write_vi_register(machine, UINT32_C(0x30), UINT32_C(0x400));
  write_vi_register(machine, UINT32_C(0x34), UINT32_C(0x400));
  write_vi_register(machine, UINT32_C(0x00), UINT32_C(2));
  for (i = 0u; i < 512u && machine->vi.packet_count < 2u; i++) {
    uint64_t tick = 0u;

    if (!next_event_tick(machine, true, &tick) || !advance_to(machine, tick)) {
      break;
    }
  }
  T64_CHECK_INT(machine->vi.packet_count, 2u);
  T64_CHECK_INT(machine->ai.packet_count, 2u);
  return machine;
}

/* ---- blob files and the decoders ---------------------------------------- */

static unsigned file_sequence;

/* Write one blob to a new 0600 file under the evidence directory; the path is
 * returned in path_out. */
static bool write_blob_file(const char *label, const uint8_t *bytes,
                            size_t length, char *path_out, size_t capacity) {
  const char *directory = getenv("T64_PHASE10_EVIDENCE_DIR");
  size_t written = 0u;
  int fd;

  if (directory == NULL || directory[0] == '\0') {
    T64_CHECK(false);
    return false;
  }
  if (mkdir(directory, 0700) != 0 && errno != EEXIST) {
    T64_CHECK(false);
    return false;
  }
  if (snprintf(path_out, capacity, "%s/%s-%ld-%u.sta", directory, label,
               (long)getpid(), file_sequence++) >= (int)capacity) {
    T64_CHECK(false);
    return false;
  }
  (void)unlink(path_out);
  fd = open(path_out, O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (fd < 0) {
    T64_CHECK(false);
    return false;
  }
  while (written < length) {
    const ssize_t count = write(fd, bytes + written, length - written);

    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) break;
    written += (size_t)count;
  }
  if (close(fd) != 0 || written != length) {
    (void)unlink(path_out);
    T64_CHECK(false);
    return false;
  }
  return true;
}

/* Runs `python3 -B -I SCRIPT --blob PATH` and captures its one output line.
 * Returns the exit status (0 accepted, 1 refused, 2 usage) or -1. */
static int run_decoder(const char *script, const char *path, char *out,
                       size_t capacity) {
  int pipe_fds[2];
  pid_t pid;
  int status = 0;
  size_t used = 0u;

  out[0] = '\0';
  if (script == NULL || script[0] == '\0' || pipe(pipe_fds) != 0) return -1;
  pid = fork();
  if (pid < 0) return -1;
  if (pid == 0) {
    char python[] = "python3";
    char no_bytecode[] = "-B";
    char isolated[] = "-I";
    char blob_option[] = "--blob";
    char *args[7];

    (void)close(pipe_fds[0]);
    (void)dup2(pipe_fds[1], STDOUT_FILENO);
    args[0] = python;
    args[1] = no_bytecode;
    args[2] = isolated;
    args[3] = (char *)(uintptr_t)script;
    args[4] = blob_option;
    args[5] = (char *)(uintptr_t)path;
    args[6] = NULL;
    execvp(python, args);
    _exit(127);
  }
  (void)close(pipe_fds[1]);
  for (;;) {
    const ssize_t count = read(pipe_fds[0], out + used, capacity - 1u - used);

    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) break;
    used += (size_t)count;
    if (used + 1u >= capacity) break;
  }
  out[used] = '\0';
  (void)close(pipe_fds[0]);
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) return -1;
  }
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* One blob through the decoder named by the environment variable, by way of a
 * private file that is removed again. */
static int decode_with(const char *variable, const char *label,
                       const uint8_t *bytes, size_t length, char *out,
                       size_t capacity) {
  char path[512];
  int status;

  if (!write_blob_file(label, bytes, length, path, sizeof path)) return -1;
  status = run_decoder(getenv(variable), path, out, capacity);
  (void)unlink(path);
  return status;
}

static bool has_text(const char *line, const char *text) {
  return strstr(line, text) != NULL;
}

/* True when `"key":"value"` appears in the decoder line. */
static bool has_pair(const char *line, const char *key, unsigned long long v) {
  char text[96];

  (void)snprintf(text, sizeof text, "\"%s\":\"%llu\"", key, v);
  return has_text(line, text);
}

static unsigned long long pair_value(const char *line, const char *key,
                                     bool *found) {
  char text[96];
  const char *at;

  (void)snprintf(text, sizeof text, "\"%s\":\"", key);
  at = strstr(line, text);
  *found = at != NULL;
  return at == NULL ? 0u : strtoull(at + strlen(text), NULL, 10);
}

/* ---- blob access with literal offsets ----------------------------------- */

static const uint8_t *table_entry(const uint8_t *blob, uint32_t index) {
  return blob + 0x120u + 56u * index;
}

/* Body offset and length of the canonical section with the given id. */
static bool find_section(const uint8_t *blob, uint32_t id, uint64_t *offset,
                         uint64_t *length) {
  const uint32_t count = rd32(blob + 0x018u);
  uint32_t i;

  for (i = 0u; i < count && i < 32u; i++) {
    const uint8_t *entry = table_entry(blob, i);

    if (rd32(entry) == id) {
      *offset = rd64(entry + 8u);
      *length = rd64(entry + 16u);
      return true;
    }
  }
  return false;
}

/* The container length from its own table: header 0x120, 56 per entry, the
 * canonical region (the 15-byte domain, then 4 + body for every canonical
 * section), the 40-byte observer and the 32-byte trailer. */
static uint64_t derived_total(const uint8_t *blob) {
  const uint32_t count = rd32(blob + 0x018u);
  uint64_t canonical = 15u;
  uint32_t i;

  for (i = 0u; i + 1u < count && i < 32u; i++) {
    canonical += 4u + rd64(table_entry(blob, i) + 16u);
  }
  return 0x120u + 56u * (uint64_t)count + canonical + 40u + 32u;
}

static bool hex_of(const uint8_t digest[T64_DIGEST_SIZE], char out[65]) {
  static const char digits[] = "0123456789abcdef";
  size_t i;

  for (i = 0u; i < T64_DIGEST_SIZE; i++) {
    out[2u * i] = digits[digest[i] >> 4];
    out[2u * i + 1u] = digits[digest[i] & 15u];
  }
  out[64] = '\0';
  return true;
}

static bool reason_is_header_level(uint32_t reason) {
  return reason == T64_STATE_REASON_MAGIC ||
         reason == T64_STATE_REASON_TOTAL_LENGTH ||
         reason == T64_STATE_REASON_CONTAINER_VERSION;
}

/* ---- case export-v26-idle ---------------------------------------------- */

static void check_pairing_in_importer(const family_row *row,
                                      const uint8_t *blob26, size_t length26,
                                      const uint8_t *blob25, size_t length25) {
  t64_machine *fresh = create_admitted(row->v26_id);
  t64_machine *fresh25 = create_admitted(row->v25_id);
  uint8_t *copy;
  t64_state_import_report report;
  snapshot before;
  t64_status status;

  if (fresh == NULL || fresh25 == NULL) {
    t64_machine_destroy(fresh);
    t64_machine_destroy(fresh25);
    return;
  }
  snapshot_take(fresh, &before);
  /* The exported v26 blob gets past the header: its refusal, if any, is not a
   * magic, length or container version reason (full v26 acceptance is plan
   * 10-12). */
  (void)memset(&report, 0, sizeof report);
  status = t64_machine_state_import(fresh, blob26, length26, 0u, &report);
  fprintf(stderr, "export-v26-idle: %s import status %d reason %d section %u\n",
          row->v26_id, (int)status, (int)report.reason,
          (unsigned)report.section_id);
  T64_CHECK(!reason_is_header_level(report.reason));
  if (status != T64_STATUS_OK) {
    T64_CHECK(snapshot_matches(fresh, &before));
  }
  snapshot_free(&before);
  /* A machine that took a blob (10-12) is a different machine; the rest of the
   * pairing rows run on fresh ones. */
  t64_machine_destroy(fresh);
  fresh = create_admitted(row->v26_id);
  if (fresh == NULL) {
    t64_machine_destroy(fresh25);
    return;
  }

  /* (2, 26) rewritten to (1, 26): refused at the version word. */
  copy = (uint8_t *)malloc(length26);
  if (copy != NULL) {
    snapshot_take(fresh, &before);
    (void)memcpy(copy, blob26, length26);
    put32(copy + 0x008u, 1u);
    (void)memset(&report, 0, sizeof report);
    status = t64_machine_state_import(fresh, copy, length26, 0u, &report);
    fprintf(stderr,
            "export-v26-idle: %s version 1 with state 26: status %d reason %d "
            "position 0x%llx\n",
            row->v26_id, (int)status, (int)report.reason,
            (unsigned long long)report.position);
    T64_CHECK(status != T64_STATUS_OK);
    T64_CHECK_INT(report.reason, T64_STATE_REASON_CONTAINER_VERSION);
    T64_CHECK_INT(report.position, 0x008);
    T64_CHECK(snapshot_matches(fresh, &before));
    snapshot_free(&before);

    /* (2, 26) rewritten to (2, 25): refused at the version word too. */
    snapshot_take(fresh, &before);
    (void)memcpy(copy, blob26, length26);
    put32(copy + 0x00Cu, 25u);
    (void)memset(&report, 0, sizeof report);
    status = t64_machine_state_import(fresh, copy, length26, 0u, &report);
    T64_CHECK(status != T64_STATUS_OK);
    T64_CHECK_INT(report.reason, T64_STATE_REASON_CONTAINER_VERSION);
    T64_CHECK_INT(report.position, 0x008);
    T64_CHECK(snapshot_matches(fresh, &before));
    snapshot_free(&before);

    /* A blob of state 26 whose domain text was forged to the state 25 text and
     * fully re-signed is still refused by a state 26 machine, untouched. */
    snapshot_take(fresh, &before);
    (void)memcpy(copy, blob26, length26);
    {
      const uint64_t canonical = rd64(copy + 0x090u);

      T64_CHECK(canonical + 15u < length26);
      T64_CHECK(copy[canonical + 14u] == (uint8_t)'6');
      copy[canonical + 14u] = (uint8_t)'5';
    }
    blob_resign(copy, length26, RS_ALL);
    (void)memset(&report, 0, sizeof report);
    status = t64_machine_state_import(fresh, copy, length26, 0u, &report);
    T64_CHECK(status != T64_STATUS_OK);
    T64_CHECK(report.reason != T64_STATE_REASON_NONE);
    T64_CHECK(snapshot_matches(fresh, &before));
    snapshot_free(&before);
    free(copy);
  }

  /* The v25 side of the pairing: (1, 25) rewritten to (2, 25) and to (1, 26)
   * refuse at the version word on the v25 machine. */
  copy = (uint8_t *)malloc(length25);
  if (copy != NULL) {
    snapshot_take(fresh25, &before);
    (void)memcpy(copy, blob25, length25);
    put32(copy + 0x008u, 2u);
    (void)memset(&report, 0, sizeof report);
    status = t64_machine_state_import(fresh25, copy, length25, 0u, &report);
    T64_CHECK(status != T64_STATUS_OK);
    T64_CHECK_INT(report.reason, T64_STATE_REASON_CONTAINER_VERSION);
    T64_CHECK_INT(report.position, 0x008);
    T64_CHECK(snapshot_matches(fresh25, &before));
    snapshot_free(&before);
    snapshot_take(fresh25, &before);
    (void)memcpy(copy, blob25, length25);
    put32(copy + 0x00Cu, 26u);
    (void)memset(&report, 0, sizeof report);
    status = t64_machine_state_import(fresh25, copy, length25, 0u, &report);
    T64_CHECK(status != T64_STATUS_OK);
    T64_CHECK_INT(report.reason, T64_STATE_REASON_CONTAINER_VERSION);
    T64_CHECK(snapshot_matches(fresh25, &before));
    snapshot_free(&before);
    free(copy);
  }
  t64_machine_destroy(fresh);
  t64_machine_destroy(fresh25);
}

static void check_idle_decoders(const family_row *row, const uint8_t *blob26,
                                size_t length26, const uint8_t *blob25,
                                size_t length25) {
  static char line[8192];
  static char old_line[8192];
  static char new_line[8192];
  char ids[256];
  char expected[320];
  size_t used = 0u;
  size_t i;
  uint8_t *copy;

  /* v2 decoder on the v26 blob. */
  T64_CHECK_INT(decode_with("T64_STATE_DECODER", "idle26", blob26, length26,
                            line, sizeof line),
                0);
  T64_CHECK(has_text(line, "\"status\":\"accepted\""));
  T64_CHECK(has_pair(line, "container_version", 2u));
  T64_CHECK(has_pair(line, "state_serialization_version", 26u));
  T64_CHECK(has_pair(line, "total_length", length26));
  T64_CHECK(has_pair(line, "hidden_table_length", HIDDEN_BYTES));
  T64_CHECK(has_pair(line, "depth_image_address", 0u));
  T64_CHECK(has_pair(line, "depth_image_valid", 0u));
  T64_CHECK(has_pair(line, "primitive_z", 0u));
  T64_CHECK(has_pair(line, "primitive_dz", 0u));
  T64_CHECK(has_pair(line, "primitive_depth_valid", 0u));
  ids[0] = '\0';
  for (i = 0u; i < row->table_count; i++) {
    used += (size_t)snprintf(ids + used, sizeof ids - used, "%s\"%u\"",
                             i == 0u ? "" : ",", (unsigned)row->ids[i]);
  }
  (void)snprintf(expected, sizeof expected, "\"section_ids\":[%s]", ids);
  T64_CHECK(has_text(line, expected));

  /* The decoder refuses the version words out of pairing. */
  copy = (uint8_t *)malloc(length26 > length25 ? length26 : length25);
  if (copy != NULL) {
    (void)memcpy(copy, blob26, length26);
    put32(copy + 0x008u, 1u);
    T64_CHECK_INT(decode_with("T64_STATE_DECODER", "pair-1-26", copy, length26,
                              line, sizeof line),
                  1);
    T64_CHECK(has_text(line, "\"kind\":\"container-version\""));
    (void)memcpy(copy, blob25, length25);
    put32(copy + 0x008u, 2u);
    T64_CHECK_INT(decode_with("T64_STATE_DECODER", "pair-2-25", copy, length25,
                              line, sizeof line),
                  1);
    T64_CHECK(has_text(line, "\"kind\":\"container-version\""));
    free(copy);
  }

  /* Version 1 is decoded unchanged: the v2 decoder and the pinned version 1
   * decoder print the same bytes for the v25 blob. */
  T64_CHECK_INT(decode_with("T64_STATE_DECODER_V1", "idle25-old", blob25,
                            length25, old_line, sizeof old_line),
                0);
  T64_CHECK_INT(decode_with("T64_STATE_DECODER", "idle25-new", blob25, length25,
                            new_line, sizeof new_line),
                0);
  T64_CHECK(old_line[0] != '\0');
  T64_CHECK_STR(new_line, old_line);
  T64_CHECK(has_pair(new_line, "state_serialization_version", 25u));
  T64_CHECK(!has_text(new_line, "container_version"));
}

static void case_export_v26_idle(void) {
  size_t index;

  for (index = 0u; index < FAMILY_COUNT; index++) {
    const family_row *row = &family[index];
    t64_machine *m25 = build_idle(row->v25_id);
    t64_machine *m26 = build_idle(row->v26_id);
    size_t length25 = 0u;
    size_t length26 = 0u;
    uint8_t *blob25 = NULL;
    uint8_t *blob26 = NULL;
    uint32_t i;

    if (m25 != NULL && m26 != NULL) {
      blob25 = export_blob(m25, &length25);
      blob26 = export_blob(m26, &length26);
    }
    if (blob25 != NULL && blob26 != NULL) {
      /* Header bytes by literal offset. */
      T64_CHECK(memcmp(blob26, "T64STATE", 8u) == 0);
      T64_CHECK_INT(rd32(blob26 + 0x008u), 2);
      T64_CHECK_INT(rd32(blob26 + 0x00Cu), 26);
      T64_CHECK_INT(rd64(blob26 + 0x010u), length26);
      T64_CHECK_INT(rd32(blob26 + 0x018u), row->table_count);
      T64_CHECK_INT(rd32(blob26 + 0x01Cu), 0);
      T64_CHECK_INT(rd64(blob26 + 0x068u), 64);
      T64_CHECK(memcmp(blob26 + 0x020u, m26->profile_digest,
                       T64_DIGEST_SIZE) == 0);
      T64_CHECK_INT(rd32(blob25 + 0x008u), 1);
      T64_CHECK_INT(rd32(blob25 + 0x00Cu), 25);
      T64_CHECK_INT(rd32(blob25 + 0x018u), row->table_count);
      /* The section ids ascend and equal the v25 list. */
      for (i = 0u; i < row->table_count; i++) {
        T64_CHECK_INT(rd32(table_entry(blob26, i)), row->ids[i]);
        T64_CHECK_INT(rd32(table_entry(blob25, i)), row->ids[i]);
        if (i > 0u) {
          T64_CHECK(rd32(table_entry(blob26, i)) >
                    rd32(table_entry(blob26, i - 1u)));
        }
        /* Only section 8 changes its size, by exactly the three additions. */
        T64_CHECK_INT(rd64(table_entry(blob26, i) + 16u),
                      rd64(table_entry(blob25, i) + 16u) +
                          (row->ids[i] == 8u ? V26_EXTRA_BYTES : 0u));
      }
      /* Sizes: the v25 idle size of the same builder, plus 20 bytes of depth
       * scalars, the 4-byte hidden length field and 2,097,152 hidden bytes. */
      T64_CHECK_INT(length25, row->v25_idle);
      T64_CHECK_INT(length26, length25 + V26_EXTRA_BYTES);
      T64_CHECK_INT(length26, row->v26_idle);
      T64_CHECK_INT(derived_total(blob26), length26);
      /* The hidden table of an idle machine is all zero and the scalars are
       * zero: the decoder side is checked next. */
      check_idle_decoders(row, blob26, length26, blob25, length25);
      check_pairing_in_importer(row, blob26, length26, blob25, length25);
    }
    free(blob25);
    free(blob26);
    t64_machine_destroy(m25);
    t64_machine_destroy(m26);
  }
}

/* ---- case export-v26-bounds -------------------------------------------- */

/* One v26 export checked against everything that is derived from the machine
 * and from the layout, never from a production constant: the container length
 * from its own table, the v26 upper bound, the decoder, one byte less and one
 * byte more, the depth scalars and the hidden table. */
static void check_bounds_state(const char *label, const t64_machine *machine,
                               size_t idle_length, uint64_t idle_section8) {
  static char line[8192];
  size_t length = 0u;
  uint8_t *blob = export_blob(machine, &length);
  uint8_t *longer;
  uint64_t body8 = 0u;
  uint64_t length8 = 0u;
  uint64_t scalars;
  uint64_t hidden;
  uint32_t pending;
  bool found = false;
  unsigned long long reported;
  uint8_t digest[T64_DIGEST_SIZE];
  char hex[65];
  char expected[96];

  if (blob == NULL) return;
  fprintf(stderr, "export-v26-bounds: %s length %zu\n", label, length);
  T64_CHECK_INT(derived_total(blob), length);
  T64_CHECK(length > idle_length || idle_length == 0u);
  T64_CHECK(length < V26_UPPER_BOUND);
  T64_CHECK(find_section(blob, 8u, &body8, &length8));
  if (idle_section8 != 0u) {
    /* The pending triangle words are the section 8 content that differs from
     * idle in the mid-render state; retained packets add their own lengths. */
    T64_CHECK(length8 >= idle_section8);
  }
  pending = rd32(blob + body8 + S8_PENDING_COUNT);
  T64_CHECK_INT(pending, machine->rdp.pending_triangle_word_count);

  /* The depth scalars at their decoded offset: the position is the section 8
   * body plus 228, 8 per pending word and the 2,097,152-byte triangle
   * coverage array. */
  scalars = body8 + S8_PENDING_WORDS + 8u * (uint64_t)pending +
            S8_TRIANGLE_TAIL + S8_TRIANGLE_COVERAGE;
  T64_CHECK_INT(rd32(blob + scalars + 0u), machine->rdp.depth_image_address);
  T64_CHECK_INT(rd32(blob + scalars + 4u), machine->rdp.depth_image_valid);
  T64_CHECK_INT(rd32(blob + scalars + 8u), machine->rdp.primitive_z);
  T64_CHECK_INT(rd32(blob + scalars + 12u), machine->rdp.primitive_dz);
  T64_CHECK_INT(rd32(blob + scalars + 16u), machine->rdp.primitive_depth_valid);
  /* The hidden table ends section 8: its length field and then every byte. */
  hidden = body8 + length8 - HIDDEN_BYTES;
  T64_CHECK_INT(rd32(blob + hidden - 4u), HIDDEN_BYTES);
  T64_CHECK(memcmp(blob + hidden, machine->rdp_depth_hidden, HIDDEN_BYTES) ==
            0);

  /* The decoder reports the same words and the same hidden table. */
  T64_CHECK_INT(decode_with("T64_STATE_DECODER", label, blob, length, line,
                            sizeof line),
                0);
  T64_CHECK(has_text(line, "\"status\":\"accepted\""));
  T64_CHECK(has_pair(line, "container_version", 2u));
  T64_CHECK(has_pair(line, "total_length", length));
  reported = pair_value(line, "depth_scalars_offset", &found);
  T64_CHECK(found);
  T64_CHECK_INT(reported, scalars);
  T64_CHECK(has_pair(line, "depth_image_address",
                     machine->rdp.depth_image_address));
  T64_CHECK(has_pair(line, "primitive_z", machine->rdp.primitive_z));
  T64_CHECK(has_pair(line, "primitive_dz", machine->rdp.primitive_dz));
  T64_CHECK(has_pair(line, "depth_image_valid", machine->rdp.depth_image_valid));
  T64_CHECK(has_pair(line, "primitive_depth_valid",
                     machine->rdp.primitive_depth_valid));
  T64_CHECK(has_pair(line, "hidden_table_length", HIDDEN_BYTES));
  reported = pair_value(line, "hidden_table_offset", &found);
  T64_CHECK(found);
  T64_CHECK_INT(reported, hidden);
  {
    t64_sha256_ctx context;

    t64_sha256_init(&context);
    t64_sha256_update(&context, machine->rdp_depth_hidden, HIDDEN_BYTES);
    t64_sha256_final(&context, digest);
  }
  (void)hex_of(digest, hex);
  (void)snprintf(expected, sizeof expected, "\"hidden_table_sha256\":\"%s\"",
                 hex);
  T64_CHECK(has_text(line, expected));

  /* One byte less and one byte more are refused by the decoder. */
  T64_CHECK_INT(decode_with("T64_STATE_DECODER", "short", blob, length - 1u,
                            line, sizeof line),
                1);
  T64_CHECK(has_text(line, "\"kind\":\"total-length\""));
  longer = (uint8_t *)malloc(length + 1u);
  if (longer != NULL) {
    (void)memcpy(longer, blob, length);
    longer[length] = 0u;
    T64_CHECK_INT(decode_with("T64_STATE_DECODER", "long", longer, length + 1u,
                              line, sizeof line),
                  1);
    T64_CHECK(has_text(line, "\"kind\":\"total-length\""));
    free(longer);
  }
  free(blob);
}

static void case_export_v26_bounds(void) {
  static const char *const ids[] = {V2_STEM, V2_STEM "-s32k"};
  size_t idle_maximum = 0u;
  size_t derived;
  size_t rounded;
  size_t index;

  /* The upper bound: the largest idle size over the four ids plus two video
   * and two audio packets at their maximum, rounded up to a whole MiB. */
  for (index = 0u; index < FAMILY_COUNT; index++) {
    t64_machine *machine = build_idle(family[index].v26_id);
    size_t required = 0u;

    if (machine == NULL) continue;
    T64_CHECK_INT(t64_machine_state_export(machine, NULL, 0u, &required),
                  T64_STATUS_BUFFER_TOO_SMALL);
    T64_CHECK_INT(required, family[index].v26_idle);
    if (required > idle_maximum) idle_maximum = required;
    t64_machine_destroy(machine);
  }
  derived = idle_maximum + 2u * VIDEO_PACKET_MAX + 2u * AUDIO_PACKET_MAX;
  rounded = ((derived + MIB - 1u) / MIB) * MIB;
  fprintf(stderr,
          "export-v26-bounds: idle maximum %zu, with packets %zu, bound %zu\n",
          idle_maximum, derived, rounded);
  T64_CHECK_INT(idle_maximum, 10578819);
  T64_CHECK_INT(derived, 10578819u + 614400u + 524272u);
  T64_CHECK_INT(rounded, V26_UPPER_BOUND);
  T64_CHECK(derived < V26_UPPER_BOUND);
  T64_CHECK_INT(V26_UPPER_BOUND, 12u * MIB);

  for (index = 0u; index < sizeof ids / sizeof ids[0]; index++) {
    t64_machine *idle = build_idle(ids[index]);
    size_t idle_length = 0u;
    uint64_t body8 = 0u;
    uint64_t length8 = 0u;
    uint8_t *idle_blob = idle == NULL ? NULL : export_blob(idle, &idle_length);
    t64_machine *mid;
    t64_machine *packets;

    if (idle_blob == NULL) {
      t64_machine_destroy(idle);
      continue;
    }
    T64_CHECK(find_section(idle_blob, 8u, &body8, &length8));
    check_bounds_state("idle", idle, 0u, 0u);

    mid = build_mid_z_triangle(ids[index]);
    if (mid != NULL) {
      uint8_t *mid_blob;
      size_t mid_length = 0u;
      uint64_t mid_body8 = 0u;
      uint64_t mid_length8 = 0u;

      check_bounds_state("mid-z", mid, idle_length, length8);
      mid_blob = export_blob(mid, &mid_length);
      if (mid_blob != NULL) {
        T64_CHECK(find_section(mid_blob, 8u, &mid_body8, &mid_length8));
        /* Section 8 differs from idle by the pending triangle words alone:
         * the packet words and nothing else (derived, not measured). */
        T64_CHECK_INT(mid_length8,
                      length8 + 8u * mid->rdp.pending_triangle_word_count);
        free(mid_blob);
      }
      t64_machine_destroy(mid);
    }
    packets = build_retained_packets(ids[index]);
    if (packets != NULL) {
      check_bounds_state("packets", packets, idle_length, length8);
      t64_machine_destroy(packets);
    }
    free(idle_blob);
    t64_machine_destroy(idle);
  }
}

int main(int argc, char **argv) {
  static const char *const cases[] = {"export-v26-idle", "export-v26-bounds"};

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    size_t index;

    for (index = 0u; index < sizeof cases / sizeof cases[0]; index++) {
      printf("%s\n", cases[index]);
    }
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    if (strcmp(argv[2], "export-v26-idle") == 0) {
      case_export_v26_idle();
      return T64_TEST_FINISH("unit.core.state.export-v26");
    }
    if (strcmp(argv[2], "export-v26-bounds") == 0) {
      case_export_v26_bounds();
      return T64_TEST_FINISH("unit.core.state.export-v26-bounds");
    }
    fprintf(stderr, "unknown case: %s\n", argv[2]);
    return 2;
  }
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--list-cases|--case NAME]\n", argv[0]);
    return 2;
  }
  case_export_v26_idle();
  case_export_v26_bounds();
  return T64_TEST_FINISH("unit.core.state.export-v26-all");
}
