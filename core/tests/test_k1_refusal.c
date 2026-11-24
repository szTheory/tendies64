/*
 * K1 characterization of Z refusal on the shipped composite ids (plan 10-02).
 *
 * Phase 10 adds depth only under new registered ids. These cases pin, before
 * any edit of core/t64_internal.h, that the shipped descriptor 11 id and the
 * four descriptor 12 ids refuse every depth command exactly as they do today:
 *
 *   dispatch-refusal: SetZImage (0x3e) and SetPrimDepth (0x2e) are refused
 *       when the command is dispatched (supported_opcode is false).
 *   apply-refusal:    the four Z triangle opcodes (0x09, 0x0b, 0x0d, 0x0f)
 *       pass dispatch and are refused when the command is applied.
 *
 * Each refusal records the status register, the refusal reason, the last
 * opcode field, both DPC counters, the untouched depth members and an
 * all-zero hidden depth table. The command words are copied from the bound
 * core/tests/test_rdp_depth.c; that file is never edited.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64_internal.h"
#include "t64_sha256.h"
#include "t64_test.h"

#define IPL3_ASSET_BYTES 4096u
#define COMMAND_ADDRESS UINT32_C(0x100)
#define DP_START UINT32_C(0x04100000)
#define DP_END (DP_START + UINT32_C(4))
#define SI_STEM "ntsc-cart-4m-openipl3-rdram-b-t1-si-v1"
#define AI_PROFILE_ID \
  "ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1"
#define K1_ID_COUNT 5u

/* The five shipped ids that must keep refusing depth: the descriptor 11 id
 * and the four descriptor 12 ids. */
static const char *const k1_ids[K1_ID_COUNT] = {
    AI_PROFILE_ID, SI_STEM, SI_STEM "-e4k", SI_STEM "-e16k",
    SI_STEM "-s32k"};

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

static bool read_openipl3_asset(uint8_t bytes[IPL3_ASSET_BYTES]) {
  const char *path = getenv("T64_TEST_IPL3_PATH");
  FILE *file;
  size_t length;
  int extra;
  int read_error;
  int close_error;

  if (path == NULL || path[0] == '\0' || bytes == NULL) return false;
  file = fopen(path, "rb");
  if (file == NULL) return false;
  length = fread(bytes, 1u, IPL3_ASSET_BYTES, file);
  extra = fgetc(file);
  read_error = ferror(file);
  close_error = fclose(file);
  return read_error == 0 && close_error == 0 && length == IPL3_ASSET_BYTES &&
         extra == EOF;
}

/* Create the machine for an id and admit the cleared openIPL3 asset. */
static t64_machine *create_admitted(const char *profile_id) {
  const t64_profile_descriptor *profile = NULL;
  t64_machine *machine = NULL;
  uint8_t asset[IPL3_ASSET_BYTES];
  t64_status status;

  T64_CHECK_INT(t64_profile_lookup(profile_id, &profile), T64_STATUS_OK);
  if (profile == NULL) return NULL;
  T64_CHECK_INT(t64_machine_create(&allocator, profile, &machine),
                T64_STATUS_OK);
  if (machine == NULL) return NULL;
  if (!read_openipl3_asset(asset)) {
    T64_CHECK(false);
    t64_machine_destroy(machine);
    return NULL;
  }
  status = t64_machine_admit_rom(machine, asset, sizeof asset);
  T64_CHECK_INT(status, T64_STATUS_OK);
  if (status != T64_STATUS_OK) {
    t64_machine_destroy(machine);
    return NULL;
  }
  return machine;
}

static void write_be64(uint8_t *bytes, uint64_t value) {
  uint32_t i;
  for (i = 0u; i < 8u; i++)
    bytes[i] = (uint8_t)(value >> (56u - i * 8u));
}

