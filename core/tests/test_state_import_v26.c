/*
 * Phase 10 state import tests for state 26 (plan 10-12).
 *
 * State 26 carries five depth scalars inside the RDP block of section 8 and
 * the 2,097,152-byte hidden depth table at the end of section 8 (plan 10-09,
 * docs/spec/state-container.md "Container v2 (state 26)"). The importer reads
 * the scalars before the triangle-render proof so a Z triangle in flight can be
 * proven from the blob values, and the commit writes the scalars and the table
 * from the blob for state 26 only.
 *
 * The file is separate from core/tests/test_state_import.c, whose case list is
 * the phase09-inventoried binary and is never edited. Offsets below are the
 * literal layout of the document; the re-signing helper is written from the
 * container definition with the state 26 domain text (the shared helper header
 * carries the state 25 text).
 *
 * Cases:
 *   roundtrip  idle, mid Z-triangle and mid scan-out states of the state 26
 *              ids imported into fresh machines and continued equal; a
 *              state 25 blob keeps the hidden table zero on a state 25 machine
 *   hostile    the independently authored hostile rows of
 *              docs/spec/state-mutants-v26.txt and the v26 fuzz windows
 *
 * usage: test_state_import_v26 [--list-cases|--case NAME]
 */
#include "state_import_helpers.h"

#define V2_STEM "ntsc-cart-4m-openipl3-rdram-b-t1-si-v2"
#define V2_E4K_ID V2_STEM "-e4k"
#define V2_E16K_ID V2_STEM "-e16k"
#define V2_S32K_ID V2_STEM "-s32k"
#define V26_DOMAIN "t64.state.si.26"
#define HIDDEN_BYTES 2097152u
#define COVERAGE_BYTES 2097152u

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
#define SCAN_COMMAND_ADDRESS UINT32_C(0x200)
#define PCM_ADDRESS UINT32_C(0x400)
#define SRAM_WORD UINT32_C(0x08000104)
#define SCAN_DACRATE 1103u

/* Section 8 body offsets of the version 2 layout: the pending triangle word
 * count n at 196, the n words from 200, then seven u32 words, the triangle
 * coverage array of 2,097,152 bytes, and the five depth scalars at
 * 228 + 8n + 2,097,152. The hidden length word is the 4 bytes before the last
 * 2,097,152 bytes of the body. */
#define S8_PENDING_COUNT 196u
#define S8_PENDING_WORDS 200u
#define S8_SCALARS_BASE 228u
#define S8_TRIANGLE_COVERAGE 2097152u

/* ---- the independent v26 re-signing helper (domain t64.state.si.26) ------ */

static void v26_resign(uint8_t *blob, size_t length, unsigned flags) {
  const uint32_t count = rd32(blob + 0x018u);
  uint32_t i;

  if (length < D_TABLE + D_TRAILER_BYTES || count > 32u) return;
  if ((flags & RS_SECTIONS) != 0u) {
    for (i = 0u; i < count; i++) {
      uint8_t *entry = blob + D_TABLE + (size_t)D_ENTRY * i;
      const uint64_t offset = rd64(entry + E_OFF);
      const uint64_t body = rd64(entry + E_LEN);

      if (offset <= length && body <= length - offset) {
        sha_of(blob + offset, (size_t)body, entry + E_HASH);
      }
    }
  }
  if ((flags & RS_DECLARED) != 0u) {
    t64_sha256_ctx context;
    uint8_t digest[T64_DIGEST_SIZE];

    t64_sha256_init(&context);
    t64_sha256_update(&context, (const uint8_t *)V26_DOMAIN, D_DOMAIN_BYTES);
    for (i = 0u; i < count; i++) {
      const uint8_t *entry = blob + D_TABLE + (size_t)D_ENTRY * i;
      const uint32_t id = rd32(entry + E_ID);
      const uint64_t offset = rd64(entry + E_OFF);
      const uint64_t body = rd64(entry + E_LEN);
      uint8_t marker[4];

      if (id == 16u) continue;
      put32(marker, id);
      t64_sha256_update(&context, marker, sizeof marker);
      if (offset <= length && body <= length - offset) {
        t64_sha256_update(&context, blob + offset, (size_t)body);
      }
    }
    t64_sha256_final(&context, digest);
    (void)memcpy(blob + 0x070u, digest, sizeof digest);
  }
  if ((flags & RS_TRAILER) != 0u) {
    sha_of(blob, length - D_TRAILER_BYTES, blob + length - D_TRAILER_BYTES);
  }
}

static bool find_section(const uint8_t *blob, uint32_t id, size_t *offset,
                         size_t *length) {
  const uint32_t count = rd32(blob + 0x018u);
  uint32_t i;

  for (i = 0u; i < count && i < 32u; i++) {
    const uint8_t *entry = blob + D_TABLE + (size_t)D_ENTRY * i;

    if (rd32(entry + E_ID) == id) {
      *offset = (size_t)rd64(entry + E_OFF);
      *length = (size_t)rd64(entry + E_LEN);
      return true;
    }
  }
  return false;
}

/* ---- builders ----------------------------------------------------------- */

static void install_spin_loop(t64_machine *machine) {
  machine->ram[UINT32_C(0x1000)] = 0x08u;
  machine->ram[UINT32_C(0x1001)] = 0x00u;
  machine->ram[UINT32_C(0x1002)] = 0x04u;
  machine->ram[UINT32_C(0x1003)] = 0x00u;
  (void)memset(machine->ram + UINT32_C(0x1004), 0, 4u);
  machine->cpu.pc = UINT32_C(0x80001000);
  machine->cpu.pc_full = t64_cpu_canonical_address(machine->cpu.pc);
}

/* Idle: admitted, a guest spin loop retiring in RDRAM, tick 64 (the builder of
 * core/tests/test_state_export_v26.c, copied). */
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
 * first_pass (copied as in test_state_export_v26.c): RGBA16 color image at
 * 0x5000 width 8, scissor [0,8) x [0,8), primitive Z compare and update,
 * SetZImage 0x6000, SetPrimDepth Z=0x1357 dZ=9, green primitive color, the
 * literal combiner, and the Z triangle opcode 0x09 with a zero Z plane. */
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
 * first row is behind it. */
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

/* ---- import helpers ----------------------------------------------------- */

static void print_report(const char *label, t64_status status,
                         const t64_state_import_report *report) {
  fprintf(stderr,
          "%s: status %u reason %u section %u rule %u position 0x%llx\n", label,
          (unsigned)status, (unsigned)report->reason,
          (unsigned)report->section_id, (unsigned)report->rule,
          (unsigned long long)report->position);
}

/* One import into a fresh machine of the given id; returns the peer (or NULL)
 * and the status and report of the import. The report line is printed for a
 * refusal so a failed run quotes status, reason, section and rule. */
static t64_machine *import_fresh(const char *profile_id, const uint8_t *blob,
                                 size_t length, const char *label,
                                 t64_status *status_out,
                                 t64_state_import_report *report_out) {
  t64_machine *peer = create_admitted(profile_id);

  if (peer == NULL) return NULL;
  poison_import_report(report_out);
  *status_out = t64_machine_state_import(peer, blob, length, 0u, report_out);
  if (*status_out != T64_STATUS_OK) print_report(label, *status_out, report_out);
  return peer;
}

/* The idle state of an id: a fresh machine at tick 64 imports back into a
 * fresh machine of the same id with an all-zero hidden table (the setup proof
 * that the header, the registry, the domain and the section 8 layout admit a
 * state 26 blob). */
