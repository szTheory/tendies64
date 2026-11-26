/*
 * Shared declarations of the installed consumer's cases (Phase 11, plan
 * 11-18): the argument record, the machine fixture, and the run engine that the
 * composite case, the three-way print and the ring cases share. Public header
 * and system headers only.
 */
#ifndef T64C_CASES_H
#define T64C_CASES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <tendies64/t64_machine.h>

#include "t64c_alloc.h"
#include "t64c_check.h"

/* Command-line options of a case. */
typedef struct {
  const char *rom_path; /* --rom PATH: the authored composite ROM */
  bool print;           /* --print: machine-readable values on stdout */
  const char *out_dir;  /* --out DIR: write the run's outputs as files */
  uint64_t slice;       /* --slice N: ticks per advance (0 selects the default) */
} t64c_args;

#define T64C_PROFILE_ID "ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k"
/* The guest-tick bound the runner uses for the composite fixture. */
#define T64C_RUN_BOUND UINT64_C(600000)
#define T64C_RUN_DEFAULT_SLICE UINT64_C(4096)
#define T64C_RUN_MAX_VIDEO 8u
#define T64C_RUN_MAX_AUDIO 4u
#define T64C_RUN_VIDEO_BYTES 48u
#define T64C_RUN_AUDIO_BYTES 16u
#define T64C_RUN_TEXT_MAX 512u
#define T64C_RUN_SAVE_BYTES 32768u

/* A machine plus the allocator ledger it uses and the ROM bytes of the file. */
typedef struct {
  t64c_ledger ledger;
  t64_host_allocator allocator;
  const t64_profile_descriptor *profile;
  t64_machine *machine;
  uint8_t *rom;
  size_t rom_length;
} t64c_fixture;

/* Read the ROM file, look up the composite profile and create a machine (not
 * admitted). NULL, with a message on stderr, on any failure. */
t64c_fixture *t64c_fixture_new(const char *rom_path);
/* Destroy the machine (when present) and free the fixture. */
void t64c_fixture_free(t64c_fixture *fixture);
/* t64_machine_admit_rom of the fixture's ROM bytes. */
t64_status t64c_fixture_admit(t64c_fixture *fixture);
/* Queue the two literal input events of the composite fixture. */
bool t64c_queue_literal_inputs(t64c_fixture *fixture);

/* Everything one run delivers to the host. */
typedef struct {
  uint64_t tick;
  uint32_t stop_reason;
  unsigned advances;
  unsigned resource_stops;
  uint8_t digest[T64_DIGEST_SIZE];
  size_t video_count;
  uint32_t video_width[T64C_RUN_MAX_VIDEO];
  uint32_t video_height[T64C_RUN_MAX_VIDEO];
  uint64_t video_packet_bytes[T64C_RUN_MAX_VIDEO];
  uint8_t video[T64C_RUN_MAX_VIDEO][T64C_RUN_VIDEO_BYTES];
  size_t audio_count;
  uint64_t audio_packet_bytes[T64C_RUN_MAX_AUDIO];
  uint8_t audio[T64C_RUN_MAX_AUDIO][T64C_RUN_AUDIO_BYTES];
  uint8_t text[T64C_RUN_TEXT_MAX];
  size_t text_length;
  uint8_t save[T64C_RUN_SAVE_BYTES];
  size_t save_length;
} t64c_run;

/* Drain every queued video packet, every queued audio packet and the new
 * ISViewer text into the run record. False on an unexpected status. */
bool t64c_drain_all(t64c_fixture *fixture, t64c_run *run);
/* The three drains separately (video and audio packets; the new ISViewer text). */
bool t64c_drain_video(t64c_fixture *fixture, t64c_run *run);
bool t64c_drain_audio(t64c_fixture *fixture, t64c_run *run);
bool t64c_drain_text(t64c_fixture *fixture, t64c_run *run);
/* Advance the admitted machine in slices of slice ticks (0: the default) up to
 * T64C_RUN_BOUND, draining after every advance, until the guest completes.
 * Records the stop, the tick and the number of RESOURCE_LIMIT stops. */
bool t64c_run_to_completion(t64c_fixture *fixture, uint64_t slice,
                            t64c_run *run);
/* After completion: the machine digest and the exported SRAM save. */
bool t64c_finish_run(t64c_fixture *fixture, t64c_run *run);

/* The smoke case (smoke.c): header and archive agree, create and destroy balance. */
int t64c_case_smoke(void);

/* The composite case (composite.c). */
int t64c_case_composite(const t64c_args *args);

/* Cases of ops.c: the names are the case names of cases.txt. */
bool t64c_ops_known(const char *name);
void t64c_ops_list(void);
int t64c_ops_run(const char *name, const t64c_args *args);

/* Cases of hostloop.c (plan 11-19): the host-shaped loop and the two-instance case. */
bool t64c_hostloop_known(const char *name);
void t64c_hostloop_list(void);
int t64c_hostloop_run(const char *name, const t64c_args *args);

#endif /* T64C_CASES_H */
