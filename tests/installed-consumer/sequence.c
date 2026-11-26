/*
 * Call-sequence generator through the installed package (Phase 11, plan 11-20,
 * API-02, T-11-42). A deterministic sequence of public calls, drawn from a
 * constant-seed 64-bit LCG that this file owns (the core owns no PRNG), runs
 * against one instance of the authored composite profile. Before every
 * classified call the consumer snapshots what the public API can observe, and
 * after a call whose status the consumer's own class table
 * (t64c_classes.h) calls refused it asserts that the snapshot is bit-equal:
 *
 *   cheap snapshot  content digest, output status, the oldest video and audio
 *                   packet info, save info, CPU stop detail, ROM size and
 *                   header, the poison query and the allocator's live count and
 *                   bad releases; compared after every refused call
 *   machine digest  t64_machine_digest costs about 22 ms (the digest hashes
 *                   4 MiB of RDRAM), so it is compared over a refusal burst:
 *                   the digest is taken before the first refused call that
 *                   follows a state change and again before the next call
 *                   that may change state (or at the end); a refused call that
 *                   changed the machine fails the burst, and the failure names
 *                   the step range and its calls (--digest-stride N verifies
 *                   every Nth burst only, for the long sanitizer runs)
 *
 * Other rules checked on the way:
 *
 *   guest stops    every advance stop reason is a guest-stop row of the class
 *                  table; guest time never moves backwards and BOUND stops at
 *                  the bound; a terminal stop stays terminal; a larger bound
 *                  makes progress or stalls only with a full output ring and
 *                  resumes after a full drain
 *   poison         no caller fault, however malformed, poisons the instance:
 *                  the poison slot is read before every call; one deliberate
 *                  defect, late in the sequence, flips a bit of the profile
 *                  digest the instance keeps in the consumer's own allocator
 *                  storage (found exactly once with the public
 *                  t64_profile_hash) and calls t64_machine_admit_rom, which
 *                  must answer INTERNAL_DEFECT; after it every call but
 *                  destroy and the poison query answers POISONED with its
 *                  outputs untouched, and destroy releases everything
 *   bus write      t64_machine_bus_write is outside the no-op domain: a device
 *                  refusal in the PI or SI register windows may move only the
 *                  machine digest (its own latches); every other observation
 *                  stays equal, and every other refusal changes nothing
 *
 * Two fixed points of the grammar: the defect step (steps - steps/8) and two
 * milestones (steps/4 and steps/4 + steps/2) that replace the composite ROM,
 * run it to completion with no drain until the output ring stalls the guest,
 * drain, resume, and compare the final digest with the runner-recorded pin.
 *
 *   t64c --case sequence --rom PATH [--steps N] [--digest-stride N]
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tendies64/t64_machine.h>

#include "t64c_alloc.h"
#include "t64c_cases.h"
#include "t64c_check.h"
#include "t64c_classes.h"
#include "t64c_composite.h"

#define SEQ_SEED UINT64_C(0x7e4d1e5c0ffee064)
#define SEQ_DEFAULT_STEPS 400u
#define SEQ_MIN_STEPS 16u
#define SEQ_DEFAULT_STRIDE 2u
#define SEQ_SENTINEL 0xa5u
#define SEQ_U32_SENTINEL UINT32_C(0xa5a5a5a5)
#define SEQ_U64_SENTINEL UINT64_C(0xa5a5a5a5a5a5a5a5)
#define SEQ_SIZE_SENTINEL ((size_t)0x5a5a5a5au)
#define SEQ_SNAP_BYTES 4096u
#define SEQ_MAX_MARKS 16u
#define SEQ_HEADER_BYTES 64u
#define SEQ_STATUS_SLOTS 40u
#define SEQ_STOP_SLOTS 10u
#define SEQ_BLOCK_BYTES 4096u
#define SEQ_SAVE_BYTES 32768u
#define SEQ_OP_COUNT 16u

/* Flags of a classified call. */
#define SEQ_REFUSED 0x01u    /* refused by design: needs a baseline digest */
#define SEQ_PURE 0x02u       /* an observation: an ok answer changes nothing */
#define SEQ_ANY 0x04u        /* may change state: closes the open burst */
#define SEQ_NO_MACHINE 0x08u /* the call cannot reach the instance */
#define SEQ_BUS_WRITE 0x10u  /* t64_machine_bus_write: the carve-out applies */
#define SEQ_QUERY 0x20u      /* the poison query */

/* ---- state ------------------------------------------------------------------ */

typedef struct {
  uint8_t bytes[SEQ_SNAP_BYTES];
  size_t length;
  size_t mark_offset[SEQ_MAX_MARKS];
  const char *mark_name[SEQ_MAX_MARKS];
  unsigned marks;
} seq_snap;

typedef struct {
  t64_status status;
  uint8_t bytes[T64_DIGEST_SIZE];
} seq_digest;

typedef struct seq seq;
typedef void (*seq_op_fn)(seq *g, unsigned variant);

struct seq {
  t64c_fixture *fx;
  uint64_t rng;
  unsigned steps;
  unsigned step;
  unsigned defect_step;
  unsigned milestone_a;
  unsigned milestone_b;
  uint64_t digest_stride;
  unsigned fault;
  bool fault_done;
  bool abort;
  bool poisoned;
  bool defect_call;
  bool loaded;
  /* the host's mirror of guest facts */
  uint64_t tick;
  uint64_t instr_limit;
  uint64_t frame_limit;
  bool terminal;
  uint32_t terminal_reason;
  uint64_t terminal_tick;
  bool stalled;
  bool await_progress;
  uint32_t stall_kind;
  uint64_t stall_bound;
  uint64_t stall_tick;
  /* the call in flight */
  unsigned cur_flags;
  uint32_t cur_address;
  const char *cur_op;
  unsigned cur_variant;
  const char *cur_label;
  seq_snap before;
  seq_snap after;
  /* machine digest bursts */
  bool burst_open;
  bool burst_eligible;
  bool known_valid;
  bool pending_verify;
  seq_digest known;
  uint64_t burst_index;
  unsigned burst_first_step;
  char burst_log[256];
  /* the state container captured after the first admission, and scratch */
  uint8_t *blob;
  size_t blob_length;
  uint8_t save_pattern[SEQ_SAVE_BYTES];
  uint8_t save_buf[SEQ_SAVE_BYTES];
  /* counters */
  unsigned calls;
  unsigned class_counts[T64C_CLASS_UNKNOWN + 1u];
  unsigned status_counts[SEQ_STATUS_SLOTS];
  unsigned stop_counts[SEQ_STOP_SLOTS];
  unsigned stalls;
  unsigned resumed;
  unsigned terminals;
  unsigned milestones;
  unsigned poisoned_calls;
  unsigned bursts;
  unsigned digests_verified;
  unsigned digests_skipped;
  unsigned bus_latched;
  unsigned hits[SEQ_OP_COUNT][32];
  unsigned a5_slot;
  size_t a5_offset;
  unsigned a5_matches;
  t64c_tally tally;
};

typedef struct {
  const char *name;
  unsigned weight;
  seq_op_fn fn;
  unsigned variants;
  uint32_t mutating; /* bit i: variant i may change the instance */
  bool poison_ok;    /* may run against a poisoned instance */
} seq_op;


static t64_machine *seq_m(seq *g) { return g->fx->machine; }

/* ---- random numbers, checks and small helpers -------------------------------- */

static uint64_t seq_next(seq *g) {
  g->rng = g->rng * UINT64_C(6364136223846793005) + UINT64_C(1442695040888963407);
  return g->rng >> 17;
}

static unsigned seq_pick(seq *g, unsigned n) { return (unsigned)(seq_next(g) % (uint64_t)n); }

static void seq_context(const seq *g) {
  (void)fprintf(stderr, "  at step %u op %s variant %u call \"%s\" (tick %llu)\n", g->step,
                g->cur_op != NULL ? g->cur_op : "-", g->cur_variant,
                g->cur_label != NULL ? g->cur_label : "-", (unsigned long long)g->tick);
}

static void seq_check(seq *g, bool ok, const char *what) {
  g->tally.checks++;
  if (!ok) {
    g->tally.failures++;
    g->abort = true;
    (void)fprintf(stderr, "FAIL sequence: %s\n", what);
    seq_context(g);
  }
}

static void seq_fill(void *object, size_t size) { (void)memset(object, (int)SEQ_SENTINEL, size); }

/* A caller-owned struct poisoned with the sentinel, its leading struct_size kept. */
static void seq_fill_struct(void *object, size_t size, uint32_t struct_size) {
  seq_fill(object, size);
  (void)memcpy(object, &struct_size, sizeof struct_size);
}

static bool seq_is_fill(const void *object, size_t size) {
  const uint8_t *bytes = (const uint8_t *)object;
  size_t i;

  for (i = 0u; i < size; i++) {
    if (bytes[i] != SEQ_SENTINEL) {
      return false;
    }
  }
  return true;
}

/* Untouched struct: the struct_size the caller wrote, every other byte sentinel. */
static bool seq_struct_intact(const void *object, size_t size, uint32_t struct_size) {
  uint32_t seen;

  (void)memcpy(&seen, object, sizeof seen);
  return seen == struct_size &&
         seq_is_fill((const uint8_t *)object + sizeof seen, size - sizeof seen);
}

static void seq_hex(const uint8_t digest[T64_DIGEST_SIZE], char hex[2u * T64_DIGEST_SIZE + 1u]) {
  static const char digits[] = "0123456789abcdef";
  size_t i;

  for (i = 0u; i < T64_DIGEST_SIZE; i++) {
    hex[2u * i] = digits[digest[i] >> 4];
    hex[2u * i + 1u] = digits[digest[i] & 0x0fu];
  }
  hex[2u * T64_DIGEST_SIZE] = '\0';
}

/* ---- the cheap snapshot ------------------------------------------------------ */

static void snap_put(seq *g, seq_snap *s, const void *data, size_t size) {
  if (s->length + size > sizeof s->bytes) {
    seq_check(g, false, "the snapshot buffer is too small for the observation set");
    return;
  }
  (void)memcpy(s->bytes + s->length, data, size);
  s->length += size;
}

static void snap_status(seq *g, seq_snap *s, t64_status status) {
  snap_put(g, s, &status, sizeof status);
}

static void snap_mark(seq_snap *s, const char *name) {
  if (s->marks < SEQ_MAX_MARKS) {
    s->mark_offset[s->marks] = s->length;
    s->mark_name[s->marks] = name;
    s->marks++;
  }
}

/* Observe everything the public API shows without changing anything. The out
 * structs are filled with the sentinel first, so a call that answers a refusal
 * leaves a deterministic image and a successful copy-out is compared whole. */