/* Same event pump as core/tests/test_rdp_depth.c dispatch_all. */
static bool dispatch_all(t64_machine *machine) {
  size_t step;

  for (step = 0u; step < 4096u && machine->queue_count != 0u; step++) {
    t64_event event = machine->events[machine->queue_head];
    machine->queue_head = (machine->queue_head + 1u) %
                          T64_EVENT_QUEUE_CAPACITY;
    machine->queue_count--;
    machine->tick = event.tick;
    T64_CHECK(event.handler != NULL);
    if (event.handler != NULL)
      T64_CHECK_INT(event.handler(machine, &event), T64_STATUS_OK);
  }
  T64_CHECK_INT(machine->queue_count, 0u);
  return machine->queue_count == 0u;
}

/* Run exactly one queued event; false when the queue is empty. */
static bool step_one(t64_machine *machine) {
  t64_event event;

  if (machine->queue_count == 0u) return false;
  event = machine->events[machine->queue_head];
  machine->queue_head = (machine->queue_head + 1u) % T64_EVENT_QUEUE_CAPACITY;
  machine->queue_count--;
  machine->tick = event.tick;
  T64_CHECK(event.handler != NULL);
  if (event.handler != NULL)
    T64_CHECK_INT(event.handler(machine, &event), T64_STATUS_OK);
  return true;
}

/* Same submission as core/tests/test_rdp_depth.c run_commands, without the
 * dispatch loop. */
static bool submit_commands(t64_machine *machine, const uint64_t *words,
                            size_t count) {
  size_t i;
  uint32_t end;

  if (machine == NULL || words == NULL || count == 0u ||
      count > (UINT32_MAX - COMMAND_ADDRESS) / 8u)
    return false;
  machine->queue_head = 0u;
  machine->queue_count = 0u;
  for (i = 0u; i < count; i++)
    write_be64(machine->ram + COMMAND_ADDRESS + i * 8u, words[i]);
  end = COMMAND_ADDRESS + (uint32_t)(count * 8u);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_START, 4u,
                                      COMMAND_ADDRESS), T64_STATUS_OK);
  T64_CHECK_INT(t64_machine_bus_write(machine, DP_END, 4u, end),
                T64_STATUS_OK);
  return true;
}

static bool run_commands(t64_machine *machine, const uint64_t *words,
                         size_t count) {
  return submit_commands(machine, words, count) && dispatch_all(machine);
}

/* Words common to every dispatch case, copied from test_rdp_depth.c. */
static const uint64_t setup_words[] = {
    UINT64_C(0x3f10000700005000), /* RGBA16 color image, width 8 */
    UINT64_C(0x2d00000000020020), /* scissor [0,8) x [0,8) */
    UINT64_C(0x2f00000000000034), /* primitive Z, compare, update */
};

/* The apply cases add the primitive color and combiner words the Z triangle
 * of test_rdp_depth.c is drawn with, so the refusal cannot be a missing-mode
 * refusal. SetZImage and SetPrimDepth are left out: these ids refuse them. */
static const uint64_t apply_setup_words[] = {
    UINT64_C(0x3f10000700005000), UINT64_C(0x2d00000000020020),
    UINT64_C(0x2f00000000000034),
    UINT64_C(0x3a00000000ff00ff), /* green primitive color, opaque alpha */
    UINT64_C(0xfc00000000018600), /* literal combiner selects primitive */
};

/* SetZImage and SetPrimDepth words of test_rdp_depth.c. */
static const uint64_t depth_state_words[2] = {
    UINT64_C(0x3e00000000006000), /* aligned byte-addressed Z image */
    UINT64_C(0x2e00000013570009), /* primitive Z=0x1357, dZ=9 */
};

/* The four Z triangle packets of case_refusal_truncation_and_recovery in
 * core/tests/test_rdp_depth.c, with their full word counts (6, 14, 14, 22). */
