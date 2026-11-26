/*
 * The optional SDL3 shell of the minimal host (Phase 12, plan 12-09, API-05, decisions DR-17 to
 * DR-19): a window, an audio stream and a gamepad around the pure-C pump. Built only with
 * -DT64H_SDL=ON against a system SDL3 (link-only: nothing of SDL3 is vendored, fetched or
 * copied); the gate never builds it. t64h_sdl.c is the shell, built as a static library;
 * t64h_sdl_main.c is the executable's main and t64h_sdl_smoke.c is the headless self-check of the
 * lane `sh scripts/check-host.sh sdl`; both link the library.
 *
 * Presentation is a blit of each video packet to the window surface with nearest sampling at an
 * integer scale: no renderer, no GPU path, no icon, and the one window title below. Input is
 * controller only: the first gamepad present at start is port 0, every sample goes through
 * t64h_map_pad, a removed pad is neutral and keyboard events are never read. There is no
 * host-side save file (the headless runs already prove the save bytes, decision DR-19).
 */
#ifndef T64H_SDL_H
#define T64H_SDL_H

#include <stddef.h>
#include <stdint.h>

#include <SDL3/SDL.h>
#include <tendies64/t64_machine.h>

#include "t64h_frames.h"
#include "t64h_input.h"
#include "t64h_pump.h"

/* The only window title of the shell (decision D7: no product name, no branding). */
#define T64H_SDL_TITLE "t64 minimal host (development)"

/* The screen of the title: 128 x 96 pixels, shown at an integer scale. */
#define T64H_SDL_SCREEN_WIDTH 128
#define T64H_SDL_SCREEN_HEIGHT 96

typedef struct {
  t64_host_allocator allocator;
  t64_machine *machine;
  uint8_t *rom;             /* the admitted ROM image, kept until close */
  uint8_t *buffers;         /* the pump's video, audio and text buffers and the pixel copy */
  t64h_pump pump;
  SDL_Window *window;
  SDL_AudioStream *audio;   /* opened by the first audio packet */
  SDL_Gamepad *pad;         /* the first gamepad present at start, or NULL */
  int scale;
  int dirty;                /* a frame was blitted and the window surface is not shown yet */
  int quit;                 /* the window was closed */
  int audio_ready;          /* the stream held data at the last poll */
  uint32_t audio_hz;
  t64h_pad pad_now;         /* the sampled logical pad of this turn */
  t64h_pad pad_queued;      /* the logical pad the guest was last given */
  t64h_status status;       /* the status of the last pump step */
  uint32_t stop_reason;
  uint32_t frame_width;     /* the geometry of the last presented frame */
  uint32_t frame_height;
  uint64_t frames;          /* video packets blitted to the window surface */
  uint64_t bad_frames;      /* video packets refused by t64h_frame_copy or SDL */
  uint64_t audio_packets;   /* packets queued to the audio stream */
  uint64_t audio_dropped;   /* packets with an unusable format or a refused queue */
  uint64_t underruns;       /* times the audio stream ran empty after it had data (silence plays) */
  uint64_t input_events;    /* logical pad changes queued to the machine */
} t64h_sdl_shell;

/* Initialize SDL, load the title ROM, create the machine, the window (128 x 96 times scale, scale
 * 1..8) and the pump, and open the first gamepad. 0 on success; on failure it prints one
 * path-free line to stderr, releases what it made and returns nonzero. */
int t64h_sdl_open(t64h_sdl_shell *shell, const char *rom_path, int scale);

/* One loop turn: window events, the pad, one pump step of at most 65,536 ticks, the present of
 * the window surface and the audio poll. 0 continues; 1 ends normally (window closed or the
 * guest ended); -1 is a pump error (already printed). */
int t64h_sdl_turn(t64h_sdl_shell *shell);

/* The first gamepad as the logical pad the core takes (neutral when there is none). */
t64h_pad t64h_sdl_sample_pad(t64h_sdl_shell *shell);

/* The tight RGBA copy (width * 4 bytes a row) of the last frame blitted, for the smoke. */
const uint8_t *t64h_sdl_frame_pixels(const t64h_sdl_shell *shell);

/* Release everything t64h_sdl_open made and quit SDL. */
void t64h_sdl_close(t64h_sdl_shell *shell);

#endif /* T64H_SDL_H */