static void seq_take(seq *g, seq_snap *s) {
  t64_machine *m = seq_m(g);
  uint8_t content[T64_DIGEST_SIZE];
  uint8_t header[SEQ_HEADER_BYTES];
  size_t header_length = SEQ_SIZE_SENTINEL;
  size_t rom_size = SEQ_SIZE_SENTINEL;
  t64_output_status ostat = T64_OUTPUT_STATUS_INIT;
  t64_video_packet_info video = T64_VIDEO_PACKET_INFO_INIT;
  t64_audio_packet_info audio = T64_AUDIO_PACKET_INFO_INIT;
  t64_save_info save = T64_SAVE_INFO_INIT;
  t64_cpu_stop_detail detail = T64_CPU_STOP_DETAIL_INIT;
  uint32_t poisoned_slot = SEQ_U32_SENTINEL;
  size_t outstanding = g->fx->ledger.outstanding;
  size_t bad_releases = g->fx->ledger.bad_releases;
  t64_status status;

  s->length = 0u;
  s->marks = 0u;
  seq_fill(content, sizeof content);
  snap_mark(s, "content digest");
  status = t64_machine_content_digest(m, content);
  snap_status(g, s, status);
  snap_put(g, s, content, sizeof content);

  seq_fill_struct(&ostat, sizeof ostat, (uint32_t)sizeof ostat);
  snap_mark(s, "output status");
  status = t64_machine_output_status(m, &ostat);
  snap_status(g, s, status);
  snap_put(g, s, &ostat, sizeof ostat);

  seq_fill_struct(&video, sizeof video, (uint32_t)sizeof video);
  snap_mark(s, "video info");
  status = t64_video_info(m, &video);
  snap_status(g, s, status);
  snap_put(g, s, &video, sizeof video);

  seq_fill_struct(&audio, sizeof audio, (uint32_t)sizeof audio);
  snap_mark(s, "audio info");
  status = t64_audio_info(m, &audio);
  snap_status(g, s, status);
  snap_put(g, s, &audio, sizeof audio);

  seq_fill_struct(&save, sizeof save, (uint32_t)sizeof save);
  snap_mark(s, "save info");
  status = t64_machine_save_info(m, T64_SAVE_SRAM_32K, &save);
  snap_status(g, s, status);
  snap_put(g, s, &save, sizeof save);

  seq_fill_struct(&detail, sizeof detail, (uint32_t)sizeof detail);
  snap_mark(s, "cpu stop detail");
  status = t64_machine_cpu_stop_detail(m, &detail);
  snap_status(g, s, status);
  snap_put(g, s, &detail, sizeof detail);

  snap_mark(s, "rom size and header");
  status = t64_machine_rom_size(m, &rom_size);
  snap_status(g, s, status);
  snap_put(g, s, &rom_size, sizeof rom_size);
  seq_fill(header, sizeof header);
  status = t64_machine_rom_header(m, header, sizeof header, &header_length);
  snap_status(g, s, status);
  snap_put(g, s, header, sizeof header);
  snap_put(g, s, &header_length, sizeof header_length);

  snap_mark(s, "poison query");
  status = t64_machine_is_poisoned(m, &poisoned_slot);
  snap_status(g, s, status);
  snap_put(g, s, &poisoned_slot, sizeof poisoned_slot);
  if (!g->poisoned && !g->defect_call) {
    seq_check(g, status == T64_STATUS_OK && poisoned_slot == 0u,
              "the instance was poisoned before the deliberate defect step");
  }

  snap_mark(s, "allocator counters");
  snap_put(g, s, &outstanding, sizeof outstanding);
  snap_put(g, s, &bad_releases, sizeof bad_releases);
}

/* The name of the observation that differs first. */
static const char *snap_first_difference(const seq_snap *a, const seq_snap *b) {
  size_t limit = a->length < b->length ? a->length : b->length;
  size_t i;
  const char *name = "length";
  unsigned k;

  for (i = 0u; i < limit && a->bytes[i] == b->bytes[i]; i++) {
  }
  if (i == limit && a->length == b->length) {
    return NULL;
  }
  for (k = 0u; k < a->marks; k++) {
    if (a->mark_offset[k] <= i) {
      name = a->mark_name[k];
    }
  }
  return name;
}

/* ---- the machine digest over a refusal burst ---------------------------------- */

static void seq_digest_take(seq *g, seq_digest *digest) {
  (void)memset(digest, 0, sizeof *digest);
  digest->status = t64_machine_digest(seq_m(g), digest->bytes);
}

static void seq_log(seq *g, const char *label) {
  const size_t used = strlen(g->burst_log);
  const size_t room = sizeof g->burst_log - used - 1u;
  size_t need = strlen(label) + 1u;

  if (need <= room) {
    (void)memcpy(g->burst_log + used, label, need - 1u);
    g->burst_log[used + need - 1u] = ';';
    g->burst_log[used + need] = '\0';
  }
}

/* Verify the refusals of the open burst against the digest taken before them. */
static void seq_verify(seq *g) {
  seq_digest now;
  bool equal;

  if (g->pending_verify && g->known_valid) {
    seq_digest_take(g, &now);
    g->digests_verified++;
    equal = now.status == g->known.status &&
            memcmp(now.bytes, g->known.bytes, sizeof now.bytes) == 0;
    if (!equal) {
      (void)fprintf(stderr,
                    "FAIL sequence: a refused call changed the machine digest: refusal burst "
                    "from step %u, calls: %s\n",
                    g->burst_first_step, g->burst_log);
    }
    seq_check(g, equal, "the machine digest is bit-equal after a burst of refused calls");
  }
  g->pending_verify = false;
}

/* Close the open burst: verify it, and forget the digest. */
static void seq_close_burst(seq *g) {
  seq_verify(g);
  g->burst_open = false;
  g->known_valid = false;
  g->burst_log[0] = '\0';
}

/* Raw calls that change the instance without a classified bracket. */
static void seq_unbracketed_mutation(seq *g) { seq_close_burst(g); }

/* ---- the classified call: begin and end --------------------------------------- */

static void seq_begin(seq *g, unsigned flags) {
  g->cur_flags = flags;
  g->cur_address = 0u;
  seq_take(g, &g->before);
  if (g->poisoned || g->defect_call || (flags & SEQ_NO_MACHINE) != 0u) {
    return;
  }
  if ((flags & SEQ_REFUSED) != 0u) {
    if (!g->burst_open) {
      g->burst_open = true;
      g->burst_index++;
      g->bursts++;
      g->burst_first_step = g->step;
      g->burst_log[0] = '\0';
      g->burst_eligible = g->digest_stride == 1u || g->burst_index % g->digest_stride == 0u;
      if (g->burst_eligible) {
        seq_digest_take(g, &g->known);
        g->known_valid = true;
      } else {
        g->known_valid = false;
      }
    }
  } else if ((flags & SEQ_ANY) != 0u) {
    seq_verify(g);
  }
}

static bool seq_pi_si_window(uint32_t address) {
  return (address >= UINT32_C(0x04600000) && address < UINT32_C(0x04700000)) ||
         (address >= UINT32_C(0x04800000) && address < UINT32_C(0x04900000));
}

/* The self-test of the generator (--seed-fault N): one deliberate fault of the
 * harness's own, so that its checks are seen to fail. 1: a refused call that
 * moved only the machine digest (a write into RDRAM); 2: a refused call that
 * changed a public observation (a save import moved the write generation); 3:
 * a caller fault that poisons the instance before the deliberate defect step
 * (see seq_random_step). */
static void seq_inject_fault(seq *g, unsigned flags) {
  if (g->fault_done || g->fault == 0u || g->fault > 2u || g->step < 8u || !g->loaded ||
      (flags & SEQ_NO_MACHINE) != 0u) {
    return;
  }
  if (g->fault == 1u) {
    g->fault_done =
        t64_machine_bus_write(seq_m(g), UINT32_C(0x003ff000), 4u, UINT32_C(0xfeedface)) ==
        T64_STATUS_OK;
  } else {
    g->fault_done = t64_machine_save_import(seq_m(g), T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_RAW,
                                            g->save_pattern, sizeof g->save_pattern) ==
                    T64_STATUS_OK;
  }
}

static void seq_end(seq *g, t64_status status, const char *label, bool intact) {
  const t64c_class cls = t64c_status_class((uint32_t)status);
  const unsigned flags = g->cur_flags;

  g->calls++;
  g->status_counts[status < SEQ_STATUS_SLOTS ? status : SEQ_STATUS_SLOTS - 1u]++;
  g->cur_label = label;
  if (g->defect_call) {
    seq_check(g, status == T64_STATUS_INTERNAL_DEFECT && cls == T64C_CLASS_DEFECT,
              "the deliberate defect answers INTERNAL_DEFECT, a defect class status");
    g->class_counts[T64C_CLASS_DEFECT]++;
    g->poisoned = true;
    g->defect_call = false;
    return;
  }
  if (g->poisoned) {
    if ((flags & SEQ_QUERY) != 0u) {
      seq_check(g, status == T64_STATUS_OK, "the poison query works on a poisoned instance");
    } else if ((flags & SEQ_NO_MACHINE) != 0u) {
      seq_check(g, status == T64_STATUS_INVALID_ARGUMENT,
                "a NULL machine answers INVALID_ARGUMENT even when the instance is poisoned");
    } else {
      seq_check(g, status == T64_STATUS_POISONED, "a poisoned instance answers POISONED");
    }
    seq_check(g, intact, "a call on a poisoned instance leaves its outputs untouched");
    g->poisoned_calls++;
    return;
  }
  seq_check(g, cls == T64C_CLASS_OK || cls == T64C_CLASS_REFUSED,
            "a caller call answers an ok or refused class status (no defect, reserved or "
            "unclassified status)");
  g->class_counts[cls <= T64C_CLASS_UNKNOWN ? cls : T64C_CLASS_UNKNOWN]++;
  seq_log(g, label);
  if (cls == T64C_CLASS_REFUSED) {
    const char *difference;
    bool latch_allowed = false;

    seq_check(g, intact, "a refused call leaves its output untouched");
    seq_inject_fault(g, flags);
    seq_take(g, &g->after);
    difference = snap_first_difference(&g->before, &g->after);
    if (difference != NULL) {
      (void)fprintf(stderr, "FAIL sequence: a refused call changed the observation \"%s\"\n",
                    difference);
    }
    seq_check(g, difference == NULL, "a refused call leaves every observation bit-equal");
    if ((flags & SEQ_NO_MACHINE) != 0u) {
      return;
    }
    if ((flags & SEQ_BUS_WRITE) != 0u && status == T64_STATUS_BUS_DEVICE_REFUSED &&
        seq_pi_si_window(g->cur_address)) {
      latch_allowed = true;
    }
    if (latch_allowed) {
      /* the carve-out: the device kept its own latch, so the digest may move. */
      g->bus_latched++;
      g->burst_open = false;
      g->known_valid = false;
      g->pending_verify = false;
    } else if (g->burst_open) {
      if (g->known_valid) {
        g->pending_verify = true;
      } else {
        g->digests_skipped++;
      }
    }
    return;
  }
  if ((flags & SEQ_PURE) != 0u && cls == T64C_CLASS_OK) {
    seq_take(g, &g->after);
    if (snap_first_difference(&g->before, &g->after) != NULL) {
      (void)fprintf(stderr, "FAIL sequence: an observation changed \"%s\"\n",
                    snap_first_difference(&g->before, &g->after));
    }
    seq_check(g, snap_first_difference(&g->before, &g->after) == NULL,
              "an observation call changes no observation");
    return;
  }
  if ((flags & SEQ_NO_MACHINE) == 0u) {
    /* the instance may have changed: the burst ends here without a digest. */
    g->burst_open = false;
    g->known_valid = false;
    g->pending_verify = false;
  }
}

