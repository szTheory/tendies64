/*
 * Phase 9 composite-profile identity tests (plan 09-01).
 *
 * predicate-matrix: every registered pre-Phase-9 id keeps its version
 * predicate row. The expected rows below are derived by hand from the
 * definitions in core/t64_internal.h (version tuples are descriptor, model,
 * state), not from running the predicates.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_internal.h"
#include "t64_test.h"

static bool test_allocate(void *context, size_t size, size_t alignment,
                          t64_host_storage *storage_out) {
  void *memory;

  (void)context;
  if (storage_out == NULL || size == 0u || alignment == 0u) return false;
  memory = malloc(size);
  if (memory == NULL || (uintptr_t)memory % alignment != 0u) {
    free(memory);
    return false;
  }
  storage_out->data = memory;
  storage_out->capacity = size;
  storage_out->alignment = alignment;
  return true;
}

static void test_release(void *context, t64_host_storage storage) {
  (void)context;
  free(storage.data);
}

static const t64_host_allocator allocator = {test_allocate, test_release,
                                             NULL};

/* Predicate bits, in the order of the plan's predicate list. */
#define P_AI (1u << 0)
#define P_V9 (1u << 1)
#define P_RDP_VI (1u << 2)
#define P_SCAN_V8 (1u << 3)
#define P_SCAN (1u << 4)
#define P_TRI (1u << 5)
#define P_TEX (1u << 6)
#define P_COMB (1u << 7)
#define P_BLEND (1u << 8)
#define P_DEPTH (1u << 9)
#define P_VR4300 (1u << 10)
#define P_T1 (1u << 11)
#define P_RDRAMP (1u << 12)
#define P_COMPAT_EXT (1u << 13)
#define P_RSP (1u << 14)
#define P_COMPAT (1u << 15)
/* Phase 9 family predicate; legacy rows never carry it. */
#define P_SI (1u << 16)

/* Every predicate reached through t64_has_rdp_vi_profile at descriptor 6+. */
#define CHAIN                                                                  \
  (P_VR4300 | P_T1 | P_RDRAMP | P_COMPAT_EXT | P_RSP | P_COMPAT)

typedef struct {
  const char *id;
  uint32_t expected;
} matrix_row;

static const matrix_row legacy_rows[] = {
    /* (1,1,5) */
    {"ntsc-cart-4m-skeleton", 0u},
    /* (2,2,8) */
    {"ntsc-cart-4m-openipl3-warm-v1", 0u},
    /* (3,3,9): VR4300 state only */
    {"ntsc-cart-4m-openipl3-vr4300-v1", P_VR4300},
    /* (3,4,10): compat policy by its own tuple */
    {"ntsc-cart-4m-openipl3-vr4300-compat-v1", P_VR4300 | P_COMPAT},
    /* (3,5,11): compat extension */
    {"ntsc-cart-4m-openipl3-vr4300-compat-v2",
     P_VR4300 | P_COMPAT_EXT | P_COMPAT},
    /* (3,6,12): RSP functional */
    {"ntsc-cart-4m-openipl3-rsp-functional-v1",
     P_VR4300 | P_COMPAT_EXT | P_RSP | P_COMPAT},
    /* (4,7,13): T1 policy */
    {"ntsc-cart-4m-openipl3-rdram-a-t1-v1",
     P_VR4300 | P_T1 | P_COMPAT_EXT | P_RSP | P_COMPAT},
    /* (4,8,13): RDRAM protocol */
    {"ntsc-cart-4m-openipl3-rdram-b-t1-v1", CHAIN},
    /* (6,11,16): first RDP/VI model */
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v2", P_RDP_VI | CHAIN},
    /* (7,12,17) */
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-triangle-v3",
     P_RDP_VI | P_TRI | CHAIN},
    /* (7,13,18) */
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-texture-v4",
     P_RDP_VI | P_TRI | P_TEX | CHAIN},
    /* (8,14,19) */
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-combiner-v5",
     P_RDP_VI | P_TRI | P_TEX | P_COMB | CHAIN},
    /* (9,15,20): depth needs descriptor 10 */
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-blender-v6",
     P_RDP_VI | P_TRI | P_TEX | P_COMB | P_BLEND | CHAIN},
    /* (10,16,21): first row with RDP depth */
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-depth-v7",
     P_RDP_VI | P_TRI | P_TEX | P_COMB | P_BLEND | P_DEPTH | CHAIN},
    /* (10,17,22): scanout v8 */
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v8",
     P_RDP_VI | P_SCAN_V8 | P_SCAN | P_TRI | P_TEX | P_COMB | P_BLEND |
         P_DEPTH | CHAIN},
    /* (10,18,23): filter v9 */
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9",
     P_V9 | P_RDP_VI | P_SCAN | P_TRI | P_TEX | P_COMB | P_BLEND | P_DEPTH |
         CHAIN},
    /* (11,19,24): audio; depth stays inactive (guarded by descriptor 10) */
    {"ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1",
     P_AI | P_V9 | P_RDP_VI | P_SCAN | P_TRI | P_TEX | P_COMB | P_BLEND |
         CHAIN},
};

#define LEGACY_ROW_COUNT (sizeof legacy_rows / sizeof legacy_rows[0])
#define AI_ROW_INDEX 16u

/*
 * Composite family stem: the AI row except the exact-tuple AI predicate is
 * false and the family predicate is true. RDP depth stays inactive (K1).
 */
