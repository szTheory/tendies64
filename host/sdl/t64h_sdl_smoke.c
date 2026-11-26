/*
 * The headless self-check of the SDL3 shell (Phase 12, plan 12-09, API-05, decision DR-17), run by
 * `sh scripts/check-host.sh sdl` and never by CTest: with the dummy video and audio drivers it
 * attaches a virtual gamepad, drives every row of the mapping table through the shell's own pad
 * sampler and checks the logical pad, then plays the title through the pump into a window and an
 * audio stream and checks the window title, the frames and the queued input.
 *
 * It is self-checking host glue only: the dummy drivers show nothing, so no claim about
 * rendering, timing or audio fidelity follows from a pass. Usage: t64h_sdl_smoke --rom PATH.
 * Stdout carries only fixed, sanitized lines ("[host-sdl] ..."); SDL error texts go to stderr.
 */
#include <stdio.h>
#include <string.h>

#include "t64h_sdl.h"

#define NEUTRAL_TRIGGER (-32768)  /* a virtual trigger axis at rest reads 0 through the gamepad */

typedef struct {
  int is_axis;      /* 0: virtual button INDEX goes down; 1: virtual axis INDEX is set to VALUE */
  int index;        /* an SDL gamepad button or axis number (the virtual pad uses the same order) */
  int16_t value;
  uint16_t buttons; /* the expected logical pad */
  int8_t x;
  int8_t y;
} row;

/* Expected values come from the mapping table of t64h_input.h, written out here independently. */
static const row rows[] = {
    {0, SDL_GAMEPAD_BUTTON_SOUTH, 0, T64H_PAD_A, 0, 0},
    {0, SDL_GAMEPAD_BUTTON_EAST, 0, T64H_PAD_B, 0, 0},
    {0, SDL_GAMEPAD_BUTTON_START, 0, T64H_PAD_START, 0, 0},
    {0, SDL_GAMEPAD_BUTTON_DPAD_UP, 0, T64H_PAD_UP, 0, 0},
    {0, SDL_GAMEPAD_BUTTON_DPAD_DOWN, 0, T64H_PAD_DOWN, 0, 0},
    {0, SDL_GAMEPAD_BUTTON_DPAD_LEFT, 0, T64H_PAD_LEFT, 0, 0},
    {0, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, 0, T64H_PAD_RIGHT, 0, 0},
    {0, SDL_GAMEPAD_BUTTON_WEST, 0, 0, 0, 0},
    {0, SDL_GAMEPAD_BUTTON_NORTH, 0, 0, 0, 0},
    {0, SDL_GAMEPAD_BUTTON_BACK, 0, 0, 0, 0},
    {0, SDL_GAMEPAD_BUTTON_GUIDE, 0, 0, 0, 0},
    {0, SDL_GAMEPAD_BUTTON_LEFT_STICK, 0, 0, 0, 0},
    {0, SDL_GAMEPAD_BUTTON_RIGHT_STICK, 0, 0, 0, 0},
    {0, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, 0, 0, 0, 0},
    {0, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, 0, 0, 0, 0},
    {1, SDL_GAMEPAD_AXIS_LEFTX, 32767, 0, 127, 0},
    {1, SDL_GAMEPAD_AXIS_LEFTX, -32768, 0, -128, 0},
    {1, SDL_GAMEPAD_AXIS_LEFTX, 2047, 0, 0, 0},
    {1, SDL_GAMEPAD_AXIS_LEFTX, 2048, 0, 8, 0},
    {1, SDL_GAMEPAD_AXIS_LEFTX, -2048, 0, -8, 0},
    {1, SDL_GAMEPAD_AXIS_LEFTY, 32767, 0, 0, -128},
    {1, SDL_GAMEPAD_AXIS_LEFTY, -32768, 0, 0, 127},
    {1, SDL_GAMEPAD_AXIS_LEFTY, 0, 0, 0, 0},
    {1, SDL_GAMEPAD_AXIS_RIGHTX, 32767, 0, 0, 0},
    {1, SDL_GAMEPAD_AXIS_RIGHTY, -32768, 0, 0, 0},
    {1, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 32767, T64H_PAD_Z, 0, 0},
    {1, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 8000, T64H_PAD_Z, 0, 0},
    {1, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, -8000, 0, 0, 0},
    {1, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, NEUTRAL_TRIGGER, 0, 0, 0},
    {1, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 32767, 0, 0, 0},
};
#define ROW_COUNT ((int)(sizeof rows / sizeof rows[0]))
#define VIRTUAL_BUTTONS 15  /* south .. dpad right, in the order of SDL_GamepadButton */