/* ---- mirrored guest facts ------------------------------------------------------ */

static void seq_after_admit(seq *g) {
  g->loaded = true;
  g->tick = 0u;
  g->instr_limit = 0u;
  g->frame_limit = 0u;
  g->terminal = false;
  g->stalled = false;
  g->await_progress = false;
}

static void seq_note_admit(seq *g, t64_status status) {
  if (status == T64_STATUS_OK) {
    seq_after_admit(g);
  }
}

/* The checks every successful advance goes through. */
static void seq_note_stop(seq *g, uint32_t kind, uint64_t value, const t64_stop_result *res) {
  const t64c_class cls = t64c_stop_class(res->reason);
  const t64c_resume resume = t64c_stop_resume(res->reason);
  const bool moved = res->tick > g->tick;
  const bool stall = res->reason == T64_STOP_REASON_RESOURCE_LIMIT && !moved;
  const bool expect_progress = kind == T64_STOP_GUEST_TICK && value > g->tick;

  g->stop_counts[res->reason < SEQ_STOP_SLOTS ? res->reason : SEQ_STOP_SLOTS - 1u]++;
  seq_check(g, cls == T64C_CLASS_GUEST_STOP,
            "every advance stop reason is a guest-stop row of the class table");
  if (g->class_counts[T64C_CLASS_OK] > 0u) {
    g->class_counts[T64C_CLASS_OK]--;
  }
  g->class_counts[T64C_CLASS_GUEST_STOP]++;
  seq_check(g, res->tick >= g->tick, "guest time never goes backwards");
  if (kind == T64_STOP_GUEST_TICK) {
    seq_check(g, res->tick <= value, "a stop does not pass the requested tick bound");
    if (res->reason == T64_STOP_REASON_BOUND) {
      seq_check(g, res->tick == value, "a BOUND stop is exactly at the bound");
    }
  }
  if (g->terminal) {
    seq_check(g, res->reason == g->terminal_reason && res->tick == g->terminal_tick,
              "a terminal stop stays the same terminal stop until a reset");
  } else if (g->await_progress) {
    seq_check(g, moved || resume == T64C_RESUME_TERMINAL,
              "the advance after a full drain makes progress or ends in a terminal stop");
    if (moved) {
      g->resumed++;
    }
  } else if (expect_progress && !g->stalled) {
    seq_check(g, moved || resume == T64C_RESUME_TERMINAL || stall,
              "a larger bound after a resumable stop makes progress, ends terminal, or stalls");
  }
  g->await_progress = false;
  if (stall) {
    t64_output_status ostat = T64_OUTPUT_STATUS_INIT;

    seq_check(g, t64_machine_output_status(seq_m(g), &ostat) == T64_STATUS_OK &&
                     (ostat.video_free == 0u || ostat.audio_free == 0u),
              "a RESOURCE_LIMIT stop that moved nothing has a full video or audio ring");
    g->stalled = true;
    g->stall_kind = kind;
    g->stall_bound = value;
    g->stall_tick = res->tick;
    g->stalls++;
  } else if (moved) {
    g->stalled = false;
  }
  if (resume == T64C_RESUME_TERMINAL && !g->terminal) {
    g->terminal = true;
    g->terminal_reason = res->reason;
    g->terminal_tick = res->tick;
    g->terminals++;
  }
  if (kind == T64_STOP_INSTRUCTION_COUNT && value > g->instr_limit) {
    g->instr_limit = value;
  }
  if (kind == T64_STOP_FRAME_EVENT_COUNT && value > g->frame_limit) {
    g->frame_limit = value;
  }
  g->tick = res->tick;
}

/* One classified advance with a sentinel result. */
static t64_status seq_do_advance(seq *g, unsigned flags, uint32_t kind, uint64_t value,
                                 const char *label, t64_stop_result *out) {
  t64_stop_request request = T64_STOP_REQUEST_INIT(kind, value);
  t64_stop_result result = T64_STOP_RESULT_INIT_WITH(SEQ_U32_SENTINEL, SEQ_U64_SENTINEL);
  t64_status status;

  seq_begin(g, flags);
  status = t64_machine_advance(seq_m(g), &request, &result);
  seq_end(g, status,
          label,
          status == T64_STATUS_OK ||
              (result.reason == SEQ_U32_SENTINEL && result.tick == SEQ_U64_SENTINEL));
  if (status == T64_STATUS_OK) {
    seq_note_stop(g, kind, value, &result);
  }
  if (out != NULL) {
    *out = result;
  }
  return status;
}

/* ---- draining -------------------------------------------------------------------- */

/* Drain both rings and the ISViewer text with raw calls (every status is an
 * ok or empty-ring answer here). False on an unexpected status. */
static bool seq_drain_all(seq *g) {
  uint8_t block[SEQ_BLOCK_BYTES];
  unsigned guard;

  seq_unbracketed_mutation(g);
  for (guard = 0u; guard < 64u; guard++) {
    t64_video_packet_info info = T64_VIDEO_PACKET_INFO_INIT;
    size_t copied = 0u;
    t64_status status = t64_video_info(seq_m(g), &info);

    if (status == T64_STATUS_NOT_READY) {
      break;
    }
    if (status != T64_STATUS_OK || info.packet_bytes > sizeof block ||
        t64_video_copy(seq_m(g), block, (size_t)info.packet_bytes, &copied) != T64_STATUS_OK) {
      return false;
    }
  }
  for (guard = 0u; guard < 64u; guard++) {
    t64_audio_packet_info info = T64_AUDIO_PACKET_INFO_INIT;
    size_t copied = 0u;
    t64_status status = t64_audio_info(seq_m(g), &info);

    if (status == T64_STATUS_NOT_READY) {
      break;
    }
    if (status != T64_STATUS_OK || info.byte_length > sizeof block ||
        t64_audio_copy(seq_m(g), block, (size_t)info.byte_length, &copied) != T64_STATUS_OK) {
      return false;
    }
  }
  for (guard = 0u; guard < 64u; guard++) {
    t64_output_drain_result result = T64_OUTPUT_DRAIN_RESULT_INIT;
    t64_status status = t64_machine_output_drain(seq_m(g), block, sizeof block, &result);

    if (status != T64_STATUS_OK) {
      return false;
    }
    if (result.copied_bytes == 0u) {
      break;
    }
  }
  return true;
}

/* One classified advance that must end with the given stop reason. */
static void seq_expect_stop(seq *g, uint32_t kind, uint64_t value, uint32_t reason,
                            const char *label) {
  t64_stop_result stop = T64_STOP_RESULT_INIT;
  t64_status status = seq_do_advance(g, SEQ_ANY, kind, value, label, &stop);

  seq_check(g, status == T64_STATUS_OK && stop.reason == reason, label);
}

/* After a stall: drain everything, then the same advance must move on. */
static void seq_resume_after_drain(seq *g) {
  bool drained;
  t64_status status;

  if (!g->stalled || g->poisoned) {
    return;
  }
  drained = seq_drain_all(g);
  seq_check(g, drained, "the full drain of a stalled host answers ok statuses only");
  g->await_progress = true;
  status = seq_do_advance(g, SEQ_ANY, g->stall_kind, g->stall_bound,
                          "advance after a full drain", NULL);
  seq_check(g, status == T64_STATUS_OK, "the advance after a full drain is accepted");
  g->await_progress = false;
}

/* ---- operations ------------------------------------------------------------------ */

static void op_admit(seq *g, unsigned v) {
  const uint8_t *rom = g->fx->rom;
  const size_t length = g->fx->rom_length;
  t64_status status;

  switch (v) {
    case 0u:
      seq_begin(g, SEQ_ANY);
      status = t64_machine_admit_rom(seq_m(g), rom, length);
      seq_end(g, status, "admit the composite ROM", true);
      seq_note_admit(g, status);
      break;
    case 1u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_admit_rom(seq_m(g), NULL, length);
      seq_end(g, status, "admit NULL bytes", true);
      seq_note_admit(g, status);
      break;
    case 2u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_admit_rom(seq_m(g), rom, 0u);
      seq_end(g, status, "admit zero length", true);
      seq_note_admit(g, status);
      break;
    case 3u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_admit_rom(seq_m(g), rom, length - 1u);
      seq_end(g, status, "admit one byte short", true);
      seq_note_admit(g, status);
      break;
    case 4u:
      g->fx->ledger.fail_in = 1u;
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_admit_rom(seq_m(g), rom, length);
      seq_end(g, status, "admit with a refusing allocator", true);
      g->fx->ledger.fail_in = 0u;
      seq_note_admit(g, status);
      break;
    default:
      seq_begin(g, SEQ_REFUSED | SEQ_NO_MACHINE);
      status = t64_machine_admit_rom(NULL, rom, length);
      seq_end(g, status, "admit on a NULL machine", true);
      break;
  }
}