static void check_idle_roundtrip(const char *profile_id) {
  t64_machine *source = build_idle(profile_id);
  t64_machine *peer;
  t64_state_import_report report = T64_STATE_IMPORT_REPORT_INIT;
  t64_status status = T64_STATUS_OK;
  uint8_t *blob;
  size_t length = 0u;

  if (source == NULL) return;
  T64_CHECK_INT(hidden_nonzero(source), 0u);
  blob = export_blob(source, &length);
  if (blob == NULL) {
    t64_machine_destroy(source);
    return;
  }
  peer = import_fresh(profile_id, blob, length, "idle-v26-precheck", &status,
                      &report);
  T64_CHECK_INT(status, T64_STATUS_OK);
  T64_CHECK_INT(report.reason, T64_STATE_REASON_NONE);
  if (status == T64_STATUS_OK) {
    T64_CHECK(digests_equal(source, peer));
    T64_CHECK_INT(hidden_nonzero(peer), 0u);
    T64_CHECK_INT(peer->rdp.depth_image_address, 0u);
    T64_CHECK(!peer->rdp.depth_image_valid);
    T64_CHECK_INT(peer->tick, source->tick);
    fprintf(stderr, "idle-v26-precheck: %s imported, %zu blob bytes\n",
            profile_id, length);
  }
  if (peer != NULL) t64_machine_destroy(peer);
  free(blob);
  t64_machine_destroy(source);
}

/* A Z triangle in flight: export, import into a fresh machine of the same id,
 * equal digests, then both machines advance 2,000 ticks and must agree on the
 * digest, the color and depth bytes and the hidden table. */
static void check_mid_z_triangle(const char *profile_id) {
  t64_machine *source = build_mid_z_triangle(profile_id);
  t64_machine *peer;
  t64_state_import_report report = T64_STATE_IMPORT_REPORT_INIT;
  t64_status status = T64_STATUS_OK;
  uint8_t *blob;
  uint8_t color_before[8u * 8u * 2u];
  size_t length = 0u;
  size_t hidden_before;

  if (source == NULL) return;
  (void)memcpy(color_before, source->ram + FRAMEBUFFER_ADDRESS,
               sizeof color_before);
  hidden_before = hidden_nonzero(source);
  blob = export_blob(source, &length);
  if (blob == NULL) {
    t64_machine_destroy(source);
    return;
  }
  peer = import_fresh(profile_id, blob, length, "mid-z-triangle", &status,
                      &report);
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status == T64_STATUS_OK) {
    uint64_t base = peer->tick;

    T64_CHECK(digests_equal(source, peer));
    T64_CHECK(peer->rdp.render_active && peer->rdp.render_triangle);
    T64_CHECK_INT(peer->rdp.depth_image_address, DEPTH_IMAGE_ADDRESS);
    T64_CHECK(peer->rdp.depth_image_valid);
    T64_CHECK(peer->rdp.primitive_depth_valid);
    T64_CHECK_INT(peer->rdp.primitive_z, source->rdp.primitive_z);
    T64_CHECK_INT(peer->rdp.primitive_dz, source->rdp.primitive_dz);
    T64_CHECK_INT(hidden_nonzero(peer), hidden_before);
    T64_CHECK(memcmp(peer->rdp_depth_hidden, source->rdp_depth_hidden,
                     HIDDEN_BYTES) == 0);
    if (advance_pair(source, peer, base + 2000u, 0u)) {
      T64_CHECK(digests_equal(source, peer));
      T64_CHECK(!source->rdp.render_active && !peer->rdp.render_active);
      T64_CHECK(memcmp(source->ram + FRAMEBUFFER_ADDRESS,
                       peer->ram + FRAMEBUFFER_ADDRESS, sizeof color_before) ==
                0);
      T64_CHECK(memcmp(source->ram + DEPTH_IMAGE_ADDRESS,
                       peer->ram + DEPTH_IMAGE_ADDRESS, 8u * 8u * 2u) == 0);
      T64_CHECK(memcmp(source->rdp_depth_hidden, peer->rdp_depth_hidden,
                       HIDDEN_BYTES) == 0);
      /* not vacuous: the triangle kept drawing after the import */
      T64_CHECK(memcmp(source->ram + FRAMEBUFFER_ADDRESS, color_before,
                       sizeof color_before) != 0);
      T64_CHECK(hidden_nonzero(source) > hidden_before);
      fprintf(stderr,
              "mid-z-triangle: %s imported at tick %llu, %zu hidden entries "
              "then %zu after 2000 ticks, digests equal\n",
              profile_id, (unsigned long long)base, hidden_before,
              hidden_nonzero(peer));
    }
  }
  if (peer != NULL) t64_machine_destroy(peer);
  free(blob);
  t64_machine_destroy(source);
}

/* ---- mid scan-out with depth active ------------------------------------ */

/* A Z-tested 4x3 frame: the RGBA16 color image at VI_ORIGIN, width 4, scissor
 * [0,4) x [0,3), the primitive Z compare and update modes of the Z triangle
 * list above, SetZImage 0x6000, SetPrimDepth Z=0x1357 dZ=9 and the Z triangle
 * opcode 0x09 with a zero Z plane (the setup words of the list above, with the
 * width and the scissor words rewritten for 4x3). */
static const uint64_t scan_frame_words[] = {
    UINT64_C(0x3f10000300003000), UINT64_C(0x2d0000000001000c),
    UINT64_C(0x2f00000000000034), UINT64_C(0x3e00000000006000),
    UINT64_C(0x2e00000013570009), UINT64_C(0x3a00000000ff00ff),
    UINT64_C(0xfc00000000018600), UINT64_C(0x09000014000c0004),
    UINT64_C(0x00010000fffe8000), UINT64_C(0x0001000000000000),
    UINT64_C(0x0004000000018000), UINT64_C(0x0000000000000000),
    UINT64_C(0x0000000000000000),
};
#define SCAN_FRAME_COUNT (sizeof scan_frame_words / sizeof scan_frame_words[0])
#define FRAME_PIXELS 12u

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

/* 16 bytes of literal PCM per DMA (four stereo samples), different per block
 * so a packet equality is not a constant comparison. */
static void load_pcm(t64_machine *machine) {
  size_t i;

  for (i = 0u; i < 8u * 16u; i++) {
    machine->ram[PCM_ADDRESS + i] = (uint8_t)(0x11u * (i % 15u) + 7u * (i / 16u));
  }
}

static void queue_pcm_dma(t64_machine *machine, uint32_t block) {
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_DRAM_ADDR,
                                      4u, PCM_ADDRESS + 16u * block),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_LEN, 4u, 16u),
                T64_STATUS_OK);
}

/* Draw a Z-tested 4x3 frame, scan it out and retain a video packet and two
 * audio packets; the machine is left between two field boundaries (a few ticks
 * after a boundary), so the next field is a scan-out in progress. */
