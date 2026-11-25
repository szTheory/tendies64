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
  (void)memset(report_out, 0xA5, sizeof *report_out);
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
  t64_state_import_report report;
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
  t64_state_import_report report;
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
    t64_video_packet_info ia;
    t64_video_packet_info ib;
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
    t64_audio_packet_info ia;
    t64_audio_packet_info ib;
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
  t64_save_info ia;
  t64_save_info ib;

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
  t64_state_import_report report;
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
  t64_state_import_report report;
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

int main(int argc, char **argv) {
  static const char *const cases[] = {"roundtrip"};

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
    fprintf(stderr, "unknown case: %s\n", argv[2]);
    return 2;
  }
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--list-cases|--case NAME]\n", argv[0]);
    return 2;
  }
  case_roundtrip();
  return T64_TEST_FINISH("unit.core.state.import-v26-all");
}