static void op_advance(seq *g, unsigned v) {
  t64_stop_request request = T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK, g->tick + 1000u);
  t64_stop_result result = T64_STOP_RESULT_INIT_WITH(SEQ_U32_SENTINEL, SEQ_U64_SENTINEL);
  t64_stop_result last = T64_STOP_RESULT_INIT;
  t64_status status;
  unsigned i;
  unsigned runs;

  switch (v) {
    case 0u:
      (void)seq_do_advance(g, SEQ_ANY, T64_STOP_GUEST_TICK, g->tick + 1u + seq_pick(g, 4096u),
                           "advance by a small tick bound", NULL);
      break;
    case 1u:
      (void)seq_do_advance(g, SEQ_ANY, T64_STOP_GUEST_TICK,
                           g->tick + 4096u + seq_pick(g, 60000u),
                           "advance by a medium tick bound", NULL);
      break;
    case 2u:
      (void)seq_do_advance(g, SEQ_ANY, T64_STOP_GUEST_TICK,
                           g->tick + 100000u + seq_pick(g, 200000u),
                           "advance by a large tick bound", NULL);
      break;
    case 3u:
      /* a run: the same far bound until it stops for another reason than the
       * per-advance event budget (every call is classified). */
      runs = 2u + seq_pick(g, 40u);
      for (i = 0u; i < runs; i++) {
        status = seq_do_advance(g, SEQ_ANY, T64_STOP_GUEST_TICK, g->tick + 200000u,
                                "advance run", &last);
        if (status != T64_STATUS_OK || last.reason != T64_STOP_REASON_RESOURCE_LIMIT ||
            g->stalled || g->terminal || g->abort) {
          break;
        }
      }
      break;
    case 4u:
      if (g->tick > 0u) {
        (void)seq_do_advance(g, SEQ_REFUSED, T64_STOP_GUEST_TICK,
                             g->tick - 1u - (uint64_t)seq_pick(g, 8u) % g->tick,
                             "advance to a tick behind guest time", NULL);
      } else {
        (void)seq_do_advance(g, SEQ_ANY, T64_STOP_GUEST_TICK, 0u, "advance to tick zero", NULL);
      }
      break;
    case 5u:
      (void)seq_do_advance(g, SEQ_ANY, T64_STOP_GUEST_TICK, g->tick,
                           "advance to the current tick", NULL);
      break;
    case 6u:
      (void)seq_do_advance(g, SEQ_ANY, T64_STOP_INSTRUCTION_COUNT,
                           g->instr_limit + 100000u + seq_pick(g, 400000u),
                           "advance by an instruction count", NULL);
      break;
    case 7u:
      (void)seq_do_advance(g, SEQ_ANY, T64_STOP_FRAME_EVENT_COUNT,
                           g->frame_limit + 1u + seq_pick(g, 3u),
                           "advance by a frame-event count", NULL);
      break;
    case 8u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_advance(seq_m(g), &request, NULL);
      seq_end(g, status, "advance with a NULL result", true);
      break;
    case 9u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_advance(seq_m(g), NULL, &result);
      seq_end(g, status, "advance with a NULL request",
              seq_struct_intact(&result, sizeof result, (uint32_t)sizeof result) ||
                  (result.reason == SEQ_U32_SENTINEL && result.tick == SEQ_U64_SENTINEL));
      break;
    case 10u:
      request.struct_size = 4u;
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_advance(seq_m(g), &request, &result);
      seq_end(g, status, "advance with a request struct_size below v1",
              result.reason == SEQ_U32_SENTINEL && result.tick == SEQ_U64_SENTINEL);
      break;
    case 11u:
      result.struct_size = 4u;
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_advance(seq_m(g), &request, &result);
      seq_end(g, status, "advance with a result struct_size below v1",
              result.struct_size == 4u && result.reason == SEQ_U32_SENTINEL &&
                  result.tick == SEQ_U64_SENTINEL);
      break;
    case 12u:
      request.kind = (seq_pick(g, 2u) == 0u) ? 0u : 99u;
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_advance(seq_m(g), &request, &result);
      seq_end(g, status, "advance with an unknown stop kind",
              result.reason == SEQ_U32_SENTINEL && result.tick == SEQ_U64_SENTINEL);
      break;
    default:
      seq_begin(g, SEQ_REFUSED | SEQ_NO_MACHINE);
      status = t64_machine_advance(NULL, &request, &result);
      seq_end(g, status, "advance on a NULL machine",
              result.reason == SEQ_U32_SENTINEL && result.tick == SEQ_U64_SENTINEL);
      break;
  }
}

static void op_input(seq *g, unsigned v) {
  const uint16_t buttons = (uint16_t)(seq_next(g) & 0xffffu);
  const int32_t axis_x = (int32_t)seq_pick(g, 256u) - 128;
  const int32_t axis_y = (int32_t)seq_pick(g, 256u) - 128;
  const uint64_t future = g->tick + 1000u + seq_pick(g, 300000u);
  t64_status status;
  unsigned i;

  switch (v) {
    case 0u:
      seq_begin(g, SEQ_ANY);
      status = t64_machine_queue_input(seq_m(g), future, 0u, buttons, axis_x, axis_y);
      seq_end(g, status, "queue an input in the future", true);
      break;
    case 1u:
      seq_begin(g, g->tick > 0u ? SEQ_REFUSED : SEQ_ANY);
      status = t64_machine_queue_input(seq_m(g), g->tick > 0u ? g->tick - 1u : 0u, 0u, buttons,
                                       axis_x, axis_y);
      seq_end(g, status, "queue an input in the past", true);
      break;
    case 2u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_queue_input(seq_m(g), future, 1u + seq_pick(g, 7u), buttons, axis_x,
                                       axis_y);
      seq_end(g, status, "queue an input on a port that is not 0", true);
      break;
    case 3u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_queue_input(seq_m(g), future, 0u, buttons,
                                       128 + (int32_t)seq_pick(g, 100u), axis_y);
      seq_end(g, status, "queue an input with an axis above 127", true);
      break;
    case 4u:
      /* fill the event queue with raw calls, then one more call must be refused
       * and change nothing */
      seq_unbracketed_mutation(g);
      for (i = 0u; i < 6000u; i++) {
        if (t64_machine_queue_input(seq_m(g), future + i, 0u, buttons, axis_x, axis_y) !=
            T64_STATUS_OK) {
          break;
        }
      }
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_queue_input(seq_m(g), future, 0u, buttons, axis_x, axis_y);
      seq_end(g, status, "queue an input into a full event queue", true);
      break;
    case 5u:
      seq_begin(g, SEQ_REFUSED | SEQ_NO_MACHINE);
      status = t64_machine_queue_input(NULL, future, 0u, buttons, axis_x, axis_y);
      seq_end(g, status, "queue an input on a NULL machine", true);
      break;
    default:
      seq_begin(g, SEQ_ANY);
      status = t64_machine_queue_input(seq_m(g), g->tick, 0u, buttons, axis_x, axis_y);
      seq_end(g, status, "queue an input at the current tick", true);
      break;
  }
}

/* video (true) or audio (false) copy */
static void op_ring(seq *g, unsigned v, bool video) {
  uint8_t block[SEQ_BLOCK_BYTES];
  size_t length = SEQ_SIZE_SENTINEL;
  size_t need = 0u;
  t64_status status;
  const char *label_prefix = video ? "video copy" : "audio copy";

#define SEQ_RING_COPY(bytes, capacity, length_out)                                         \
  (video ? t64_video_copy(seq_m(g), (bytes), (capacity), (length_out))                   \
         : t64_audio_copy(seq_m(g), (bytes), (capacity), (length_out)))

  seq_fill(block, sizeof block);
  switch (v) {
    case 0u:
      seq_begin(g, SEQ_PURE);
      status = SEQ_RING_COPY(NULL, 0u, &length);
      seq_end(g, status, label_prefix, true);
      break;
    case 1u:
      seq_begin(g, SEQ_REFUSED);
      status = SEQ_RING_COPY(block, 1u, &length);
      seq_end(g, status, label_prefix,
              seq_is_fill(block, sizeof block) &&
                  (status == T64_STATUS_BUFFER_TOO_SMALL || length == SEQ_SIZE_SENTINEL));
      break;
    case 2u:
      (void)SEQ_RING_COPY(NULL, 0u, &need);
      if (need > 0u && need <= sizeof block) {
        seq_begin(g, SEQ_ANY);
        status = SEQ_RING_COPY(block, need, &length);
        seq_end(g, status, label_prefix, true);
      } else {
        length = SEQ_SIZE_SENTINEL;
        seq_begin(g, SEQ_PURE);
        status = SEQ_RING_COPY(NULL, 0u, &length);
        seq_end(g, status, label_prefix, true);
      }
      break;
    case 3u:
      seq_begin(g, SEQ_REFUSED);
      status = SEQ_RING_COPY(block, 64u, NULL);
      seq_end(g, status, label_prefix, seq_is_fill(block, sizeof block));
      break;
    case 4u:
      seq_begin(g, SEQ_REFUSED);
      status = SEQ_RING_COPY(NULL, 64u, &length);
      seq_end(g, status, label_prefix,
              status == T64_STATUS_BUFFER_TOO_SMALL || length == SEQ_SIZE_SENTINEL);
      break;
    default:
      seq_begin(g, SEQ_REFUSED | SEQ_NO_MACHINE);
      status = video ? t64_video_copy(NULL, block, 64u, &length)
                     : t64_audio_copy(NULL, block, 64u, &length);
      seq_end(g, status, label_prefix,
              seq_is_fill(block, sizeof block) && length == SEQ_SIZE_SENTINEL);
      break;
  }
#undef SEQ_RING_COPY
}

static void op_video(seq *g, unsigned v) { op_ring(g, v, true); }
static void op_audio(seq *g, unsigned v) { op_ring(g, v, false); }

static void op_drain(seq *g, unsigned v) {
  uint8_t block[512];
  t64_output_drain_result result = T64_OUTPUT_DRAIN_RESULT_INIT;
  t64_status status;
  bool bytes_intact;

  seq_fill(block, sizeof block);
  seq_fill_struct(&result, sizeof result, (uint32_t)sizeof result);
  switch (v) {
    case 0u:
      seq_begin(g, SEQ_ANY);
      status = t64_machine_output_drain(seq_m(g), block, 1u + (size_t)seq_pick(g, 16u), &result);
      seq_end(g, status, "output drain into a small buffer", true);
      break;
    case 1u:
      seq_begin(g, SEQ_ANY);
      status = t64_machine_output_drain(seq_m(g), block, sizeof block, &result);
      seq_end(g, status, "output drain into a large buffer", true);
      break;
    case 2u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_output_drain(seq_m(g), block, sizeof block, NULL);
      seq_end(g, status, "output drain with a NULL result", seq_is_fill(block, sizeof block));
      break;
    case 3u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_output_drain(seq_m(g), NULL, 16u, &result);
      bytes_intact = status == T64_STATUS_BUFFER_TOO_SMALL ||
                     seq_struct_intact(&result, sizeof result, (uint32_t)sizeof result);
      seq_end(g, status, "output drain into a NULL buffer", bytes_intact);
      break;
    case 4u:
      result.struct_size = 4u;
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_output_drain(seq_m(g), block, sizeof block, &result);
      seq_end(g, status, "output drain with a result struct_size below v1",
              seq_is_fill(block, sizeof block) && result.struct_size == 4u &&
                  seq_is_fill((const uint8_t *)&result + sizeof(uint32_t),
                              sizeof result - sizeof(uint32_t)));
      break;
    case 5u:
      seq_begin(g, SEQ_REFUSED | SEQ_NO_MACHINE);
      status = t64_machine_output_drain(NULL, block, sizeof block, &result);
      seq_end(g, status, "output drain on a NULL machine",
              seq_is_fill(block, sizeof block) &&
                  seq_struct_intact(&result, sizeof result, (uint32_t)sizeof result));
      break;
    default:
      seq_begin(g, SEQ_ANY);
      status = t64_machine_output_drain(seq_m(g), NULL, 0u, &result);
      seq_end(g, status, "output drain size query", true);
      break;
  }
}