static t64_machine *build_mid_scan_out(const char *profile_id) {
  static const uint16_t unused_marker = 0u;
  t64_machine *machine = create_admitted(profile_id);
  uint64_t tick = 0u;
  size_t i;

  (void)unused_marker;
  if (machine == NULL) return NULL;
  install_spin_loop(machine);
  load_pcm(machine);
  (void)memset(machine->ram + VI_ORIGIN, 0xa5, FRAME_PIXELS * 2u);
  for (i = 0u; i < FRAME_PIXELS; i++) {
    machine->ram[DEPTH_IMAGE_ADDRESS + i * 2u] = 0xffu;
    machine->ram[DEPTH_IMAGE_ADDRESS + i * 2u + 1u] = 0xfcu;
  }
  for (i = 0u; i < SCAN_FRAME_COUNT; i++) {
    write_be64(machine->ram + SCAN_COMMAND_ADDRESS + i * 8u,
               scan_frame_words[i]);
  }
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_START, 4u,
                                      SCAN_COMMAND_ADDRESS),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_END, 4u,
                                      SCAN_COMMAND_ADDRESS +
                                          (uint32_t)(SCAN_FRAME_COUNT * 8u)),
                T64_STATUS_OK);
  for (i = 0u; i < 4000u && !(machine->rdp.triangle_count != 0u &&
                              !machine->rdp.render_active);
       i++) {
    T64_CHECK(advance_to(machine, machine->tick + 1u));
  }
  /* Reached: the Z triangle drew, so the depth members and the sidecar are
   * live behind the frame the VI is about to scan out. */
  T64_CHECK(machine->rdp.triangle_count != 0u && !machine->rdp.render_active);
  T64_CHECK(machine->rdp.depth_image_valid);
  T64_CHECK(hidden_nonzero(machine) != 0u);
  {
    /* the frame the VI scans out carries pixels the Z-tested triangle drew */
    size_t drawn = 0u;

    for (i = 0u; i < FRAME_PIXELS * 2u; i++) {
      if (machine->ram[VI_ORIGIN + i] != 0xa5u) drawn++;
    }
    T64_CHECK(drawn != 0u);
  }
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_CONTROL, 4u, 1u),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_DACRATE, 4u, SCAN_DACRATE),
                T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, AI_BITRATE, 4u, 15u),
                T64_STATUS_OK);
  queue_pcm_dma(machine, 0u);
  queue_pcm_dma(machine, 1u);
  write_vi_register(machine, UINT32_C(0x04), VI_ORIGIN);
  write_vi_register(machine, UINT32_C(0x08), UINT32_C(4));
  write_vi_register(machine, UINT32_C(0x18), UINT32_C(8));
  write_vi_register(machine, UINT32_C(0x1c), UINT32_C(15));
  write_vi_register(machine, UINT32_C(0x24), UINT32_C(4));
  write_vi_register(machine, UINT32_C(0x28), UINT32_C(2));
  write_vi_register(machine, UINT32_C(0x30), UINT32_C(0x400));
  write_vi_register(machine, UINT32_C(0x34), UINT32_C(0x400));
  write_vi_register(machine, UINT32_C(0x00), UINT32_C(2));
  for (i = 0u; i < 512u && machine->vi.packet_count == 0u; i++) {
    if (!next_event_tick(machine, true, &tick) || !advance_to(machine, tick)) {
      break;
    }
  }
  T64_CHECK(machine->vi.packet_count >= 1u);
  T64_CHECK(machine->vi.packet_lengths[machine->vi.packet_head] > 0u);
  /* Inside the next field: a few ticks past the boundary that retained the
   * packet, with the next field event still ahead. */
  T64_CHECK(advance_to(machine, machine->tick + 3u));
  T64_CHECK(next_event_tick(machine, true, &tick) && tick > machine->tick);
  return machine;
}

/* Drain every retained packet of both machines and compare them (the packet
 * observations by field, the bytes by memcmp); returns the number of video and
 * audio packets drained. */
static void drain_compare(const char *label, t64_machine *a, t64_machine *b,
                          size_t *video_out, size_t *audio_out) {
  uint8_t *left = (uint8_t *)malloc(T64_VIDEO_MAX_PACKET_BYTES);
  uint8_t *right = (uint8_t *)malloc(T64_VIDEO_MAX_PACKET_BYTES);

  *video_out = 0u;
  *audio_out = 0u;
  if (left == NULL || right == NULL) {
    T64_CHECK(false);
    free(left);
    free(right);
    return;
  }
  T64_CHECK_INT(a->vi.packet_count, b->vi.packet_count);
  T64_CHECK_INT(a->ai.packet_count, b->ai.packet_count);
  while (a->vi.packet_count > 0u && b->vi.packet_count > 0u) {
    t64_video_packet_info ia = T64_VIDEO_PACKET_INFO_INIT;
    t64_video_packet_info ib = T64_VIDEO_PACKET_INFO_INIT;
    size_t la = 0u;
    size_t lb = 0u;

    T64_CHECK_INT(t64_video_info(a, &ia), T64_STATUS_OK);
    T64_CHECK_INT(t64_video_info(b, &ib), T64_STATUS_OK);
    T64_CHECK_INT(ia.sequence, ib.sequence);
    T64_CHECK_INT(ia.start_tick, ib.start_tick);
    T64_CHECK_INT(ia.end_tick, ib.end_tick);
    T64_CHECK_INT(ia.packet_bytes, ib.packet_bytes);
    T64_CHECK_INT(ia.source_address, ib.source_address);
    T64_CHECK_INT(ia.field, ib.field);
    T64_CHECK_INT(t64_video_copy(a, left, T64_VIDEO_MAX_PACKET_BYTES, &la),
                  T64_STATUS_OK);
    T64_CHECK_INT(t64_video_copy(b, right, T64_VIDEO_MAX_PACKET_BYTES, &lb),
                  T64_STATUS_OK);
    T64_CHECK_INT(la, lb);
    T64_CHECK(la != 0u && la <= T64_VIDEO_MAX_PACKET_BYTES &&
              memcmp(left, right, la) == 0);
    (*video_out)++;
  }
  free(left);
  free(right);
  left = (uint8_t *)malloc(T64_AUDIO_MAX_PACKET_BYTES);
  right = (uint8_t *)malloc(T64_AUDIO_MAX_PACKET_BYTES);
  if (left == NULL || right == NULL) {
    T64_CHECK(false);
    free(left);
    free(right);
    return;
  }
  while (a->ai.packet_count > 0u && b->ai.packet_count > 0u) {
    t64_audio_packet_info ia = T64_AUDIO_PACKET_INFO_INIT;
    t64_audio_packet_info ib = T64_AUDIO_PACKET_INFO_INIT;
    size_t la = 0u;
    size_t lb = 0u;

    T64_CHECK_INT(t64_audio_info(a, &ia), T64_STATUS_OK);
    T64_CHECK_INT(t64_audio_info(b, &ib), T64_STATUS_OK);
    T64_CHECK_INT(ia.sequence, ib.sequence);
    T64_CHECK_INT(ia.first_sample_index, ib.first_sample_index);
    T64_CHECK_INT(ia.first_tick, ib.first_tick);
    T64_CHECK_INT(ia.last_tick, ib.last_tick);
    T64_CHECK_INT(ia.byte_length, ib.byte_length);
    T64_CHECK_INT(t64_audio_copy(a, left, T64_AUDIO_MAX_PACKET_BYTES, &la),
                  T64_STATUS_OK);
    T64_CHECK_INT(t64_audio_copy(b, right, T64_AUDIO_MAX_PACKET_BYTES, &lb),
                  T64_STATUS_OK);
    T64_CHECK_INT(la, lb);
    T64_CHECK(la != 0u && memcmp(left, right, la) == 0);
    (*audio_out)++;
  }
  (void)label;
  free(left);
  free(right);
}

/* The save bytes of both machines, through the public export, and the busy
 * flag of the device; equal on both sides. */
static bool save_state_equal(const t64_machine *a, const t64_machine *b,
                             uint32_t kind, uint8_t *copy_a, uint8_t *copy_b) {
  size_t la = 0u;
  size_t lb = 0u;
  t64_save_info ia = T64_SAVE_INFO_INIT;
  t64_save_info ib = T64_SAVE_INFO_INIT;

  if (t64_machine_save_export(a, kind, T64_SAVE_FORMAT_RAW, copy_a, 32768u,
                              &la) != T64_STATUS_OK ||
      t64_machine_save_export(b, kind, T64_SAVE_FORMAT_RAW, copy_b, 32768u,
                              &lb) != T64_STATUS_OK ||
      t64_machine_save_info(a, kind, &ia) != T64_STATUS_OK ||
      t64_machine_save_info(b, kind, &ib) != T64_STATUS_OK) {
    return false;
  }
  return la == lb && memcmp(copy_a, copy_b, la) == 0 &&
         (ia.flags & T64_SAVE_INFO_BUSY) == (ib.flags & T64_SAVE_INFO_BUSY);
}