static void say(const char *text) {
  (void)printf("[host-sdl] %s\n", text);
  (void)fflush(stdout);
}

static int fail(const char *text) {
  (void)printf("[host-sdl] FAIL: %s\n", text);
  (void)fflush(stdout);
  return 1;
}

/* Every virtual control at rest: buttons up, sticks centered, triggers released. */
static void rest(SDL_Joystick *joystick) {
  int i;

  for (i = 0; i < VIRTUAL_BUTTONS; i++) {
    (void)SDL_SetJoystickVirtualButton(joystick, i, false);
  }
  for (i = 0; i < SDL_GAMEPAD_AXIS_COUNT; i++) {
    int16_t centered = 0;

    if (i == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || i == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
      centered = NEUTRAL_TRIGGER;
    }
    (void)SDL_SetJoystickVirtualAxis(joystick, i, centered);
  }
}

static int check_rows(t64h_sdl_shell *shell, SDL_Joystick *joystick, int *rows_ok) {
  int r;
  char text[48];

  *rows_ok = 0;
  for (r = 0; r < ROW_COUNT; r++) {
    t64h_pad pad;

    rest(joystick);
    if (rows[r].is_axis) {
      (void)SDL_SetJoystickVirtualAxis(joystick, rows[r].index, rows[r].value);
    } else {
      (void)SDL_SetJoystickVirtualButton(joystick, rows[r].index, true);
    }
    pad = t64h_sdl_sample_pad(shell);
    if (pad.buttons != rows[r].buttons || pad.x != rows[r].x || pad.y != rows[r].y) {
      (void)snprintf(text, sizeof text, "mapping row %d", r);
      return fail(text);
    }
    (*rows_ok)++;
  }
  rest(joystick);
  return 0;
}

/* The title's first frames through the pump into the window, with a held button queued to the
 * guest. A frame needs at most one pump step (a step is far shorter than a field). */
static int check_title(t64h_sdl_shell *shell, SDL_Joystick *joystick) {
  int w = 0;
  int h = 0;
  const Uint64 deadline = SDL_GetTicksNS() + UINT64_C(10) * 1000000000u;

  if (strcmp(SDL_GetWindowTitle(shell->window), "t64 minimal host (development)") != 0) {
    return fail("window title");
  }
  if (!SDL_GetWindowSize(shell->window, &w, &h) || w != T64H_SDL_SCREEN_WIDTH * shell->scale ||
      h != T64H_SDL_SCREEN_HEIGHT * shell->scale) {
    return fail("window size");
  }
  (void)SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, true);
  while (shell->frames < 8u) {
    if (t64h_sdl_turn(shell) != 0 || SDL_GetTicksNS() > deadline) {
      return fail("title frames");
    }
  }
  (void)SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, false);
  if (shell->frames != 8u || shell->bad_frames != 0u || shell->frame_width != 128u ||
      shell->frame_height != 96u) {
    return fail("title geometry");
  }
  if (shell->input_events == 0u) {
    return fail("input events");
  }
  if (shell->audio == NULL || shell->audio_packets == 0u || shell->audio_dropped != 0u) {
    return fail("audio stream");
  }
  return 0;
}

/* The blit: at the integer scale, every pixel of the window surface inside the block of a frame
 * pixel equals that frame pixel (nearest sampling), at the corners of the 16 x 16 square the
 * title draws and at pixels outside it. */