static const uint64_t packet_09[] = {
    UINT64_C(0x09000014000c0004), UINT64_C(0x00010000fffe8000),
    UINT64_C(0x0001000000008000), UINT64_C(0x0004000000018000),
    UINT64_C(0x0000000000000000), UINT64_C(0x0000000000000000),
};
static const uint64_t packet_0b[] = {
    UINT64_C(0x0b000014000c0004), UINT64_C(0x00010000fffe8000),
    UINT64_C(0x0001000000008000), UINT64_C(0x0004000000018000),
    UINT64_C(0x0000000000010000), 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    UINT64_C(0x0000000000000000), UINT64_C(0x0000000000000000),
};
static const uint64_t packet_0d[] = {
    UINT64_C(0x0d000014000c0004), UINT64_C(0x00010000fffe8000),
    UINT64_C(0x0001000000008000), UINT64_C(0x0004000000018000),
    UINT64_C(0x0080004000c000ff), 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    UINT64_C(0x0000000000000000), UINT64_C(0x0000000000000000),
};
static const uint64_t packet_0f[] = {
    UINT64_C(0x0f000014000c0004), UINT64_C(0x00010000fffe8000),
    UINT64_C(0x0001000000008000), UINT64_C(0x0004000000018000),
    UINT64_C(0x0080004000c000ff), 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    UINT64_C(0x0000000000010000), 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    UINT64_C(0x0000000000000000), UINT64_C(0x0000000000000000),
};

typedef struct {
  const uint64_t *words;
  size_t count;
  uint32_t opcode;
} z_triangle;

#define Z_TRIANGLE_COUNT 4u
static const z_triangle z_triangles[Z_TRIANGLE_COUNT] = {
    {packet_09, sizeof packet_09 / sizeof packet_09[0], 0x09u},
    {packet_0b, sizeof packet_0b / sizeof packet_0b[0], 0x0bu},
    {packet_0d, sizeof packet_0d / sizeof packet_0d[0], 0x0du},
    {packet_0f, sizeof packet_0f / sizeof packet_0f[0], 0x0fu},
};

/*
 * Machine digests after each refusal, captured from the tree before the
 * Phase 10 depth predicate edit. Rows follow k1_ids; columns follow
 * depth_state_words and z_triangles. They depend only on the cleared openIPL3
 * asset, the authored command words and profile identity; no packet is
 * emitted, so the build identity does not enter them.
 */