static const matrix_row family_rows[] = {
    {"ntsc-cart-4m-openipl3-rdram-b-t1-si-v1",
     (P_V9 | P_RDP_VI | P_SCAN | P_TRI | P_TEX | P_COMB | P_BLEND | CHAIN |
      P_SI)},
};

#define FAMILY_ROW_COUNT (sizeof family_rows / sizeof family_rows[0])

static uint32_t observed_row(const t64_machine *machine) {
  uint32_t row = 0u;

  row |= t64_has_ai_profile(machine) ? P_AI : 0u;
  row |= t64_has_vi_filter_v9_profile(machine) ? P_V9 : 0u;
  row |= t64_has_rdp_vi_profile(machine) ? P_RDP_VI : 0u;
  row |= t64_has_vi_scanout_v8_profile(machine) ? P_SCAN_V8 : 0u;
  row |= t64_has_vi_scanout_profile(machine) ? P_SCAN : 0u;
  row |= t64_has_rdp_triangle_profile(machine) ? P_TRI : 0u;
  row |= t64_has_rdp_texture_profile(machine) ? P_TEX : 0u;
  row |= t64_has_rdp_combiner_profile(machine) ? P_COMB : 0u;
  row |= t64_has_rdp_blender_profile(machine) ? P_BLEND : 0u;
  row |= t64_has_rdp_depth_profile(machine) ? P_DEPTH : 0u;
  row |= t64_has_vr4300_cpu_state(machine) ? P_VR4300 : 0u;
  row |= t64_has_t1_policy(machine) ? P_T1 : 0u;
  row |= t64_has_rdram_protocol_policy(machine) ? P_RDRAMP : 0u;
  row |= t64_has_cpu_compat_extension_policy(machine) ? P_COMPAT_EXT : 0u;
  row |= t64_has_rsp_functional_profile(machine) ? P_RSP : 0u;
  row |= t64_has_cpu_compat_policy(machine) ? P_COMPAT : 0u;
  row |= t64_has_si_profile(machine) ? P_SI : 0u;
  return row;
}

static t64_machine *create_machine(const char *profile_id) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;

  T64_CHECK_INT(t64_profile_lookup(profile_id, &profile), T64_STATUS_OK);
  if (profile == NULL) return NULL;
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  return machine;
}

static void case_predicate_matrix(void) {
  size_t index;

  for (index = 0u; index < LEGACY_ROW_COUNT; index++) {
    const matrix_row *row = &legacy_rows[index];
    t64_machine *machine = create_machine(row->id);
    uint32_t observed;

    T64_CHECK(machine != NULL);
    if (machine == NULL) continue;
    observed = observed_row(machine);
    if (observed != row->expected) {
      fprintf(stderr, "predicate row differs for %s: got 0x%04x want 0x%04x\n",
              row->id, (unsigned)observed, (unsigned)row->expected);
    }
    T64_CHECK_INT(observed, row->expected);
    t64_machine_destroy(machine);
  }
  T64_CHECK_INT(LEGACY_ROW_COUNT, 17);

  /* The family rows equal the AI row except the AI and family predicates. */
  for (index = 0u; index < FAMILY_ROW_COUNT; index++) {
    const matrix_row *row = &family_rows[index];
    t64_machine *machine = create_machine(row->id);
    uint32_t observed;
    uint32_t ai_row = legacy_rows[AI_ROW_INDEX].expected;

    T64_CHECK(machine != NULL);
    if (machine == NULL) continue;
    observed = observed_row(machine);
    if (observed != row->expected) {
      fprintf(stderr, "predicate row differs for %s: got 0x%05x want 0x%05x\n",
              row->id, (unsigned)observed, (unsigned)row->expected);
    }
    T64_CHECK_INT(observed, row->expected);
    T64_CHECK_INT(row->expected, (ai_row & ~(uint32_t)P_AI) | P_SI);
    T64_CHECK((observed & P_DEPTH) == 0u);
    T64_CHECK(t64_has_ai_device(machine));
    T64_CHECK(!t64_has_ai_profile(machine));
    t64_machine_destroy(machine);
  }
  for (index = 0u; index < LEGACY_ROW_COUNT; index++) {
    t64_machine *machine = create_machine(legacy_rows[index].id);

    T64_CHECK(machine != NULL);
    if (machine == NULL) continue;
    T64_CHECK(t64_has_ai_device(machine) ==
              ((legacy_rows[index].expected & P_AI) != 0u));
    t64_machine_destroy(machine);
  }

  /* K1: depth is active on the three descriptor 10 rows only. */
  T64_CHECK((legacy_rows[13].expected & P_DEPTH) != 0u);
  T64_CHECK((legacy_rows[14].expected & P_DEPTH) != 0u);
  T64_CHECK((legacy_rows[15].expected & P_DEPTH) != 0u);
  T64_CHECK((legacy_rows[16].expected & P_DEPTH) == 0u);
  T64_CHECK((family_rows[0].expected & P_DEPTH) == 0u);
}

int main(int argc, char **argv) {
  static const char *const cases[] = {"predicate-matrix"};

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    size_t index;

    for (index = 0u; index < sizeof cases / sizeof cases[0]; index++) {
      printf("%s\n", cases[index]);
    }
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    if (strcmp(argv[2], "predicate-matrix") == 0) {
      case_predicate_matrix();
      return T64_TEST_FINISH("unit.core.profile.predicate-matrix");
    }
    fprintf(stderr, "unknown case: %s\n", argv[2]);
    return 2;
  }
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--list-cases|--case NAME]\n", argv[0]);
    return 2;
  }
  case_predicate_matrix();
  return T64_TEST_FINISH("unit.core.profile.si");
}
