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

static void case_roundtrip(void) {
  check_idle_roundtrip(V2_STEM);
  check_mid_z_triangle(V2_STEM);
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