typedef enum { SAVE_NONE = 0, SAVE_SRAM, SAVE_EEPROM } save_write_kind;

/* A state exported inside a scan-out with a retained video packet, two
 * retained audio packets and the depth members live, imported into a fresh
 * machine of the same id: the retained packets are drained and compared, then
 * both machines run through three more field boundaries (a PCM DMA queued on
 * both at each) and agree on the digest, the video packet bytes, the audio
 * packet bytes and the save bytes at each. One save write before the export
 * makes the save class part of the comparison. */
static void check_mid_scan_out(const char *profile_id, save_write_kind save) {
  t64_machine *source = build_mid_scan_out(profile_id);
  t64_machine *peer;
  t64_state_import_report report = T64_STATE_IMPORT_REPORT_INIT;
  t64_status status = T64_STATUS_OK;
  uint8_t *blob;
  uint8_t *copy_a = (uint8_t *)malloc(32768u);
  uint8_t *copy_b = (uint8_t *)malloc(32768u);
  size_t length = 0u;
  size_t boundaries = 0u;
  size_t events = 0u;
  size_t video = 0u;
  size_t audio = 0u;
  size_t total_video = 0u;
  size_t total_audio = 0u;
  uint32_t kind = 0u;
  uint64_t exported_tick = 0u;

  if (source == NULL || copy_a == NULL || copy_b == NULL) {
    free(copy_a);
    free(copy_b);
    if (source != NULL) t64_machine_destroy(source);
    T64_CHECK(false);
    return;
  }
  kind = source->profile.save_kind;
  if (save == SAVE_SRAM) {
    T64_CHECK_INT(t64_machine_bus_write(source, SRAM_WORD, 4u,
                                        UINT32_C(0xCAFEBABE)),
                  T64_STATUS_OK);
  } else if (save == SAVE_EEPROM) {
    static const uint8_t tx[10] = {0x05, 0x01, 0x11, 0x12, 0x13,
                                   0x14, 0x15, 0x16, 0x17, 0x18};
    uint8_t rx[1] = {0xFF};

    T64_CHECK_INT(t64_save_channel4_execute(source, source->tick, tx, 10u, rx,
                                            1u),
                  0u);
    T64_CHECK(source->save.busy_until > source->tick);
  }
  exported_tick = source->tick;
  blob = export_blob(source, &length);
  if (blob == NULL) {
    t64_machine_destroy(source);
    free(copy_a);
    free(copy_b);
    return;
  }
  peer = import_fresh(profile_id, blob, length, "mid-scan-out", &status,
                      &report);
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status == T64_STATUS_OK) {
    T64_CHECK(digests_equal(source, peer));
    T64_CHECK(peer->rdp.depth_image_valid);
    T64_CHECK(memcmp(peer->rdp_depth_hidden, source->rdp_depth_hidden,
                     HIDDEN_BYTES) == 0);
    T64_CHECK(hidden_nonzero(peer) != 0u);
    T64_CHECK_INT(peer->vi.packet_count, source->vi.packet_count);
    T64_CHECK(save == SAVE_NONE || save_state_equal(source, peer, kind, copy_a,
                                                    copy_b));
    /* the retained packets of the exported state */
    drain_compare("retained", source, peer, &video, &audio);
    total_video += video;
    total_audio += audio;
    T64_CHECK(video >= 1u);
    /* Field by field: the VI produces a retained packet on some of its field
     * events and stalls on a full ring, so the packets of both machines are
     * drained and compared whenever one exists. The digest and the sidecar are
     * compared at each of the first three packet-bearing boundaries and again
     * at the end; the loop runs on until the two retained PCM DMAs have played
     * out into audio packets, which the exported state could not yet hold. */
    while ((boundaries < 3u || total_audio < 2u) && events < 4000u) {
      uint64_t tick = 0u;
      uint64_t peer_tick = 0u;
      bool with_digest;

      T64_CHECK(next_event_tick(source, true, &tick));
      T64_CHECK(next_event_tick(peer, true, &peer_tick));
      T64_CHECK_INT(tick, peer_tick);
      if (!advance_pair(source, peer, tick, 0u)) break;
      events++;
      if (source->vi.packet_count == 0u && source->ai.packet_count == 0u) {
        continue;
      }
      with_digest = boundaries < 3u && source->vi.packet_count > 0u;
      if (with_digest) {
        T64_CHECK(digests_equal(source, peer));
        T64_CHECK(memcmp(source->rdp_depth_hidden, peer->rdp_depth_hidden,
                         HIDDEN_BYTES) == 0);
        T64_CHECK(save == SAVE_NONE ||
                  save_state_equal(source, peer, kind, copy_a, copy_b));
      }
      drain_compare("boundary", source, peer, &video, &audio);
      total_video += video;
      total_audio += audio;
      if (with_digest) {
        T64_CHECK(video >= 1u);
        boundaries++;
      }
    }
    T64_CHECK(digests_equal(source, peer));
    T64_CHECK(save == SAVE_NONE ||
              save_state_equal(source, peer, kind, copy_a, copy_b));
    T64_CHECK_INT(boundaries, 3u);
    T64_CHECK(total_audio >= 2u);
    T64_CHECK(events < 4000u);
    if (save == SAVE_SRAM) {
      T64_CHECK_INT(rd32(source->save.bytes + 0x104u), UINT32_C(0xCAFEBABE));
    } else if (save == SAVE_EEPROM) {
      T64_CHECK_INT(source->save.bytes[8], 0x11u);
    }
    fprintf(stderr,
            "mid-scan-out: %s imported at tick %llu; %zu field boundaries "
            "compared with digests, %zu field events run, %zu video and %zu "
            "audio packets drained equal%s\n",
            profile_id, (unsigned long long)exported_tick, boundaries, events,
            total_video, total_audio,
            save == SAVE_NONE ? "" : ", save bytes and busy flag equal");
  }
  if (peer != NULL) t64_machine_destroy(peer);
  free(blob);
  free(copy_a);
  free(copy_b);
  t64_machine_destroy(source);
}

/* A state 25 blob imported into a state 25 machine leaves the hidden depth
 * table zero and the depth address zero (decision K1: depth is inactive on the
 * family, so the importer neither reads nor applies the members): the peer's
 * table is dirtied byte by byte first, so a commit that kept it would show. */
static void check_v25_hidden_zero(void) {
  t64_machine *source = build_idle(SAVE_S32K_ID);
  t64_machine *peer = create_admitted(SAVE_S32K_ID);
  t64_state_import_report report = T64_STATE_IMPORT_REPORT_INIT;
  uint8_t *blob;
  size_t length = 0u;
  size_t i;
  size_t zero = 0u;
  size_t hidden_bytes;

  if (source == NULL || peer == NULL) {
    T64_CHECK(false);
    if (source != NULL) t64_machine_destroy(source);
    if (peer != NULL) t64_machine_destroy(peer);
    return;
  }
  hidden_bytes = T64_RDP_DEPTH_HIDDEN_BYTES(peer->profile.rdram_bytes);
  T64_CHECK(peer->rdp_depth_hidden != NULL);
  T64_CHECK_INT(hidden_bytes, HIDDEN_BYTES);
  T64_CHECK_INT(peer->profile.state_serialization_version, 25u);
  blob = export_blob(source, &length);
  if (blob != NULL && peer->rdp_depth_hidden != NULL) {
    t64_status status;

    for (i = 0u; i < hidden_bytes; i++) {
      peer->rdp_depth_hidden[i] = (uint8_t)(0x80u | (i & 3u));
    }
    peer->rdp.depth_image_address = DEPTH_IMAGE_ADDRESS;
    peer->rdp.depth_image_valid = true;
    peer->rdp.primitive_z = 0x1234u;
    peer->rdp.primitive_depth_valid = true;
    status = t64_machine_state_import(peer, blob, length, 0u, &report);
    T64_CHECK_INT(status, T64_STATUS_OK);
    if (status == T64_STATUS_OK) {
      for (i = 0u; i < hidden_bytes; i++) {
        if (peer->rdp_depth_hidden[i] == 0u) zero++;
      }
      T64_CHECK_INT(zero, hidden_bytes);
      T64_CHECK_INT(peer->rdp.depth_image_address, 0u);
      T64_CHECK(!peer->rdp.depth_image_valid);
      T64_CHECK_INT(peer->rdp.primitive_z, 0u);
      T64_CHECK(!peer->rdp.primitive_depth_valid);
      T64_CHECK(digests_equal(source, peer));
      fprintf(stderr,
              "v25-hidden-zero: %s state 25 import left %zu of %zu hidden "
              "bytes zero (T64_RDP_DEPTH_HIDDEN_BYTES), depth address 0\n",
              SAVE_S32K_ID, zero, hidden_bytes);
    }
  }
  free(blob);
  t64_machine_destroy(peer);
  t64_machine_destroy(source);
}