static const char *const dispatch_digests[K1_ID_COUNT][2] = {
    {"2fbf7ca8dffcd0f3196db744b2cbb9632919269b7beca619fb05b0b89262822d",
     "b40cc4f413b12655c6ccf96200a4224126c092d0fa68e3d8ea2a66b01310f206"},
    {"8f537eed5ca9873c1c7785d9cb4bad395804c40ae7e395738ea2a3f2e0d8d2a5",
     "c5e5d02a320c263fa0eaf441df43489dff2f59d1c8d07fb4b0c7cfd4a8639314"},
    {"7847203580c13b5d8f2df937cf63160c6dea8ecff817e9727b9c4d9c659724f7",
     "fbbd9febbee53fb14db4038ba587c39ac77b2633c760625c47ab4f90b277c711"},
    {"3f0a0fac484b7540307ecaab68afa1fcfeaab58f9c83000b1f54d5676cd9fe12",
     "285a5a35ee81b19e647a8a3e977232e5b9f24f304a066ac3dd16fddb42356e6d"},
    {"ebdcd5036fe323d9406b876a43baf602842abdf6de19c8f79350ac2506a4e137",
     "160beaa97a15ca3857c07cd16b743af4733b5470f442b5e352f74eda6a426e0b"}
};
static const char *const apply_digests[K1_ID_COUNT][Z_TRIANGLE_COUNT] = {
    {"e25ff4462103a17596b25916fa654db148057e4cf4e7411153359cbb84ed2d8e",
     "f03a2e2a94b499bf5ddfeadf886329e51c184d7b7938413f4b1d810aee02c131",
     "7e063935c2242a8e22c7872af31a70a82bed4700d5bb1bbbdd08435698343332",
     "46949a016af1aebfdcbc0663ab2e03b93787d7867ecff1938a53e9a631a9a9df"},
    {"2eec1c70c8bb350d7e168eba60125e941c84276bb07f34bd3e923abcafa64968",
     "62f6e06992877750a10e83747039b7739ebc58b706edbda8cb672d909c42e7d6",
     "5d1570989649d1cb9a1111201858207be8ef7938c70ac1e434c149ab482ffa65",
     "d0ad2168b3989e0848d0a45bed0e37cb307de7c1b58ad626991934f87867d8aa"},
    {"b072ed9775045cece68036de317a3cdb5b1095514f6ae862760a424510fcb761",
     "c4f14644708e722861bb1ad27b076e70c9458122f1bc4caefbdeabb6bd5446f5",
     "a36d05f9704d036546025fcf765739e1fc255a1fad928342fca297012bc4e689",
     "6f03f5d50baad3ef6681228bc92660b2678593ba421c70b15970ab52255f67b4"},
    {"e1d7b011ec4bc1e7d0cf8cf85f1322b8f265b7579302e053aa72c9504100f9dc",
     "0a6aea589379ff7d949760c6abddabc9b15406d86d352eb6dcb30e5806b245c1",
     "8ca4cd807f3ff3bcb232aba1d11fa7cd264f2fb5a88466a0288dad7b118aa2cb",
     "4d96ab31347638916b1779b3f2a73a9631dc03b8d3b12e1cc4c56ec1e21569a3"},
    {"ba65df5d4ec2ac6b7c33bdf34193228da22575a982e2ba8927b96e59e4a406ed",
     "95cc4ad431080047a98dba48a1746f73aff86dbf4730a8bcb76d222f39525231",
     "d07e2b1d8899467fae5ccd1a6f87996416129f635d1eba27e71703e885247bc8",
     "fc9f9688904dfea899894ad4a4b895c456c9d47a5308ba68d66f7b8482e0128f"}
};

/* Increments the dispatch path makes for one command that reaches the pending
 * stage or is refused at dispatch: one clock count and one buffer-busy count
 * (core/t64_rdp.c, t64_rdp_event: the unsupported opcode branch and the
 * accepted-command tail make the same two increments). The apply stage of a
 * refused command makes none. */
#define DISPATCH_CLOCK_INCREMENT 1u
#define DISPATCH_BUSY_INCREMENT 1u

static void to_hex(const uint8_t digest[T64_DIGEST_SIZE],
                   char hex[2u * T64_DIGEST_SIZE + 1u]) {
  static const char digits[] = "0123456789abcdef";
  size_t i;

  for (i = 0u; i < T64_DIGEST_SIZE; i++) {
    hex[2u * i] = digits[digest[i] >> 4];
    hex[2u * i + 1u] = digits[digest[i] & 0x0fu];
  }
  hex[2u * T64_DIGEST_SIZE] = '\0';
}

/* The machine digest after a refusal equals the literal captured before the
 * predicate edit. The line is printed so a recapture needs no debugger. */
static void check_digest(const t64_machine *machine, const char *profile_id,
                         uint32_t opcode, const char *want) {
  uint8_t digest[T64_DIGEST_SIZE];
  char hex[2u * T64_DIGEST_SIZE + 1u];

  T64_CHECK_INT(t64_machine_digest(machine, digest), T64_STATUS_OK);
  to_hex(digest, hex);
  printf("k1: id=%s opcode=0x%02x digest=%s\n", profile_id,
         (unsigned)opcode, hex);
  T64_CHECK_STR(hex, want);
}