/* the observers, each with a valid and an invalid argument */
static void op_observe(seq *g, unsigned v) {
  t64_machine *m = seq_m(g);
  uint8_t digest[T64_DIGEST_SIZE];
  uint8_t header[SEQ_HEADER_BYTES];
  size_t length = SEQ_SIZE_SENTINEL;
  t64_output_status ostat = T64_OUTPUT_STATUS_INIT;
  t64_video_packet_info video = T64_VIDEO_PACKET_INFO_INIT;
  t64_audio_packet_info audio = T64_AUDIO_PACKET_INFO_INIT;
  t64_save_info save = T64_SAVE_INFO_INIT;
  t64_cpu_stop_detail detail = T64_CPU_STOP_DETAIL_INIT;
  const t64_profile_descriptor *profile = NULL;
  static const uint32_t bad_kinds[4] = {0u, 99u, T64_SAVE_FLASHRAM, T64_SAVE_EEPROM_4K};
  t64_status status;

  seq_fill(digest, sizeof digest);
  seq_fill(header, sizeof header);
  seq_fill_struct(&ostat, sizeof ostat, (uint32_t)sizeof ostat);
  seq_fill_struct(&video, sizeof video, (uint32_t)sizeof video);
  seq_fill_struct(&audio, sizeof audio, (uint32_t)sizeof audio);
  seq_fill_struct(&save, sizeof save, (uint32_t)sizeof save);
  seq_fill_struct(&detail, sizeof detail, (uint32_t)sizeof detail);
  switch (v) {
    case 0u:
      seq_begin(g, SEQ_PURE);
      status = t64_machine_digest(m, digest);
      seq_end(g, status, "machine digest", true);
      break;
    case 1u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_digest(m, NULL);
      seq_end(g, status, "machine digest into NULL", true);
      break;
    case 2u:
      seq_begin(g, SEQ_REFUSED | SEQ_NO_MACHINE);
      status = t64_machine_digest(NULL, digest);
      seq_end(g, status, "machine digest of a NULL machine", seq_is_fill(digest, sizeof digest));
      break;
    case 3u:
      seq_begin(g, SEQ_PURE);
      status = t64_machine_content_digest(m, digest);
      seq_end(g, status, "content digest", true);
      break;
    case 4u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_content_digest(m, NULL);
      seq_end(g, status, "content digest into NULL", true);
      break;
    case 5u:
      seq_begin(g, SEQ_PURE);
      status = t64_machine_rom_size(m, &length);
      seq_end(g, status, "ROM size", true);
      break;
    case 6u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_rom_size(m, NULL);
      seq_end(g, status, "ROM size into NULL", true);
      break;
    case 7u:
      seq_begin(g, SEQ_PURE);
      status = t64_machine_rom_header(m, header, sizeof header, &length);
      seq_end(g, status, "ROM header", true);
      break;
    case 8u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_rom_header(m, header, 10u, &length);
      seq_end(g, status, "ROM header into a short buffer",
              seq_is_fill(header, sizeof header) &&
                  (status == T64_STATUS_BUFFER_TOO_SMALL || length == SEQ_SIZE_SENTINEL));
      break;
    case 9u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_rom_header(m, NULL, 0u, &length);
      seq_end(g, status, "ROM header size query",
              status == T64_STATUS_BUFFER_TOO_SMALL || length == SEQ_SIZE_SENTINEL);
      break;
    case 10u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_rom_header(m, header, sizeof header, NULL);
      seq_end(g, status, "ROM header without a length output", seq_is_fill(header, sizeof header));
      break;
    case 11u:
      seq_begin(g, SEQ_PURE);
      status = t64_machine_output_status(m, &ostat);
      seq_end(g, status, "output status", true);
      break;
    case 12u:
      ostat.struct_size = 4u;
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_output_status(m, &ostat);
      seq_end(g, status, "output status with a struct_size below v1",
              ostat.struct_size == 4u && seq_is_fill(&ostat.video_queued, sizeof ostat - 4u));
      break;
    case 13u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_output_status(m, NULL);
      seq_end(g, status, "output status into NULL", true);
      break;
    case 14u:
      seq_begin(g, SEQ_PURE);
      status = t64_video_info(m, &video);
      seq_end(g, status, "video info", true);
      break;
    case 15u:
      video.struct_size = 4u;
      seq_begin(g, SEQ_REFUSED);
      status = t64_video_info(m, &video);
      seq_end(g, status, "video info with a struct_size below v1", video.struct_size == 4u);
      break;
    case 16u:
      seq_begin(g, SEQ_PURE);
      status = t64_audio_info(m, &audio);
      seq_end(g, status, "audio info", true);
      break;
    case 17u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_audio_info(m, NULL);
      seq_end(g, status, "audio info into NULL", true);
      break;
    case 18u:
      seq_begin(g, SEQ_PURE);
      status = t64_machine_save_info(m, T64_SAVE_SRAM_32K, &save);
      seq_end(g, status, "save info", true);
      break;
    case 19u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_save_info(m, bad_kinds[seq_pick(g, 4u)], &save);
      seq_end(g, status, "save info of another device kind",
              seq_struct_intact(&save, sizeof save, (uint32_t)sizeof save));
      break;
    case 20u:
      seq_begin(g, SEQ_PURE);
      status = t64_machine_cpu_stop_detail(m, &detail);
      seq_end(g, status, "CPU stop detail", seq_struct_intact(&detail, sizeof detail,
                                                              (uint32_t)sizeof detail) ||
                                                   status == T64_STATUS_OK);
      break;
    case 21u:
      detail.struct_size = 4u;
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_cpu_stop_detail(m, &detail);
      seq_end(g, status, "CPU stop detail with a struct_size below v1", detail.struct_size == 4u);
      break;
    case 22u:
      seq_begin(g, SEQ_PURE);
      status = t64_machine_profile(m, &profile);
      seq_end(g, status, "the instance's profile", true);
      break;
    default:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_profile(m, NULL);
      seq_end(g, status, "the instance's profile into NULL", true);
      break;
  }
}

static void op_save(seq *g, unsigned v) {
  uint8_t small[100];
  size_t length = SEQ_SIZE_SENTINEL;
  t64_status status;
  static const uint32_t unknown_kinds[2] = {0u, 99u};
  t64_machine *m = seq_m(g);

  seq_fill(g->save_buf, sizeof g->save_buf);
  seq_fill(small, sizeof small);
  switch (v) {
    case 0u:
      seq_begin(g, SEQ_PURE);
      status = t64_machine_save_export(m, T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_RAW, g->save_buf,
                                       sizeof g->save_buf, &length);
      seq_end(g, status, "save export, raw", true);
      break;
    case 1u:
      seq_begin(g, SEQ_PURE);
      status = t64_machine_save_export(m, T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_SWAP16, g->save_buf,
                                       sizeof g->save_buf, &length);
      seq_end(g, status, "save export, swap16", true);
      break;
    case 2u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_save_export(m, unknown_kinds[seq_pick(g, 2u)], T64_SAVE_FORMAT_RAW,
                                       g->save_buf, sizeof g->save_buf, &length);
      seq_end(g, status, "save export of an unknown device kind",
              seq_is_fill(g->save_buf, sizeof g->save_buf) && length == SEQ_SIZE_SENTINEL);
      break;
    case 3u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_save_export(m, T64_SAVE_FLASHRAM + seq_pick(g, 4u),
                                       T64_SAVE_FORMAT_RAW, g->save_buf, sizeof g->save_buf,
                                       &length);
      seq_end(g, status, "save export of an unsupported device kind",
              seq_is_fill(g->save_buf, sizeof g->save_buf) && length == SEQ_SIZE_SENTINEL);
      break;
    case 4u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_save_export(m, T64_SAVE_EEPROM_4K + seq_pick(g, 2u),
                                       T64_SAVE_FORMAT_RAW, g->save_buf, sizeof g->save_buf,
                                       &length);
      seq_end(g, status, "save export of a device the profile does not have",
              seq_is_fill(g->save_buf, sizeof g->save_buf) && length == SEQ_SIZE_SENTINEL);
      break;
    case 5u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_save_export(m, T64_SAVE_SRAM_32K, seq_pick(g, 2u) == 0u ? 0u : 4u,
                                       g->save_buf, sizeof g->save_buf, &length);
      seq_end(g, status, "save export in an unknown byte order",
              seq_is_fill(g->save_buf, sizeof g->save_buf) && length == SEQ_SIZE_SENTINEL);
      break;
    case 6u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_save_export(m, T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_RAW, small,
                                       sizeof small, &length);
      seq_end(g, status, "save export into a short buffer",
              seq_is_fill(small, sizeof small) &&
                  (status == T64_STATUS_BUFFER_TOO_SMALL || length == SEQ_SIZE_SENTINEL));
      break;
    case 7u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_save_export(m, T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_RAW, g->save_buf,
                                       sizeof g->save_buf, NULL);
      seq_end(g, status, "save export without a length output",
              seq_is_fill(g->save_buf, sizeof g->save_buf));
      break;
    case 8u:
      seq_begin(g, SEQ_ANY);
      status = t64_machine_save_import(m, T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_RAW, g->save_pattern,
                                       sizeof g->save_pattern);
      seq_end(g, status, "save import of a full image", true);
      break;
    case 9u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_save_import(m, T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_RAW, g->save_pattern,
                                       sizeof small);
      seq_end(g, status, "save import of the wrong length", true);
      break;
    case 10u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_save_import(m, T64_SAVE_SRAM_32K, 0u, g->save_pattern,
                                       sizeof g->save_pattern);
      seq_end(g, status, "save import in an unknown byte order", true);
      break;
    case 11u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_save_import(m, unknown_kinds[seq_pick(g, 2u)], T64_SAVE_FORMAT_RAW,
                                       g->save_pattern, sizeof g->save_pattern);
      seq_end(g, status, "save import of an unknown device kind", true);
      break;
    case 12u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_save_import(m, T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_RAW, NULL,
                                       sizeof g->save_pattern);
      seq_end(g, status, "save import of NULL bytes", true);
      break;
    default:
      seq_begin(g, SEQ_REFUSED | SEQ_NO_MACHINE);
      status = t64_machine_save_import(NULL, T64_SAVE_SRAM_32K, T64_SAVE_FORMAT_RAW,
                                       g->save_pattern, sizeof g->save_pattern);
      seq_end(g, status, "save import on a NULL machine", true);
      break;
  }
}