static void case_roundtrip(void) {
  static const char *const ids[4] = {V2_STEM, V2_E4K_ID, V2_E16K_ID,
                                     V2_S32K_ID};
  size_t i;

  for (i = 0u; i < 4u; i++) {
    check_idle_roundtrip(ids[i]);
  }
  check_mid_z_triangle(V2_STEM);
  check_mid_scan_out(V2_S32K_ID, SAVE_SRAM);
  check_mid_scan_out(V2_E4K_ID, SAVE_EEPROM);
  check_v25_hidden_zero();
}


/* ======================================================================== */
/* The hostile rows, the preflight parity and the v26 fuzz windows           */
/* ======================================================================== */

/* Defined in core/tests/fuzz_state_import.c (declared there as well). */
void state_fuzz_campaign_v26(const uint8_t *blob, size_t length,
                             uint64_t iterations, uint64_t seed,
                             state_fuzz_stats *stats);

/* ---- the independently authored inventory (docs/spec/state-mutants-v26.txt) */

typedef struct {
  const char *name;
  uint32_t value;
} nv;

#define NV_STATUS(x) {#x, (uint32_t)T64_STATUS_##x}
#define NV_REASON(x) {#x, (uint32_t)T64_STATE_REASON_##x}
#define NV_RULE(x) {#x, (uint32_t)T64_STATE_RULE_##x}

static const nv status_names[] = {
    NV_STATUS(STATE_FORMAT),   NV_STATUS(STATE_INTEGRITY),
    NV_STATUS(STATE_VERSION),  NV_STATUS(STATE_PROFILE),
    NV_STATUS(STATE_CONTENT),  NV_STATUS(STATE_BUILD),
    NV_STATUS(STATE_RANGE),    NV_STATUS(STATE_INVARIANT)};

static const nv reason_names[] = {
    NV_REASON(STATE_VERSION),    NV_REASON(SECTION_BOUNDS),
    NV_REASON(FIELD_RANGE),      NV_REASON(FIELD_DERIVED),
    NV_REASON(FIELD_PADDING),    NV_REASON(FIELD_RESERVED),
    NV_REASON(CONTAINER_VERSION), NV_REASON(TOTAL_LENGTH)};

static const nv rule_names[] = {
    NV_RULE(NONE),          NV_RULE(COUNT_CAP),    NV_RULE(INDEX_LT),
    NV_RULE(ENUM_CLOSED),   NV_RULE(BOOL_01),      NV_RULE(ALIGN),
    NV_RULE(ADDR_RANGE),    NV_RULE(LEN_LE),       NV_RULE(PROFILE_DERIVED),
    NV_RULE(DERIVED_EQ)};

static bool nv_lookup(const nv *table, size_t count, const char *name,
                      uint32_t *out) {
  size_t i;

  for (i = 0u; i < count; i++) {
    if (strcmp(table[i].name, name) == 0) {
      *out = table[i].value;
      return true;
    }
  }
  return false;
}

#define NV_FIND(table, name, out) \
  nv_lookup(table, sizeof table / sizeof table[0], name, out)

typedef struct {
  char name[64];
  uint32_t status;
  uint32_t reason;
  uint32_t section;
  uint32_t rule;
} inventory_row;

#define INVENTORY_MAX 48u

static size_t inventory_rows;
static inventory_row inventory[INVENTORY_MAX];

static bool inventory_load(void) {
  const char *path = getenv("T64_STATE_MUTANT_INVENTORY_V26");
  FILE *file;
  char line[256];
  bool ok = true;

  if (path == NULL || path[0] == '\0') return false;
  file = fopen(path, "r");
  if (file == NULL) return false;
  inventory_rows = 0u;
  while (fgets(line, sizeof line, file) != NULL) {
    size_t length = strlen(line);
    char *field[5];
    size_t n = 0u;
    char *cursor = line;
    inventory_row *item;

    while (length > 0u && (line[length - 1u] == '\n' || line[length - 1u] == '\r')) {
      line[--length] = '\0';
    }
    if (length == 0u || line[0] == '#') continue;
    while (n < 5u) {
      char *sep = strstr(cursor, " :: ");

      field[n++] = cursor;
      if (sep == NULL) break;
      *sep = '\0';
      cursor = sep + 4;
    }
    if (n != 5u || inventory_rows >= INVENTORY_MAX) {
      ok = false;
      break;
    }
    item = &inventory[inventory_rows++];
    (void)snprintf(item->name, sizeof item->name, "%s", field[0]);
    item->section = (uint32_t)strtoul(field[3], NULL, 10);
    if (!NV_FIND(status_names, field[1], &item->status) ||
        !NV_FIND(reason_names, field[2], &item->reason) ||
        !NV_FIND(rule_names, field[4], &item->rule)) {
      fprintf(stderr, "inventory row %s: unknown name\n", item->name);
      ok = false;
    }
  }
  (void)fclose(file);
  return ok;
}

/* ---- the base blobs and the located fields -------------------------------- */

typedef struct {
  uint8_t *blob;
  size_t length;
  size_t base;      /* section 8 body offset in the blob */
  size_t body;      /* section 8 body length */
  size_t pending;   /* pending_triangle_words[0] */
  size_t scalars;   /* depth_image_address; the four others follow */
  size_t hidden_len; /* the hidden length word */
  size_t hidden;    /* the first hidden byte */
} base_blob;

typedef enum { BASE_IDLE = 0, BASE_Z, BASE_V25, BASE_COUNT } base_id;

static base_blob bases[BASE_COUNT];

/* The fields are located by the layout offsets of the document and every
 * located value is asserted against the machine the blob came from, so a layout
 * error cannot patch the wrong bytes. */