/* Every depth member is as a machine that never saw a depth command. */
static void check_depth_untouched(const t64_machine *machine) {
  size_t i;
  size_t bytes = T64_RDP_DEPTH_HIDDEN_BYTES(machine->profile.rdram_bytes);

  T64_CHECK(!machine->rdp.depth_image_valid);
  T64_CHECK(!machine->rdp.primitive_depth_valid);
  T64_CHECK_INT(machine->rdp.depth_image_address, 0u);
  T64_CHECK_INT(machine->rdp.primitive_z, 0u);
  T64_CHECK_INT(machine->rdp.primitive_dz, 0u);
  T64_CHECK(machine->rdp_depth_hidden != NULL);
  if (machine->rdp_depth_hidden == NULL) return;
  for (i = 0u; i < bytes; i++) {
    if (machine->rdp_depth_hidden[i] != 0u) {
      T64_CHECK_INT(machine->rdp_depth_hidden[i], 0u);
      break;
    }
  }
}

/* The refusal the status register and the pipeline state show. */
static void check_refused(const t64_machine *machine, uint32_t opcode,
                          uint64_t word) {
  T64_CHECK_INT(machine->rdp.refusal_reason,
                T64_RDP_REFUSAL_UNSUPPORTED_OPCODE);
  T64_CHECK_INT(machine->rdp.last_opcode_field, opcode);
  T64_CHECK_INT(machine->rdp.last_original_word, word);
  T64_CHECK((machine->rdp.dpc_status & T64_RDP_STATUS_ERROR) != 0u);
  T64_CHECK((machine->rdp.dpc_status & T64_RDP_STATUS_BUSY) == 0u);
  T64_CHECK(!machine->rdp.busy);
  T64_CHECK(!machine->rdp.command_pending);
  T64_CHECK(!machine->rdp.render_active);
  T64_CHECK_INT(machine->queue_count, 0u);
}

static unsigned refusals_checked;