/* state import and export; the blob is the container of the first admission */
static void op_state(seq *g, unsigned v) {
  t64_machine *m = seq_m(g);
  t64_state_import_report report = T64_STATE_IMPORT_REPORT_INIT;
  uint8_t small[64];
  size_t length = SEQ_SIZE_SENTINEL;
  t64_status status;
  uint8_t saved;
  size_t at;

  seq_fill(small, sizeof small);
  seq_fill_struct(&report, sizeof report, (uint32_t)sizeof report);
  switch (v) {
    case 0u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_state_export(m, NULL, 0u, &length);
      seq_end(g, status, "state export size query",
              status == T64_STATUS_BUFFER_TOO_SMALL || length == SEQ_SIZE_SENTINEL);
      break;
    case 1u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_state_export(m, small, sizeof small, &length);
      seq_end(g, status, "state export into a short buffer",
              seq_is_fill(small, sizeof small) &&
                  (status == T64_STATUS_BUFFER_TOO_SMALL || length == SEQ_SIZE_SENTINEL));
      break;
    case 2u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_state_export(m, small, sizeof small, NULL);
      seq_end(g, status, "state export without a length output", seq_is_fill(small, sizeof small));
      break;
    case 3u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_state_import(m, g->blob, g->blob_length - 1u, 0u, &report);
      seq_end(g, status, "state import of a truncated container", true);
      break;
    case 4u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_state_import(m, g->blob, 0u, 0u, &report);
      seq_end(g, status, "state import of zero bytes", true);
      break;
    case 5u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_state_import(m, NULL, g->blob_length, 0u, &report);
      seq_end(g, status, "state import of NULL bytes", true);
      break;
    case 6u:
      saved = g->blob[0];
      g->blob[0] = (uint8_t)(saved ^ 0xffu);
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_state_import(m, g->blob, g->blob_length, 0u, &report);
      seq_end(g, status, "state import with a damaged magic", true);
      g->blob[0] = saved;
      break;
    case 7u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_state_import(m, g->blob, g->blob_length, UINT32_C(0x80000000), &report);
      seq_end(g, status, "state import with an unknown flag", true);
      break;
    case 8u:
      report.struct_size = 4u;
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_state_import(m, g->blob, g->blob_length, 0u, &report);
      seq_end(g, status, "state import with a report struct_size below v1",
              report.struct_size == 4u &&
                  seq_is_fill(&report.status, sizeof report - sizeof(uint32_t)));
      break;
    case 9u:
      seq_begin(g, SEQ_REFUSED | SEQ_NO_MACHINE);
      status = t64_machine_state_import(NULL, g->blob, g->blob_length, 0u, &report);
      seq_end(g, status, "state import on a NULL machine",
              seq_struct_intact(&report, sizeof report, (uint32_t)sizeof report));
      break;
    case 10u:
      at = g->blob_length / 2u;
      saved = g->blob[at];
      g->blob[at] = (uint8_t)(saved ^ 0x01u);
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_state_import(m, g->blob, g->blob_length, 0u, &report);
      seq_end(g, status, "state import with a flipped byte in the body", true);
      g->blob[at] = saved;
      break;
    case 11u:
      seq_begin(g, SEQ_ANY);
      status = t64_machine_state_import(m, g->blob, g->blob_length, 0u, &report);
      seq_end(g, status, "state import of the first container", true);
      seq_note_admit(g, status);
      break;
    default:
      seq_begin(g, SEQ_ANY);
      status = t64_machine_state_import(m, g->blob, g->blob_length,
                                        T64_STATE_IMPORT_DROP_OBSERVERS, &report);
      seq_end(g, status, "state import of the first container, observers dropped", true);
      seq_note_admit(g, status);
      break;
  }
}

static void op_reset(seq *g, unsigned v) {
  t64_status status;

  if (v == 0u) {
    seq_begin(g, SEQ_ANY);
    status = t64_machine_reset(seq_m(g));
    seq_end(g, status, "reset", true);
    seq_note_admit(g, status);
  } else {
    seq_begin(g, SEQ_REFUSED | SEQ_NO_MACHINE);
    status = t64_machine_reset(NULL);
    seq_end(g, status, "reset of a NULL machine", true);
  }
}

static void op_unload(seq *g, unsigned v) {
  t64_status status;

  if (v == 0u) {
    seq_begin(g, SEQ_ANY);
    status = t64_machine_unload(seq_m(g));
    seq_end(g, status, "unload", true);
    if (status == T64_STATUS_OK) {
      g->loaded = false;
      g->tick = 0u;
      g->instr_limit = 0u;
      g->frame_limit = 0u;
      g->terminal = false;
      g->stalled = false;
    }
  } else {
    seq_begin(g, SEQ_REFUSED | SEQ_NO_MACHINE);
    status = t64_machine_unload(NULL);
    seq_end(g, status, "unload of a NULL machine", true);
  }
}

/* debug bus access: RDRAM scratch at the top of RDRAM, windows that refuse */
static void op_bus(seq *g, unsigned v) {
  t64_machine *m = seq_m(g);
  const uint32_t scratch = UINT32_C(0x003ff000) + 4u * seq_pick(g, 256u);
  uint32_t value = SEQ_U32_SENTINEL;
  t64_status status;

  switch (v) {
    case 0u:
      seq_begin(g, SEQ_PURE);
      status = t64_machine_bus_read(m, scratch, 4u, &value);
      seq_end(g, status, "bus read of RDRAM", true);
      break;
    case 1u:
      seq_begin(g, SEQ_PURE);
      status = t64_machine_bus_read(m, UINT32_C(0x10000000), 4u, &value);
      seq_end(g, status, "bus read of the cartridge window", true);
      break;
    case 2u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_bus_read(m, UINT32_C(0x80000000), 4u, &value);
      seq_end(g, status, "bus read of an unmapped address", value == SEQ_U32_SENTINEL);
      break;
    case 3u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_bus_read(m, scratch + 1u, 4u, &value);
      seq_end(g, status, "bus read of a misaligned word", value == SEQ_U32_SENTINEL);
      break;
    case 4u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_bus_read(m, scratch, 3u, &value);
      seq_end(g, status, "bus read of width 3", value == SEQ_U32_SENTINEL);
      break;
    case 5u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_bus_read(m, scratch, 4u, NULL);
      seq_end(g, status, "bus read into NULL", true);
      break;
    case 6u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_bus_read(m, UINT32_C(0xfffffffe), 4u, &value);
      seq_end(g, status, "bus read leaving the address space", value == SEQ_U32_SENTINEL);
      break;
    case 7u:
      seq_begin(g, SEQ_REFUSED | SEQ_NO_MACHINE);
      status = t64_machine_bus_read(NULL, scratch, 4u, &value);
      seq_end(g, status, "bus read on a NULL machine", value == SEQ_U32_SENTINEL);
      break;
    case 8u:
      seq_begin(g, SEQ_ANY | SEQ_BUS_WRITE);
      g->cur_address = scratch;
      status = t64_machine_bus_write(m, scratch, 4u, (uint32_t)seq_next(g));
      seq_end(g, status, "bus write of RDRAM", true);
      break;
    case 9u:
      seq_begin(g, SEQ_REFUSED | SEQ_BUS_WRITE);
      g->cur_address = UINT32_C(0x80000000);
      status = t64_machine_bus_write(m, UINT32_C(0x80000000), 4u, 1u);
      seq_end(g, status, "bus write of an unmapped address", true);
      break;
    case 10u:
      seq_begin(g, SEQ_REFUSED | SEQ_BUS_WRITE);
      g->cur_address = scratch + 1u;
      status = t64_machine_bus_write(m, scratch + 1u, 2u, 1u);
      seq_end(g, status, "bus write of a misaligned halfword", true);
      break;
    case 11u:
      seq_begin(g, SEQ_REFUSED | SEQ_BUS_WRITE);
      g->cur_address = scratch;
      status = t64_machine_bus_write(m, scratch, 8u, 1u);
      seq_end(g, status, "bus write of width 8", true);
      break;
    case 12u:
      seq_begin(g, SEQ_REFUSED | SEQ_BUS_WRITE);
      g->cur_address = UINT32_C(0x10000000);
      status = t64_machine_bus_write(m, UINT32_C(0x10000000), 4u, UINT32_C(0xdeadbeef));
      seq_end(g, status, "bus write into the read-only cartridge window", true);
      break;
    case 13u:
      seq_begin(g, SEQ_ANY | SEQ_BUS_WRITE);
      g->cur_address = UINT32_C(0x04600000);
      status = t64_machine_bus_write(m, UINT32_C(0x04600000), 4u, UINT32_C(0x1000));
      seq_end(g, status, "PI DRAM address write", true);
      break;
    case 14u:
      seq_begin(g, SEQ_ANY | SEQ_BUS_WRITE);
      g->cur_address = UINT32_C(0x04600004);
      status = t64_machine_bus_write(m, UINT32_C(0x04600004), 4u, UINT32_C(0x10100000));
      seq_end(g, status, "PI cartridge address write", true);
      break;
    case 15u:
      /* a PI request the device rejects: the two address registers are set with
       * raw writes, then the length write is refused and the device latches its
       * own record (the carve-out) */
      seq_unbracketed_mutation(g);
      (void)t64_machine_bus_write(m, UINT32_C(0x04600000), 4u, UINT32_C(0x1000));
      (void)t64_machine_bus_write(m, UINT32_C(0x04600004), 4u, UINT32_C(0x10100000));
      seq_begin(g, SEQ_ANY | SEQ_BUS_WRITE);
      g->cur_address = UINT32_C(0x0460000c);
      status = t64_machine_bus_write(m, UINT32_C(0x0460000c), 4u, UINT32_C(0x3f));
      seq_end(g, status, "PI length write the device refuses", true);
      break;
    default:
      seq_begin(g, SEQ_REFUSED | SEQ_NO_MACHINE);
      status = t64_machine_bus_write(NULL, scratch, 4u, 1u);
      seq_end(g, status, "bus write on a NULL machine", true);
      break;
  }
}

static void op_trace(seq *g, unsigned v) {
  t64_machine *m = seq_m(g);
  t64_trace_record records[8];
  size_t count = SEQ_SIZE_SENTINEL;
  uint64_t dropped = SEQ_U64_SENTINEL;
  bool saturated = true;
  t64_status status;

  seq_fill(records, sizeof records);
  switch (v) {
    case 0u:
      seq_begin(g, SEQ_PURE);
      status = t64_machine_trace_set_enabled(m, true);
      seq_end(g, status, "trace on", true);
      break;
    case 1u:
      seq_begin(g, SEQ_PURE);
      status = t64_machine_trace_set_enabled(m, false);
      seq_end(g, status, "trace off", true);
      break;
    case 2u:
      seq_begin(g, SEQ_PURE);
      status = t64_machine_trace_drain(m, records, (uint32_t)sizeof records[0], 8u, &count,
                                       &dropped, &saturated);
      seq_end(g, status, "trace drain", true);
      break;
    case 3u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_trace_drain(m, records, 4u, 8u, &count, &dropped, &saturated);
      seq_end(g, status, "trace drain with a record size below v1",
              seq_is_fill(records, sizeof records) && count == SEQ_SIZE_SENTINEL);
      break;
    case 4u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_trace_drain(m, records, (uint32_t)sizeof records[0], 8u, NULL, &dropped,
                                       &saturated);
      seq_end(g, status, "trace drain without a count output", seq_is_fill(records, sizeof records));
      break;
    case 5u:
      seq_begin(g, SEQ_REFUSED);
      status = t64_machine_trace_drain(m, NULL, (uint32_t)sizeof records[0], 8u, &count, &dropped,
                                       &saturated);
      seq_end(g, status, "trace drain into NULL records", count == SEQ_SIZE_SENTINEL);
      break;
    default:
      seq_begin(g, SEQ_REFUSED | SEQ_NO_MACHINE);
      status = t64_machine_trace_set_enabled(NULL, true);
      seq_end(g, status, "trace on for a NULL machine", true);
      break;
  }
}