static bool base_prepare(base_id id) {
  base_blob *b = &bases[id];
  t64_machine *machine;
  size_t n;
  size_t i;

  if (b->blob != NULL) return true;
  if (id == BASE_IDLE) machine = build_idle(V2_STEM);
  else if (id == BASE_Z) machine = build_mid_z_triangle(V2_STEM);
  else machine = build_idle(SI_STEM_ID);
  if (machine == NULL) return false;
  b->blob = export_blob(machine, &b->length);
  if (b->blob != NULL && find_section(b->blob, T64_STATE_SECTION_RDP_VI,
                                      &b->base, &b->body)) {
    n = rd32(b->blob + b->base + S8_PENDING_COUNT);
    T64_CHECK_INT(n, machine->rdp.pending_triangle_word_count);
    b->pending = b->base + S8_PENDING_WORDS;
    b->scalars = b->base + S8_SCALARS_BASE + 8u * n + S8_TRIANGLE_COVERAGE;
    b->hidden_len = b->base + b->body - HIDDEN_BYTES - 4u;
    b->hidden = b->base + b->body - HIDDEN_BYTES;
    if (id != BASE_V25) {
      T64_CHECK_INT(rd32(b->blob + b->scalars), machine->rdp.depth_image_address);
      T64_CHECK_INT(rd32(b->blob + b->scalars + 4u),
                    machine->rdp.depth_image_valid ? 1u : 0u);
      T64_CHECK_INT(rd32(b->blob + b->scalars + 8u), machine->rdp.primitive_z);
      T64_CHECK_INT(rd32(b->blob + b->scalars + 12u),
                    machine->rdp.primitive_dz);
      T64_CHECK_INT(rd32(b->blob + b->scalars + 16u),
                    machine->rdp.primitive_depth_valid ? 1u : 0u);
      T64_CHECK_INT(rd32(b->blob + b->hidden_len), HIDDEN_BYTES);
      for (i = 0u; i < HIDDEN_BYTES; i += 0x100u) {
        T64_CHECK_INT(b->blob[b->hidden + i], machine->rdp_depth_hidden[i]);
      }
      {
        /* the entries around the depth image (RDRAM 0x6000 is entry 0x3000):
         * the blob equals the machine, and a Z triangle in flight has written
         * some of them */
        size_t written = 0u;

        for (i = 0x3000u; i < 0x3040u; i++) {
          T64_CHECK_INT(b->blob[b->hidden + i], machine->rdp_depth_hidden[i]);
          if (machine->rdp_depth_hidden[i] != 0u) written++;
        }
        T64_CHECK((id == BASE_Z) == (written != 0u));
      }
      if (id == BASE_Z) {
        T64_CHECK_INT(n, 6u);
        T64_CHECK(rd64(b->blob + b->pending) ==
                  machine->rdp.pending_triangle_words[0]);
      }
      {
        /* the integrity fields of the exporter's blob equal a re-signing
         * from the container definition, so a refusal below is the rule */
        uint8_t *copy = (uint8_t *)malloc(b->length);

        T64_CHECK(copy != NULL);
        if (copy != NULL) {
          (void)memcpy(copy, b->blob, b->length);
          v26_resign(copy, b->length, RS_ALL);
          T64_CHECK(memcmp(copy, b->blob, b->length) == 0);
          free(copy);
        }
      }
    }
  } else {
    T64_CHECK(false);
  }
  t64_machine_destroy(machine);
  return b->blob != NULL && b->body != 0u;
}

static void bases_release(void) {
  size_t i;

  for (i = 0u; i < BASE_COUNT; i++) {
    free(bases[i].blob);
    bases[i].blob = NULL;
  }
}

/* A copy of a base blob with one 32-bit edit in section 8 and the integrity
 * fields re-signed with the domain text of the blob. */
static uint8_t *dup_base(base_id id, size_t *length_out) {
  uint8_t *copy;

  if (!base_prepare(id)) return NULL;
  copy = (uint8_t *)malloc(bases[id].length);
  if (copy == NULL) return NULL;
  (void)memcpy(copy, bases[id].blob, bases[id].length);
  *length_out = bases[id].length;
  return copy;
}

static void sign26(uint8_t *blob, size_t length) {
  blob_resign_entry(blob, length, T64_STATE_SECTION_RDP_VI);
  v26_resign(blob, length, RS_DECLARED | RS_TRAILER);
}

static uint8_t *edit_word(base_id id, size_t base_offset, uint32_t value,
                          size_t *length_out) {
  uint8_t *blob = dup_base(id, length_out);

  if (blob == NULL) return NULL;
  put32(blob + base_offset, value);
  sign26(blob, *length_out);
  return blob;
}

static uint8_t *m_depth_address_beyond_ram(size_t *length) {
  return edit_word(BASE_IDLE, bases[BASE_IDLE].scalars, 0x00400000u, length);
}
static uint8_t *m_depth_address_unaligned(size_t *length) {
  return edit_word(BASE_IDLE, bases[BASE_IDLE].scalars, 0x00006004u, length);
}
static uint8_t *m_depth_valid_two(size_t *length) {
  return edit_word(BASE_IDLE, bases[BASE_IDLE].scalars + 4u, 2u, length);
}
static uint8_t *m_primitive_z_wide(size_t *length) {
  return edit_word(BASE_IDLE, bases[BASE_IDLE].scalars + 8u, 0x00010000u,
                   length);
}
static uint8_t *m_primitive_dz_wide(size_t *length) {
  return edit_word(BASE_IDLE, bases[BASE_IDLE].scalars + 12u, 0x00010000u,
                   length);
}
static uint8_t *m_primitive_valid_two(size_t *length) {
  return edit_word(BASE_IDLE, bases[BASE_IDLE].scalars + 16u, 2u, length);
}

/* A hidden entry with the valid bit and a bit of the reserved field 6:2. */
static uint8_t *m_hidden_byte(size_t *length) {
  uint8_t *blob = dup_base(BASE_IDLE, length);

  if (blob == NULL) return NULL;
  blob[bases[BASE_IDLE].hidden + 0x3000u] = 0x84u;
  sign26(blob, *length);
  return blob;
}

static uint8_t *m_hidden_length_short(size_t *length) {
  return edit_word(BASE_IDLE, bases[BASE_IDLE].hidden_len, HIDDEN_BYTES - 4u,
                   length);
}

/* A consistent shorter layout: eight table bytes cut off the end of section 8
 * and the length word lowered to match, so the reader reaches the rule and not
 * a later misread. The structural encoder writes the state 25 domain text, which
 * is patched to the state 26 text before the re-signing. */
static uint8_t *m_hidden_truncated(size_t *length) {
  const base_blob *b = &bases[BASE_IDLE];
  tblob t;
  uint8_t *blob;
  size_t i;

  if (!base_prepare(BASE_IDLE)) return NULL;
  if (!tblob_decode(b->blob, b->length, &t)) return NULL;
  for (i = 0u; i < t.count &&
               t.section[i].id != T64_STATE_SECTION_RDP_VI; i++) {
  }
  if (i == t.count) {
    tblob_free(&t);
    return NULL;
  }
  put32(t.section[i].body + (b->hidden_len - b->base), HIDDEN_BYTES - 8u);
  t.section[i].length -= 8u;
  blob = tblob_encode(&t, length);
  tblob_free(&t);
  if (blob == NULL) return NULL;
  (void)memcpy(blob + (size_t)rd64(blob + D_CANON_OFF), V26_DOMAIN,
               D_DOMAIN_BYTES);
  v26_resign(blob, *length, RS_ALL);
  return blob;
}

static uint8_t *m_z_without_depth_image(size_t *length) {
  return edit_word(BASE_Z, bases[BASE_Z].scalars + 4u, 0u, length);
}

/* The last depth word of the first row is inside RDRAM, the pixel extent of
 * the triangle is not (the extent of the render proof, not the base rule). */
static uint8_t *m_z_extent_beyond_ram(size_t *length) {
  return edit_word(BASE_Z, bases[BASE_Z].scalars, 0x003ffff8u, length);
}

/* The Z plane derivatives and the major edge slope are the largest 16.16
 * values, so the products of the rows cannot be proven to fit; the extent of the
 * rows (the first and last row) does not move. */
static uint8_t *m_z_products(size_t *length) {
  uint8_t *blob = dup_base(BASE_Z, length);

  if (blob == NULL) return NULL;
  put64(blob + bases[BASE_Z].pending + 8u * 2u, UINT64_C(0x000100007fffffff));
  put64(blob + bases[BASE_Z].pending + 8u * 4u, UINT64_C(0x7fffffff7fffffff));
  put64(blob + bases[BASE_Z].pending + 8u * 5u, UINT64_C(0x7fffffff7fffffff));
  sign26(blob, *length);
  return blob;
}