/* One SetZImage or SetPrimDepth word refused at dispatch on one id. */
static void check_dispatch_refusal(const char *profile_id, uint64_t word,
                                   const char *want_digest) {
  t64_machine *machine = create_admitted(profile_id);
  uint32_t opcode = (uint32_t)((word >> 56) & UINT64_C(0x3f));
  uint32_t clock_before;
  uint32_t busy_before;
  uint64_t commands_before;

  if (machine == NULL) return;
  T64_CHECK(run_commands(machine, setup_words,
                         sizeof setup_words / sizeof setup_words[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  clock_before = machine->rdp.dpc_clock_counter;
  busy_before = machine->rdp.dpc_buffer_busy_counter;
  commands_before = machine->rdp.command_count;
  check_depth_untouched(machine);

  T64_CHECK(run_commands(machine, &word, 1u));
  check_refused(machine, opcode, word);
  T64_CHECK_INT(machine->rdp.dpc_clock_counter,
                clock_before + DISPATCH_CLOCK_INCREMENT);
  T64_CHECK_INT(machine->rdp.dpc_buffer_busy_counter,
                busy_before + DISPATCH_BUSY_INCREMENT);
  T64_CHECK_INT(machine->rdp.command_count, commands_before);
  check_depth_untouched(machine);
  check_digest(machine, profile_id, opcode, want_digest);
  refusals_checked++;
  t64_machine_destroy(machine);
}

/* One Z triangle packet accepted at dispatch and refused at apply on one id. */
static void check_apply_refusal(const char *profile_id,
                                const z_triangle *triangle,
                                const char *want_digest) {
  t64_machine *machine = create_admitted(profile_id);
  uint32_t clock_before;
  uint32_t busy_before;
  uint32_t clock_dispatched;
  uint32_t busy_dispatched;
  uint64_t commands_before;

  if (machine == NULL) return;
  T64_CHECK_INT((triangle->words[0] >> 56) & UINT64_C(0x3f),
                triangle->opcode);
  T64_CHECK(run_commands(machine, apply_setup_words,
                         sizeof apply_setup_words /
                             sizeof apply_setup_words[0]));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  clock_before = machine->rdp.dpc_clock_counter;
  busy_before = machine->rdp.dpc_buffer_busy_counter;
  commands_before = machine->rdp.command_count;
  check_depth_untouched(machine);

  /* Dispatch: the packet is read and held, no refusal yet. */
  T64_CHECK(submit_commands(machine, triangle->words, triangle->count));
  T64_CHECK(step_one(machine));
  T64_CHECK_INT(machine->rdp.refusal_reason, T64_RDP_REFUSAL_NONE);
  T64_CHECK(machine->rdp.command_pending);
  T64_CHECK_INT(machine->rdp.pending_opcode_field, triangle->opcode);
  T64_CHECK_INT(machine->rdp.pending_triangle_word_count, triangle->count);
  T64_CHECK_INT(machine->rdp.dpc_clock_counter,
                clock_before + DISPATCH_CLOCK_INCREMENT);
  T64_CHECK_INT(machine->rdp.dpc_buffer_busy_counter,
                busy_before + DISPATCH_BUSY_INCREMENT);
  clock_dispatched = machine->rdp.dpc_clock_counter;
  busy_dispatched = machine->rdp.dpc_buffer_busy_counter;

  /* Apply: refused as an unsupported opcode, counters unchanged. */
  T64_CHECK(step_one(machine));
  check_refused(machine, triangle->opcode, triangle->words[0]);
  T64_CHECK_INT(machine->rdp.dpc_current, COMMAND_ADDRESS);
  T64_CHECK_INT(machine->rdp.dpc_clock_counter, clock_dispatched);
  T64_CHECK_INT(machine->rdp.dpc_buffer_busy_counter, busy_dispatched);
  T64_CHECK_INT(machine->rdp.command_count, commands_before);
  check_depth_untouched(machine);
  check_digest(machine, profile_id, triangle->opcode, want_digest);
  refusals_checked++;
  t64_machine_destroy(machine);
}

static void case_dispatch_refusal(void) {
  size_t id;
  size_t word;

  refusals_checked = 0u;
  for (id = 0u; id < K1_ID_COUNT; id++) {
    for (word = 0u; word < 2u; word++) {
      check_dispatch_refusal(k1_ids[id], depth_state_words[word],
                             dispatch_digests[id][word]);
    }
  }
  T64_CHECK_INT(refusals_checked, 10u);
  printf("k1: dispatch-refusal refusals=%u\n", refusals_checked);
}

static void case_apply_refusal(void) {
  size_t id;
  size_t packet;

  refusals_checked = 0u;
  for (id = 0u; id < K1_ID_COUNT; id++) {
    for (packet = 0u; packet < Z_TRIANGLE_COUNT; packet++) {
      check_apply_refusal(k1_ids[id], &z_triangles[packet],
                          apply_digests[id][packet]);
    }
  }
  /* 5 ids x 4 opcodes */
  T64_CHECK_INT(refusals_checked, 20u);
  printf("k1: apply-refusal refusals=%u\n", refusals_checked);
}

int main(int argc, char **argv) {
  static const char *const cases[] = {"dispatch-refusal", "apply-refusal"};

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    size_t index;

    for (index = 0u; index < sizeof cases / sizeof cases[0]; index++) {
      printf("%s\n", cases[index]);
    }
    return 0;
  }
  if (argc == 3 && strcmp(argv[1], "--case") == 0) {
    if (strcmp(argv[2], "dispatch-refusal") == 0) {
      case_dispatch_refusal();
      return T64_TEST_FINISH("unit.core.k1.dispatch-refusal");
    }
    if (strcmp(argv[2], "apply-refusal") == 0) {
      case_apply_refusal();
      return T64_TEST_FINISH("unit.core.k1.apply-refusal");
    }
    fprintf(stderr, "unknown case: %s\n", argv[2]);
    return 2;
  }
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--list-cases|--case NAME]\n", argv[0]);
    return 2;
  }
  case_dispatch_refusal();
  case_apply_refusal();
  return T64_TEST_FINISH("unit.core.k1");
}