static void op_poison_query(seq *g, unsigned v) {
  uint32_t slot = SEQ_U32_SENTINEL;
  t64_status status;

  switch (v) {
    case 0u:
      seq_begin(g, SEQ_PURE | SEQ_QUERY);
      status = t64_machine_is_poisoned(seq_m(g), &slot);
      seq_end(g, status, "the poison query", true);
      seq_check(g, status == T64_STATUS_OK && slot == (g->poisoned ? 1u : 0u),
                "the poison query reports 1 exactly when the instance is poisoned");
      break;
    case 1u:
      seq_begin(g, SEQ_REFUSED | SEQ_NO_MACHINE);
      status = t64_machine_is_poisoned(NULL, &slot);
      seq_end(g, status, "the poison query of a NULL machine", slot == SEQ_U32_SENTINEL);
      break;
    default:
      seq_begin(g, SEQ_REFUSED | SEQ_NO_MACHINE);
      status = t64_machine_is_poisoned(seq_m(g), NULL);
      seq_end(g, status, "the poison query into NULL", true);
      break;
  }
}

/* creates that must be refused (descriptor mutations, S-07, and handshake faults);
 * a create that is accepted is destroyed again, the instance is not the one under test */
static void op_create(seq *g, unsigned v) {
  t64_profile_descriptor desc = *g->fx->profile;
  t64_machine_create_info info = T64_MACHINE_CREATE_INFO_INIT(&g->fx->allocator, &desc);
  t64_machine *slot = (t64_machine *)(void *)&g->tally;
  t64_machine **slot_pointer = &slot;
  t64_status status;
  const char *label = "create with a mutated descriptor";

  switch (v) {
    case 0u: desc.event_queue_capacity = 0u; label = "create with an event queue capacity of 0"; break;
    case 1u: desc.sequence_width_bits = 0u; label = "create with a sequence width of 0"; break;
    case 2u: desc.rdram_bytes = 0u; label = "create with no RDRAM"; break;
    case 3u: desc.event_queue_capacity = 1u + seq_pick(g, 3u); label = "create with a tiny event queue"; break;
    case 4u: desc.sequence_width_bits = 1u + seq_pick(g, 64u); label = "create with another sequence width"; break;
    case 5u: desc.struct_size = 4u; label = "create with a descriptor struct_size below v1"; break;
    case 6u: info.struct_size = 4u; label = "create with a create-info struct_size below v1"; break;
    case 7u: info.abi_version = T64_ABI_VERSION + 1u + seq_pick(g, 3u); label = "create with another ABI minor"; break;
    case 8u: info.allocator = NULL; label = "create without an allocator"; break;
    case 9u: desc.advance_event_budget = 0u; label = "create with no advance event budget"; break;
    case 10u: desc.cpu_rsp_ratio_denominator = 0u; label = "create with a CPU to RSP ratio denominator of 0"; break;
    case 11u: desc.rom_min_bytes = desc.rom_max_bytes + 1u; label = "create with a ROM minimum above the maximum"; break;
    case 12u: desc.profile_id[0] = 'x'; label = "create with a changed profile id"; break;
    default: slot_pointer = NULL; label = "create into a NULL machine slot"; break;
  }
  seq_begin(g, SEQ_REFUSED | SEQ_NO_MACHINE);
  status = t64_machine_create(&info, slot_pointer);
  seq_end(g, status, label, slot_pointer == NULL || slot == NULL);
  if (status == T64_STATUS_OK && slot != NULL && slot != (t64_machine *)(void *)&g->tally) {
    t64_machine_destroy(slot);
  }
}

static void op_drain_all(seq *g, unsigned v) {
  (void)v;
  seq_check(g, seq_drain_all(g), "the full drain answers ok statuses only");
  seq_resume_after_drain(g);
}

/* ---- the operation table --------------------------------------------------------- */

#define SEQ_BIT(n) (UINT32_C(1) << (n))

static const seq_op seq_ops[SEQ_OP_COUNT] = {
    {"admit", 3u, op_admit, 6u, SEQ_BIT(0), true},
    {"advance", 14u, op_advance, 14u,
     SEQ_BIT(0) | SEQ_BIT(1) | SEQ_BIT(2) | SEQ_BIT(3) | SEQ_BIT(5) | SEQ_BIT(6) | SEQ_BIT(7), true},
    {"input", 5u, op_input, 7u, SEQ_BIT(0) | SEQ_BIT(4) | SEQ_BIT(6), true},
    {"video", 4u, op_video, 6u, SEQ_BIT(2), true},
    {"audio", 4u, op_audio, 6u, SEQ_BIT(2), true},
    {"drain", 4u, op_drain, 7u, SEQ_BIT(0) | SEQ_BIT(1) | SEQ_BIT(6), true},
    {"observe", 14u, op_observe, 24u, 0u, true},
    {"save", 5u, op_save, 14u, SEQ_BIT(8), true},
    {"state", 3u, op_state, 13u, SEQ_BIT(11) | SEQ_BIT(12), true},
    {"reset", 2u, op_reset, 2u, SEQ_BIT(0), true},
    {"unload", 1u, op_unload, 2u, SEQ_BIT(0), true},
    {"bus", 6u, op_bus, 17u, SEQ_BIT(8) | SEQ_BIT(13) | SEQ_BIT(14) | SEQ_BIT(15), true},
    {"trace", 2u, op_trace, 7u, 0u, true},
    {"poison", 1u, op_poison_query, 3u, 0u, true},
    {"create", 3u, op_create, 14u, 0u, false},
    {"drain-all", 3u, op_drain_all, 1u, SEQ_BIT(0), false},
};

/* The state op is drawn from a table in which the two imports (the heavy
 * variants: 50 to 100 ms) are rare. */
static const unsigned seq_state_table[32] = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 0u,
                                             1u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 2u, 3u, 4u,
                                             5u, 6u, 7u, 8u, 9u, 0u, 3u, 10u, 11u, 12u};

static bool seq_find_profile_digest(seq *g);

static void seq_random_step(seq *g) {
  unsigned total = 0u;
  unsigned tries;
  unsigned op = 0u;
  unsigned variant = 0u;
  unsigned i;

  if (g->fault == 3u && g->step == 10u && !g->fault_done && seq_find_profile_digest(g)) {
    /* the self-test fault: an unclassified admission after a bit flip poisons the
     * instance at a step the sequence did not plan */
    uint8_t *where = (uint8_t *)g->fx->ledger.slots[g->a5_slot].data + g->a5_offset;

    where[0] = (uint8_t)(where[0] ^ 0x01u);
    (void)t64_machine_admit_rom(seq_m(g), g->fx->rom, g->fx->rom_length);
    g->fault_done = true;
  }
  for (i = 0u; i < SEQ_OP_COUNT; i++) {
    total += seq_ops[i].weight;
  }
  for (tries = 0u; tries < 8u; tries++) {
    unsigned draw = seq_pick(g, total);

    for (op = 0u; op + 1u < SEQ_OP_COUNT && draw >= seq_ops[op].weight; op++) {
      draw -= seq_ops[op].weight;
    }
    variant = seq_pick(g, seq_ops[op].variants);
    if (seq_ops[op].fn == op_state) {
      variant = seq_state_table[seq_pick(g, 32u)];
    }
    if (g->poisoned && !seq_ops[op].poison_ok) {
      continue;
    }
    /* state changes (and so machine digests) stay rare: a variant that may
     * change the instance is kept one time in six */
    if (!g->poisoned && (seq_ops[op].mutating & SEQ_BIT(variant)) != 0u && seq_pick(g, 6u) != 0u) {
      continue;
    }
    break;
  }
  if (g->poisoned && !seq_ops[op].poison_ok) {
    op = 6u;
    variant = 0u;
  }
  if (!g->loaded && !g->poisoned && seq_pick(g, 4u) == 0u) {
    op = 0u;
    variant = 0u;
  } else if (g->terminal && !g->poisoned && seq_pick(g, 8u) == 0u) {
    op = 9u;
    variant = 0u;
  }
  g->cur_op = seq_ops[op].name;
  g->cur_variant = variant;
  g->hits[op][variant]++;
  seq_ops[op].fn(g, variant);
}

/* ---- the two fixed points: milestone and defect ---------------------------------- */

/* Replace the image, queue the two literal inputs, and run the guest to its end
 * without draining until the output ring stalls it; drain; resume; the final
 * digest is the runner-recorded pin. */
static void seq_milestone(seq *g) {
  char hex[2u * T64_DIGEST_SIZE + 1u];
  uint8_t digest[T64_DIGEST_SIZE];
  t64_stop_result stop = T64_STOP_RESULT_INIT;
  t64_status status;
  unsigned guard;
  unsigned stalls_before = g->stalls;
  unsigned resumed_before = g->resumed;

  g->cur_op = "milestone";
  g->cur_variant = 0u;
  seq_begin(g, SEQ_ANY);
  status = t64_machine_admit_rom(seq_m(g), g->fx->rom, g->fx->rom_length);
  seq_end(g, status, "milestone: admit the composite ROM", true);
  seq_check(g, status == T64_STATUS_OK, "the milestone admission is accepted");
  seq_note_admit(g, status);
  seq_unbracketed_mutation(g);
  seq_check(g, t64c_queue_literal_inputs(g->fx), "the milestone queues the two literal inputs");
  /* the other two stop reasons, from the fresh machine: an instruction-count limit
   * stops with INSTRUCTION_COUNT and a larger one resumes; a frame-event limit of 0
   * stops at once with FRAME_EVENT_COUNT at the tick bound just reached */
  seq_expect_stop(g, T64_STOP_INSTRUCTION_COUNT, 1000u, T64_STOP_REASON_INSTRUCTION_COUNT,
                  "milestone: stop at an instruction count");
  guard = (unsigned)g->tick;
  seq_expect_stop(g, T64_STOP_INSTRUCTION_COUNT, 3000u, T64_STOP_REASON_INSTRUCTION_COUNT,
                  "milestone: resume to a larger instruction count");
  seq_check(g, g->tick > (uint64_t)guard, "a larger instruction count limit makes progress");
  for (guard = 0u; guard < 64u && !g->abort; guard++) {
    status = seq_do_advance(g, SEQ_ANY, T64_STOP_GUEST_TICK, 5000u,
                            "milestone: advance to tick 5000", &stop);
    if (status != T64_STATUS_OK || stop.reason == T64_STOP_REASON_BOUND) {
      break;
    }
  }
  seq_expect_stop(g, T64_STOP_FRAME_EVENT_COUNT, 0u, T64_STOP_REASON_FRAME_EVENT_COUNT,
                  "milestone: stop at a frame-event count");
  for (guard = 0u; guard < 2048u && !g->abort; guard++) {
    status = seq_do_advance(g, SEQ_ANY, T64_STOP_GUEST_TICK, T64C_RUN_BOUND,
                            "milestone: advance to the run bound", &stop);
    if (status != T64_STATUS_OK || g->terminal) {
      break;
    }
    if (g->stalled) {
      seq_resume_after_drain(g);
    }
  }
  seq_check(g, status == T64_STATUS_OK && g->terminal && g->terminal_reason == T64C_PIN_STOP_REASON,
            "the milestone run ends in GUEST_COMPLETION");
  seq_check(g, g->tick == T64C_PIN_COMPLETION_TICK, "the milestone run completes at the pinned tick");
  seq_check(g, g->stalls > stalls_before && g->resumed > resumed_before,
            "the milestone run met a backpressure stall and resumed after a drain");
  seq_begin(g, SEQ_PURE);
  status = t64_machine_digest(seq_m(g), digest);
  seq_end(g, status, "milestone: final digest", true);
  seq_hex(digest, hex);
  seq_check(g, status == T64_STATUS_OK && strcmp(hex, T64C_PIN_FINAL_DIGEST_HEX) == 0,
            "the milestone final digest is the runner-recorded pin");
  {
    t64_cpu_stop_detail detail = T64_CPU_STOP_DETAIL_INIT;

    seq_begin(g, SEQ_PURE);
    status = t64_machine_cpu_stop_detail(seq_m(g), &detail);
    seq_end(g, status, "milestone: CPU stop detail after completion", true);
    seq_check(g, status == T64_STATUS_OK, "the CPU stop detail is available after a terminal stop");
  }
  /* terminal: a farther bound reports the same stop at the same tick */
  (void)seq_do_advance(g, SEQ_ANY, T64_STOP_GUEST_TICK, T64C_RUN_BOUND + 1000u,
                       "milestone: advance past a terminal stop", NULL);
  g->milestones++;
  (void)printf("sequence: milestone at step %u: completed at tick %llu, digest %s\n", g->step,
               (unsigned long long)g->tick, hex);
}