/* The canonical domain text forged to the other state version and the whole
 * blob re-signed under the forged text. */
static uint8_t *m_v26_domain_25(size_t *length) {
  uint8_t *blob = dup_base(BASE_IDLE, length);

  if (blob == NULL) return NULL;
  (void)memcpy(blob + (size_t)rd64(blob + D_CANON_OFF), D_DOMAIN,
               D_DOMAIN_BYTES);
  blob_resign(blob, *length, RS_ALL);
  return blob;
}

static uint8_t *m_v25_domain_26(size_t *length) {
  uint8_t *blob = dup_base(BASE_V25, length);

  if (blob == NULL) return NULL;
  (void)memcpy(blob + (size_t)rd64(blob + D_CANON_OFF), V26_DOMAIN,
               D_DOMAIN_BYTES);
  v26_resign(blob, *length, RS_ALL);
  return blob;
}

static uint8_t *m_state_25_blob(size_t *length) {
  return dup_base(BASE_V25, length);
}

static uint8_t *m_state_26_blob(size_t *length) {
  return dup_base(BASE_IDLE, length);
}

typedef enum { TARGET_V26 = 0, TARGET_V25, TARGET_COUNT } target_id;

typedef struct {
  const char *name;
  uint32_t status;
  uint32_t reason;
  uint32_t section;
  uint32_t rule;
  target_id target;
  uint8_t *(*mutate)(size_t *length);
} hostile_row;

#define S8 ((uint32_t)T64_STATE_SECTION_RDP_VI)

/* The C table: the same rows in the same order as the inventory file, with the
 * tuple written with the closed names of the header. */
static const hostile_row hostile_rows[] = {
    {"v26-depth-address-beyond-ram", T64_STATUS_STATE_RANGE,
     T64_STATE_REASON_FIELD_RANGE, S8, T64_STATE_RULE_ADDR_RANGE, TARGET_V26,
     m_depth_address_beyond_ram},
    {"v26-depth-address-unaligned", T64_STATUS_STATE_RANGE,
     T64_STATE_REASON_FIELD_RANGE, S8, T64_STATE_RULE_ADDR_RANGE, TARGET_V26,
     m_depth_address_unaligned},
    {"v26-depth-valid-two", T64_STATUS_STATE_RANGE,
     T64_STATE_REASON_FIELD_RANGE, S8, T64_STATE_RULE_BOOL_01, TARGET_V26,
     m_depth_valid_two},
    {"v26-primitive-z-beyond-16-bits", T64_STATUS_STATE_RANGE,
     T64_STATE_REASON_FIELD_RANGE, S8, T64_STATE_RULE_INDEX_LT, TARGET_V26,
     m_primitive_z_wide},
    {"v26-primitive-dz-beyond-16-bits", T64_STATUS_STATE_RANGE,
     T64_STATE_REASON_FIELD_RANGE, S8, T64_STATE_RULE_INDEX_LT, TARGET_V26,
     m_primitive_dz_wide},
    {"v26-primitive-valid-two", T64_STATUS_STATE_RANGE,
     T64_STATE_REASON_FIELD_RANGE, S8, T64_STATE_RULE_BOOL_01, TARGET_V26,
     m_primitive_valid_two},
    {"v26-hidden-byte-outside-set", T64_STATUS_STATE_RANGE,
     T64_STATE_REASON_FIELD_RANGE, S8, T64_STATE_RULE_ENUM_CLOSED, TARGET_V26,
     m_hidden_byte},
    {"v26-hidden-length-short", T64_STATUS_STATE_RANGE,
     T64_STATE_REASON_FIELD_DERIVED, S8, T64_STATE_RULE_PROFILE_DERIVED,
     TARGET_V26, m_hidden_length_short},
    {"v26-hidden-table-truncated", T64_STATUS_STATE_RANGE,
     T64_STATE_REASON_FIELD_DERIVED, S8, T64_STATE_RULE_PROFILE_DERIVED,
     TARGET_V26, m_hidden_truncated},
    {"v26-z-triangle-without-depth-image", T64_STATUS_STATE_RANGE,
     T64_STATE_REASON_FIELD_RANGE, S8, T64_STATE_RULE_LEN_LE, TARGET_V26,
     m_z_without_depth_image},
    {"v26-z-triangle-depth-extent-beyond-ram", T64_STATUS_STATE_RANGE,
     T64_STATE_REASON_FIELD_RANGE, S8, T64_STATE_RULE_LEN_LE, TARGET_V26,
     m_z_extent_beyond_ram},
    {"v26-z-triangle-products", T64_STATUS_STATE_RANGE,
     T64_STATE_REASON_FIELD_RANGE, S8, T64_STATE_RULE_LEN_LE, TARGET_V26,
     m_z_products},
    {"v26-domain-state-25-text", T64_STATUS_STATE_FORMAT,
     T64_STATE_REASON_SECTION_BOUNDS, 0u, T64_STATE_RULE_NONE, TARGET_V26,
     m_v26_domain_25},
    {"v25-domain-state-26-text", T64_STATUS_STATE_FORMAT,
     T64_STATE_REASON_SECTION_BOUNDS, 0u, T64_STATE_RULE_NONE, TARGET_V25,
     m_v25_domain_26},
    {"v26-machine-state-25-blob", T64_STATUS_STATE_VERSION,
     T64_STATE_REASON_STATE_VERSION, 0u, T64_STATE_RULE_NONE, TARGET_V26,
     m_state_25_blob},
    {"v25-machine-state-26-blob", T64_STATUS_STATE_VERSION,
     T64_STATE_REASON_STATE_VERSION, 0u, T64_STATE_RULE_NONE, TARGET_V25,
     m_state_26_blob},
};
#define HOSTILE_COUNT (sizeof hostile_rows / sizeof hostile_rows[0])

/* The inventory file and the C table agree name by name, in order, tuple by
 * tuple, and the file has at least nine data rows. */
static void check_inventory(void) {
  size_t i;

  T64_CHECK(inventory_load());
  T64_CHECK_INT(inventory_rows, HOSTILE_COUNT);
  T64_CHECK(inventory_rows >= 9u);
  for (i = 0u; i < inventory_rows && i < HOSTILE_COUNT; i++) {
    const hostile_row *row = &hostile_rows[i];

    if (strcmp(inventory[i].name, row->name) != 0 ||
        inventory[i].status != (uint32_t)row->status ||
        inventory[i].reason != row->reason ||
        inventory[i].section != row->section || inventory[i].rule != row->rule) {
      fprintf(stderr, "inventory row %zu: %s does not match the table row %s\n",
              i, inventory[i].name, row->name);
      T64_CHECK(false);
    }
  }
}

static void run_hostile_row(const hostile_row *row, t64_machine *machine) {
  t64_state_import_report report = T64_STATE_IMPORT_REPORT_INIT;
  snapshot before;
  uint8_t *blob;
  size_t length = 0u;
  t64_status status;

  blob = row->mutate(&length);
  if (blob == NULL) {
    fprintf(stderr, "hostile row %s: no blob\n", row->name);
    T64_CHECK(false);
    return;
  }
  snapshot_take(machine, &before);
  poison_import_report(&report);
  status = t64_machine_state_import(machine, blob, length, 0u, &report);
  if (status != row->status || report.status != row->status ||
      report.reason != row->reason || report.section_id != row->section ||
      report.rule != row->rule) {
    fprintf(stderr,
            "hostile row %s: got status %u reason %u section %u rule %u, "
            "wanted %u %u %u %u\n",
            row->name, (unsigned)status, (unsigned)report.reason,
            (unsigned)report.section_id, (unsigned)report.rule,
            (unsigned)row->status, (unsigned)row->reason,
            (unsigned)row->section, (unsigned)row->rule);
    T64_CHECK(false);
  }
  T64_CHECK(snapshot_matches(machine, &before));
  T64_CHECK(t64_state_summary_is_clear(machine));
  snapshot_free(&before);
  free(blob);
}