static int check_blit(t64h_sdl_shell *shell) {
  static const int points[][2] = {{0, 0}, {127, 95}, {55, 39}, {56, 40}, {64, 48},
                                  {71, 55}, {72, 56}, {10, 20}, {100, 70}};
  SDL_Surface *screen = SDL_GetWindowSurface(shell->window);
  const uint8_t *pixels = t64h_sdl_frame_pixels(shell);
  size_t p;
  int corner;

  if (screen == NULL || screen->w != T64H_SDL_SCREEN_WIDTH * shell->scale) {
    return fail("window surface");
  }
  for (p = 0u; p < sizeof points / sizeof points[0]; p++) {
    const uint8_t *want = pixels + ((size_t)points[p][1] * T64H_SDL_SCREEN_WIDTH + (size_t)points[p][0]) * 4u;

    for (corner = 0; corner < 4; corner++) {
      Uint8 r = 0;
      Uint8 g = 0;
      Uint8 b = 0;
      Uint8 a = 0;
      const int x = points[p][0] * shell->scale + (corner & 1 ? shell->scale - 1 : 0);
      const int y = points[p][1] * shell->scale + (corner & 2 ? shell->scale - 1 : 0);

      if (!SDL_ReadSurfacePixel(screen, x, y, &r, &g, &b, &a) || r != want[0] || g != want[1] ||
          b != want[2]) {
        return fail("blit");
      }
    }
  }
  return 0;
}

static int run(const char *rom) {
  static const char *want = "dummy";
  SDL_VirtualJoystickDesc desc;
  SDL_JoystickID id;
  SDL_Joystick *joystick;
  t64h_sdl_shell shell;
  const char *video;
  const char *audio;
  char text[48];
  int version;
  int rows_ok = 0;
  int rc;

  if (SDL_getenv("SDL_VIDEO_DRIVER") == NULL || SDL_getenv("SDL_AUDIO_DRIVER") == NULL ||
      strcmp(SDL_getenv("SDL_VIDEO_DRIVER"), want) != 0 ||
      strcmp(SDL_getenv("SDL_AUDIO_DRIVER"), want) != 0) {
    return fail("the dummy drivers are not selected by the environment");
  }
  if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
    (void)fprintf(stderr, "t64h_sdl_smoke: %s\n", SDL_GetError());
    return fail("gamepad subsystem");
  }
  SDL_INIT_INTERFACE(&desc);
  desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
  desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
  desc.nbuttons = VIRTUAL_BUTTONS;
  desc.name = "virtual pad";
  id = SDL_AttachVirtualJoystick(&desc);
  joystick = id != 0u ? SDL_OpenJoystick(id) : NULL;
  if (joystick == NULL) {
    (void)fprintf(stderr, "t64h_sdl_smoke: %s\n", SDL_GetError());
    SDL_Quit();
    return fail("virtual gamepad");
  }
  rest(joystick);
  if (t64h_sdl_open(&shell, rom, 2) != 0) {
    SDL_Quit();
    return fail("shell open");
  }
  version = SDL_GetVersion();
  (void)snprintf(text, sizeof text, "sdl %d.%d.%d", SDL_VERSIONNUM_MAJOR(version),
                 SDL_VERSIONNUM_MINOR(version), SDL_VERSIONNUM_MICRO(version));
  say(text);
  video = SDL_GetCurrentVideoDriver();
  audio = SDL_GetCurrentAudioDriver();
  if (video == NULL || audio == NULL || strcmp(video, want) != 0 || strcmp(audio, want) != 0) {
    rc = fail("driver names");
  } else {
    say("video-driver dummy");
    say("audio-driver dummy");
    rc = shell.pad == NULL ? fail("the shell found no gamepad") : check_rows(&shell, joystick, &rows_ok);
  }
  if (rc == 0) {
    (void)snprintf(text, sizeof text, "mapping rows %d/%d", rows_ok, ROW_COUNT);
    say(text);
    rc = check_title(&shell, joystick);
  }
  if (rc == 0) {
    rc = check_blit(&shell);
  }
  if (rc == 0) {
    say("title ok");
    say("blit ok");
    (void)snprintf(text, sizeof text, "frames %llu", (unsigned long long)shell.frames);
    say(text);
    say("audio ok");
  }
  SDL_CloseJoystick(joystick);
  (void)SDL_DetachVirtualJoystick(id);
  t64h_sdl_close(&shell);
  return rc;
}

int main(int argc, char **argv) {
  int rc;

  if (argc != 3 || strcmp(argv[1], "--rom") != 0) {
    (void)fprintf(stderr, "usage: t64h_sdl_smoke --rom PATH\n");
    return 2;
  }
  rc = run(argv[2]);
  if (rc == 0) {
    say("PASS");
  }
  return rc;
}