static bool seq_find_profile_digest(seq *g) {
  uint8_t digest[T64_DIGEST_SIZE];
  size_t slot;
  size_t i;

  g->a5_matches = 0u;
  if (t64_profile_hash(g->fx->profile, digest) != T64_STATUS_OK) {
    return false;
  }
  for (slot = 0u; slot < g->fx->ledger.slot_count; slot++) {
    const uint8_t *bytes = (const uint8_t *)g->fx->ledger.slots[slot].data;
    const size_t size = g->fx->ledger.slots[slot].size;

    if (!g->fx->ledger.slots[slot].active || size < sizeof digest) {
      continue;
    }
    for (i = 0u; i + sizeof digest <= size; i++) {
      if (bytes[i] == digest[0] && memcmp(bytes + i, digest, sizeof digest) == 0) {
        g->a5_matches++;
        g->a5_slot = (unsigned)slot;
        g->a5_offset = i;
      }
    }
  }
  return g->a5_matches == 1u;
}

/* The deliberate defect: a bit of the profile digest the instance keeps, flipped
 * in the consumer's own allocator storage, then an admission. */
static void seq_defect(seq *g) {
  t64_status status;
  uint32_t slot = SEQ_U32_SENTINEL;
  uint8_t *where;

  g->cur_op = "defect";
  g->cur_variant = 0u;
  seq_close_burst(g);
  seq_check(g, seq_find_profile_digest(g),
            "the profile digest of the instance is found exactly once in its allocations");
  if (g->abort) {
    return;
  }
  where = (uint8_t *)g->fx->ledger.slots[g->a5_slot].data + g->a5_offset;
  where[0] = (uint8_t)(where[0] ^ 0x01u);
  g->defect_call = true;
  seq_begin(g, SEQ_ANY);
  status = t64_machine_admit_rom(seq_m(g), g->fx->rom, g->fx->rom_length);
  seq_end(g, status, "defect: admit with a damaged stored profile digest", true);
  seq_check(g, g->poisoned, "the defect poisons the instance");
  status = t64_machine_is_poisoned(seq_m(g), &slot);
  seq_check(g, status == T64_STATUS_OK && slot == 1u, "the poison query reports 1 after the defect");
  (void)printf("sequence: defect at step %u: the profile digest was found once (slot %u offset %zu), "
               "admit answered INTERNAL_DEFECT, the instance is poisoned\n",
               g->step, g->a5_slot, g->a5_offset);
}

/* Once poisoned: every variant of every machine-taking entry must answer POISONED. */
static void seq_poison_sweep(seq *g) {
  unsigned op;
  unsigned variant;
  unsigned before = g->poisoned_calls;

  for (op = 0u; op < SEQ_OP_COUNT && !g->abort; op++) {
    if (!seq_ops[op].poison_ok) {
      continue;
    }
    for (variant = 0u; variant < seq_ops[op].variants && !g->abort; variant++) {
      g->cur_op = seq_ops[op].name;
      g->cur_variant = variant;
      seq_ops[op].fn(g, variant);
    }
  }
  (void)printf("sequence: poison sweep: %u calls answered POISONED or the poison query, none "
               "changed an output\n",
               g->poisoned_calls - before);
}

/* ---- the case ------------------------------------------------------------------------- */

static bool seq_capture_blob(seq *g) {
  size_t length = 0u;
  t64_status status = t64_machine_state_export(seq_m(g), NULL, 0u, &length);

  if (status != T64_STATUS_BUFFER_TOO_SMALL || length < 64u) {
    return false;
  }
  g->blob = (uint8_t *)malloc(length);
  if (g->blob == NULL) {
    return false;
  }
  g->blob_length = length;
  status = t64_machine_state_export(seq_m(g), g->blob, length, &length);
  return status == T64_STATUS_OK && length == g->blob_length;
}

static void seq_print_summary(const seq *g) {
  unsigned i;

  (void)printf("sequence: seed 0x%llx steps %u calls %u (ok %u, refused %u, guest-stop %u, "
               "defect %u, poisoned-instance calls %u)\n",
               (unsigned long long)SEQ_SEED, g->steps, g->calls, g->class_counts[T64C_CLASS_OK],
               g->class_counts[T64C_CLASS_REFUSED], g->class_counts[T64C_CLASS_GUEST_STOP],
               g->class_counts[T64C_CLASS_DEFECT], g->poisoned_calls);
  (void)printf("sequence: status counts:");
  for (i = 0u; i < SEQ_STATUS_SLOTS; i++) {
    if (g->status_counts[i] != 0u) {
      (void)printf(" %u:%u", i, g->status_counts[i]);
    }
  }
  (void)printf("\nsequence: stop reason counts:");
  for (i = 0u; i < SEQ_STOP_SLOTS; i++) {
    if (g->stop_counts[i] != 0u) {
      (void)printf(" %u:%u", i, g->stop_counts[i]);
    }
  }
  (void)printf("\nsequence: variants drawn at random, per operation:");
  for (i = 0u; i < SEQ_OP_COUNT; i++) {
    unsigned k;
    unsigned drawn = 0u;
    unsigned total = 0u;

    for (k = 0u; k < seq_ops[i].variants; k++) {
      total += g->hits[i][k];
      drawn += g->hits[i][k] != 0u ? 1u : 0u;
    }
    (void)printf(" %s %u/%u (%u calls)", seq_ops[i].name, drawn, seq_ops[i].variants, total);
  }
  (void)printf("\nsequence: backpressure stalls %u resumed after a drain %u, terminal stops %u, "
               "milestones %u, PI/SI device latches %u\n",
               g->stalls, g->resumed, g->terminals, g->milestones, g->bus_latched);
  (void)printf("sequence: refusal bursts %u, machine digests verified %u, refusals without a "
               "digest check %u (stride %llu), state container %zu bytes\n",
               g->bursts, g->digests_verified, g->digests_skipped,
               (unsigned long long)g->digest_stride,
               g->blob_length);
}

static int case_sequence(const t64c_args *args) {
  seq *g = (seq *)calloc(1u, sizeof *g);
  unsigned i;
  int status;

  if (g == NULL) {
    return 2;
  }
  g->steps = args->steps == 0u ? SEQ_DEFAULT_STEPS : (unsigned)args->steps;
  if (args->steps > UINT32_C(10000000) || g->steps < SEQ_MIN_STEPS) {
    (void)fprintf(stderr, "t64c: --steps must be between %u and 10000000\n", SEQ_MIN_STEPS);
    free(g);
    return 2;
  }
  g->digest_stride = args->digest_stride == 0u ? SEQ_DEFAULT_STRIDE : args->digest_stride;
  g->fault = (unsigned)args->seed_fault;
  g->defect_step = g->steps - g->steps / 8u;
  g->milestone_a = g->steps / 4u;
  g->milestone_b = g->steps / 4u + g->steps / 2u;
  g->rng = SEQ_SEED;
  g->fx = t64c_fixture_new(args->rom_path);
  if (g->fx == NULL) {
    free(g);
    return 2;
  }
  for (i = 0u; i < sizeof g->save_pattern; i++) {
    g->save_pattern[i] = (uint8_t)(seq_next(g) >> 8);
  }
  g->rng = SEQ_SEED;
  g->cur_op = "setup";
  seq_check(g, t64c_fixture_admit(g->fx) == T64_STATUS_OK, "the composite ROM is admitted");
  seq_after_admit(g);
  seq_check(g, seq_capture_blob(g), "the state container of the first admission is captured");

  for (g->step = 0u; g->step < g->steps && !g->abort; g->step++) {
    if (g->step == g->defect_step) {
      seq_defect(g);
      if (!g->abort) {
        seq_poison_sweep(g);
      }
    } else if (!g->poisoned && (g->step == g->milestone_a || g->step == g->milestone_b)) {
      seq_milestone(g);
    } else {
      seq_random_step(g);
    }
  }
  g->cur_op = "end";
  if (!g->poisoned) {
    seq_close_burst(g);
  }
  seq_check(g, g->class_counts[T64C_CLASS_DEFECT] == 1u,
            "exactly one defect: the deliberate one");
  seq_check(g, g->class_counts[T64C_CLASS_REFUSED] >= g->steps / 4u,
            "refused calls are at least a quarter of the steps");
  seq_check(g, g->class_counts[T64C_CLASS_GUEST_STOP] >= 1u, "guest-stop results were observed");
  seq_check(g, g->milestones == 2u && g->stalls >= 2u && g->resumed >= 2u && g->terminals >= 2u,
            "both milestones stalled, resumed and ended terminal");
  seq_check(g, g->poisoned_calls >= 1u, "calls on the poisoned instance were made");

  t64_machine_destroy(seq_m(g));
  g->fx->machine = NULL;
  seq_check(g, g->fx->ledger.outstanding == 0u && g->fx->ledger.bad_releases == 0u,
            "destroy of the poisoned instance released every host allocation");
  seq_print_summary(g);
  status = t64c_report("sequence", &g->tally);
  t64c_fixture_free(g->fx);
  free(g->blob);
  free(g);
  return status;
}

/* ---- dispatch ------------------------------------------------------------------------- */

bool t64c_sequence_known(const char *name) { return strcmp(name, "sequence") == 0; }

void t64c_sequence_list(void) { (void)puts("sequence"); }

int t64c_sequence_run(const char *name, const t64c_args *args) {
  if (strcmp(name, "sequence") == 0) {
    return case_sequence(args);
  }
  return 2;
}