/* The importer's Z product proof and the live acceptance preflight give the
 * same verdict for the same packet: a machine of the state 26 id holds the
 * registers of the Z triangle setup and the preflight of core/t64_rdp_depth.c
 * is called with the benign packet (accepted) and with the packet of the
 * hostile row (refused). */
static void check_preflight_parity(void) {
  t64_machine *machine = create_admitted(V2_STEM);
  uint64_t benign[6];
  uint64_t hostile[6];
  size_t i;

  if (machine == NULL) return;
  machine->rdp.depth_image_valid = true;
  machine->rdp.depth_image_address = DEPTH_IMAGE_ADDRESS;
  machine->rdp.color_image_width = 8u;
  /* primitive depth source with compare and update (bits 2, 4 and 5) */
  machine->rdp.other_modes = UINT64_C(0x0000000000000034);
  machine->rdp.primitive_depth_valid = true;
  for (i = 0u; i < 6u; i++) benign[i] = hostile[i] = z_triangle_words[7u + i];
  hostile[2] = UINT64_C(0x000100007fffffff);
  hostile[4] = UINT64_C(0x7fffffff7fffffff);
  hostile[5] = UINT64_C(0x7fffffff7fffffff);
  T64_CHECK(t64_rdp_depth_triangle_preflight(machine, 0x09u, benign, 6u, 0u, 8u,
                                             1u, 4u));
  T64_CHECK(!t64_rdp_depth_triangle_preflight(machine, 0x09u, hostile, 6u, 0u,
                                              8u, 1u, 4u));
  /* the same two packets through the importer: the benign one is the accepted
   * Z base blob, the hostile one is the products row above */
  fprintf(stderr,
          "preflight-parity: the live preflight accepts the benign packet and "
          "refuses the packet of v26-z-triangle-products\n");
  t64_machine_destroy(machine);
}

/* Windows over the new section 8 bytes of one base blob: the five scalars, the
 * hidden length word, the hidden entries around the sidecar the Z triangle
 * wrote and, for a Z triangle in flight, its packet words. */
static void fuzz_one_base(base_id id, const char *label, uint64_t iterations,
                          uint64_t seed) {
  state_fuzz_window windows[4];
  size_t count = 0u;
  state_fuzz_stats stats;
  const base_blob *b;

  if (!base_prepare(id)) return;
  b = &bases[id];
  windows[count].section = T64_STATE_SECTION_RDP_VI;
  windows[count].offset = b->scalars - b->base;
  windows[count].length = 20u;
  count++;
  windows[count].section = T64_STATE_SECTION_RDP_VI;
  windows[count].offset = b->hidden_len - b->base;
  windows[count].length = 4u;
  count++;
  windows[count].section = T64_STATE_SECTION_RDP_VI;
  windows[count].offset = b->hidden - b->base + 0x2ff0u;
  windows[count].length = 0x60u;
  count++;
  if (id == BASE_Z) {
    windows[count].section = T64_STATE_SECTION_RDP_VI;
    windows[count].offset = b->pending - b->base;
    windows[count].length = 8u * 6u;
    count++;
  }
  state_fuzz_set_windows(windows, count);
  state_fuzz_campaign_v26(b->blob, b->length, iterations, seed, &stats);
  state_fuzz_set_windows(NULL, 0u);
  fprintf(stderr,
          "fuzz-v26-%s: %llu iterations, %llu refused (%llu by a reader or a "
          "later check), %llu accepted, %llu violations\n",
          label, (unsigned long long)stats.iterations,
          (unsigned long long)stats.refusals,
          (unsigned long long)stats.field_level,
          (unsigned long long)stats.accepted,
          (unsigned long long)stats.violations);
  T64_CHECK_INT(stats.iterations, iterations);
  T64_CHECK_INT(stats.violations, 0u);
  /* reach: at least a quarter of the iterations get past the container level */
  T64_CHECK(stats.field_level + stats.accepted >=
            (iterations / 4u > 1u ? iterations / 4u : 1u));
}

static uint64_t env_u64(const char *name, uint64_t fallback) {
  const char *text = getenv(name);
  char *end = NULL;
  unsigned long long value;

  if (text == NULL || text[0] == '\0') return fallback;
  value = strtoull(text, &end, 10);
  return end != NULL && *end == '\0' ? (uint64_t)value : fallback;
}

static void case_hostile(void) {
  t64_machine *machines[TARGET_COUNT];
  size_t i;

  check_inventory();
  machines[TARGET_V26] = create_admitted(V2_STEM);
  machines[TARGET_V25] = create_admitted(SI_STEM_ID);
  T64_CHECK(machines[TARGET_V26] != NULL && machines[TARGET_V25] != NULL);
  if (machines[TARGET_V26] != NULL && machines[TARGET_V25] != NULL &&
      base_prepare(BASE_IDLE) && base_prepare(BASE_Z) &&
      base_prepare(BASE_V25)) {
    /* the accepted baselines of the same setup: each base blob imports into a
     * fresh machine of its own id before any mutant runs */
    for (i = 0u; i < BASE_COUNT; i++) {
      t64_state_import_report report = T64_STATE_IMPORT_REPORT_INIT;
      t64_status status = T64_STATUS_OK;
      t64_machine *fresh =
          import_fresh(i == BASE_V25 ? SI_STEM_ID : V2_STEM, bases[i].blob,
                       bases[i].length, "baseline", &status, &report);

      T64_CHECK_INT(status, T64_STATUS_OK);
      if (fresh != NULL) t64_machine_destroy(fresh);
    }
    for (i = 0u; i < HOSTILE_COUNT; i++) {
      run_hostile_row(&hostile_rows[i], machines[hostile_rows[i].target]);
    }
    fprintf(stderr, "hostile: %zu rows refused with their own tuple, machine "
                    "allocation and allocator counters unchanged\n",
            (size_t)HOSTILE_COUNT);
    check_preflight_parity();
    {
      const uint64_t iterations = env_u64("T64_STATE_FUZZ_ITERATIONS", 24u);

      fuzz_one_base(BASE_IDLE, "idle", iterations, UINT64_C(0x5EED2612));
      fuzz_one_base(BASE_Z, "mid-z-triangle", iterations,
                    UINT64_C(0x7121A926));
    }
  }
  for (i = 0u; i < TARGET_COUNT; i++) {
    if (machines[i] != NULL) t64_machine_destroy(machines[i]);
  }
  bases_release();
}

int main(int argc, char **argv) {
  static const char *const cases[] = {"roundtrip", "hostile"};

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    size_t index;

    for (index = 0u; index < sizeof cases / sizeof cases[0]; index++) {
      printf("%s\n", cases[index]);
    }
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    if (strcmp(argv[2], "roundtrip") == 0) {
      case_roundtrip();
      return T64_TEST_FINISH("unit.core.state.import-v26-roundtrip");
    }
    if (strcmp(argv[2], "hostile") == 0) {
      case_hostile();
      return T64_TEST_FINISH("unit.core.state.import-v26-hostile");
    }
    fprintf(stderr, "unknown case: %s\n", argv[2]);
    return 2;
  }
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--list-cases|--case NAME]\n", argv[0]);
    return 2;
  }
  case_roundtrip();
  case_hostile();
  return T64_TEST_FINISH("unit.core.state.import-v26-all");
}
