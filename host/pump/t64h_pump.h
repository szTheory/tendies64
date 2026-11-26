/*
 * The host pump (Phase 12, plan 12-07, API-05, decisions DR-11, DR-14, DR-15).
 *
 * Pure C99 on the installed package: this file and t64h_pump.c include only
 * <tendies64/t64_machine.h> and C99 standard headers (scripts/check-host-includes.sh
 * guards it). The pump reads no clock, file, entropy or thread of its own: host
 * time enters through the injected clock callback and nothing else, so the pacing
 * is a pure function of the clock values the host supplies and a test can drive it
 * with a fake clock.
 *
 * One step reads the clock once and converts the elapsed host nanoseconds, counted
 * from the origin the clock showed at init, into an absolute guest-tick bound: the
 * guest tick of these profiles runs at 62,500,000 ticks per second, one tick per
 * 16 ns, a host policy value the core never reads (docs/spec/api.md section 8.3).
 * The step then advances the machine to that bound, drains the video ring, the
 * audio ring and the ISViewer text, in that order, to the sinks inside the advance
 * loop whenever the machine stops for lack of ring space and once more at the end
 * of the step, and reports why it stopped.
 *
 * A RESOURCE_LIMIT stop that moved the guest tick is the per-advance event budget
 * and the loop simply continues; one that moved nothing is ring backpressure: the
 * pump drains and retries once, and a second stop that moved nothing after a drain
 * that delivered nothing is the permanent "stall without progress"
 * (docs/spec/api.md section 8.1). While a sink-refused packet is held, guest
 * movement is not progress (the copy into the pump freed the ring slot, the sink
 * has taken nothing), so a sink that refuses everything also ends in the stall,
 * after one retry, and loses nothing: the held packet is offered first again.
 *
 * After a terminal stop of the guest (GUEST_COMPLETION or CPU_UNSUPPORTED) the step
 * that saw it drains once more and every later step answers T64H_TERMINAL without
 * reading the clock, calling the input hook, advancing or delivering; a pump is
 * not reusable after a reset or a replaced image of its machine (init a new one).
 */
#ifndef T64H_PUMP_H
#define T64H_PUMP_H

#include <stddef.h>
#include <stdint.h>

#include <tendies64/t64_machine.h>

/* Host nanoseconds per guest tick (62,500,000 ticks per second). */
#define T64H_TICK_NS UINT64_C(16)

/* Minimum buffer sizes that never refuse a packet of these profiles: a 320 x 240
 * RGBA8 frame (the largest video packet the host supports), an AI DMA descriptor
 * (at most 0x3fff8 bytes) and the 4096-byte retained ISViewer prefix. A smaller
 * buffer is legal: a packet that does not fit ends the step with T64H_API_ERROR
 * and T64_STATUS_BUFFER_TOO_SMALL. */
#define T64H_VIDEO_BUFFER_BYTES 307200u
#define T64H_AUDIO_BUFFER_BYTES 262144u
#define T64H_TEXT_BUFFER_BYTES 4096u

typedef enum {
  T64H_OK = 0,          /* the step ran to its bound (or to a stop of the guest) */
  T64H_CLOCK_BACKWARD,  /* the clock went backward: counted, nothing advanced */
  T64H_STALL,           /* "stall without progress": the machine can not move */
  T64H_TERMINAL,        /* the guest ended earlier: nothing is advanced or queued */
  T64H_API_ERROR,       /* a core call answered a status the pump can not handle */
  T64H_INPUT_ERROR,     /* the input hook reported a failure */
  T64H_BAD_ARGUMENT     /* a NULL or unusable argument of init or step */
} t64h_status;

/* The name of a status, for logs. */
const char *t64h_status_name(t64h_status status);

/* Guest ticks elapsed after the given host nanoseconds: elapsed_ns / 16. */
uint64_t t64h_ticks_from_ns(uint64_t elapsed_ns);

/* The monotonic clock of the host, in nanoseconds. It is the pump's only source
 * of time; a test passes a fake. */
typedef uint64_t (*t64h_clock_fn)(void *context);

/* Called once before every advance of a step with the guest tick reached and the
 * coming bound: the hook queues, through t64_machine_queue_input, every input event
 * whose tick is at most *bound. A hook that could not queue such an event (the core
 * answered QUEUE_FULL) lowers *bound to that event's tick, so the guest never passes
 * an event the machine does not hold yet (a late event would be TIME_REVERSED); the
 * pump never raises the bound and keeps it at the guest tick or above. The step then
 * ends at the lowered bound and the event is tried again by the next step. The hook
 * returns 0, or nonzero to end the step with T64H_INPUT_ERROR. It is never called
 * after a terminal stop of the guest. */
typedef int (*t64h_input_fn)(void *context, t64_machine *machine,
                             uint64_t guest_tick, uint64_t *bound);

/* The sinks take the drained output. A callback returns 0 when it consumed the
 * packet or the text. A nonzero return refuses it: the pump keeps the data (it is
 * already out of the ring), offers it again first at the next drain pass and takes
 * no further data of that kind in between. A NULL callback consumes everything of
 * its kind (the data is drained and dropped, so the machine never waits on it). */
typedef int (*t64h_video_fn)(void *context, const t64_video_packet_info *info,
                             const uint8_t *bytes, size_t length);
typedef int (*t64h_audio_fn)(void *context, const t64_audio_packet_info *info,
                             const uint8_t *bytes, size_t length);
typedef int (*t64h_text_fn)(void *context, const uint8_t *bytes, size_t length);

typedef struct {
  void *context;
  t64h_video_fn video;
  t64h_audio_fn audio;
  t64h_text_fn text;
} t64h_sinks;

/* Everything the pump is given. The buffers are the caller's (the pump allocates
 * nothing); see T64H_*_BUFFER_BYTES for sizes that never refuse a packet. */
typedef struct {
  t64_machine *machine;  /* admitted, at guest tick 0, not advanced by anyone else */
  t64h_clock_fn clock;
  void *clock_context;
  t64h_sinks sinks;
  t64h_input_fn input;   /* optional */
  void *input_context;
  uint8_t *video_buffer;
  size_t video_capacity;
  uint8_t *audio_buffer;
  size_t audio_capacity;
  uint8_t *text_buffer;
  size_t text_capacity;
} t64h_config;

typedef struct {
  uint64_t steps;               /* calls of t64h_pump_step */
  uint64_t advances;            /* t64_machine_advance calls */
  uint64_t budget_stops;        /* RESOURCE_LIMIT stops that moved the guest tick */
  uint64_t backpressure_stops;  /* RESOURCE_LIMIT stops that moved nothing */
  uint64_t stall_retries;       /* advances retried after a drain of such a stop */
  uint64_t drain_passes;        /* drain passes (video, then audio, then text) */
  uint64_t video_packets;       /* video packets consumed by the sink */
  uint64_t audio_packets;       /* audio packets consumed by the sink */
  uint64_t text_bytes;          /* ISViewer bytes consumed by the sink */
  uint64_t sink_refusals;       /* offers a sink refused */
  uint64_t backward_clocks;     /* steps refused because the clock went backward */
} t64h_counters;

typedef struct {
  t64_machine *machine;
  t64h_config config;
  uint64_t origin_ns;       /* the clock value at init */
  uint64_t last_ns;         /* the latest clock value accepted */
  uint64_t tick;            /* the guest tick the last advance reached */
  uint32_t last_reason;     /* T64_STOP_REASON_* of the last advance (0 before any) */
  int terminal;             /* the guest reached a terminal stop */
  int video_held;           /* a refused packet waits in config.video_buffer */
  size_t video_held_length;
  t64_video_packet_info video_held_info;
  int audio_held;
  size_t audio_held_length;
  t64_audio_packet_info audio_held_info;
  int text_held;            /* refused text waits in config.text_buffer */
  size_t text_held_length;
  t64h_counters counters;
} t64h_pump;

typedef struct {
  uint64_t tick;            /* the guest tick reached */
  uint32_t stop_reason;     /* T64_STOP_REASON_* of the last advance of the step */
  t64_status api_status;    /* the core status behind T64H_API_ERROR, else T64_STATUS_OK */
  const char *message;      /* a fixed text for STALL, TERMINAL and the errors, else NULL */
} t64h_step_result;

/* Bind the pump to a machine and read the clock once for the origin. T64H_OK, or
 * T64H_BAD_ARGUMENT for a NULL machine, clock or buffer. */
t64h_status t64h_pump_init(t64h_pump *pump, const t64h_config *config);

/* One step of host time: bring the guest to the tick the clock allows, but at most
 * max_ticks beyond the tick it has (UINT64_MAX is no cap), drain what the guest
 * produced and report. The result is filled on every return. */
t64h_status t64h_pump_step(t64h_pump *pump, uint64_t max_ticks,
                           t64h_step_result *result);

#endif /* T64H_PUMP_H */
