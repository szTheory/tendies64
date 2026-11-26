/*
 * The optional SDL3 shell of the minimal host (Phase 12, plan 12-09, API-05). See t64h_sdl.h for
 * the contract. The executable's main is t64h_sdl_main.c; the smoke links this file as a library.
 */
#include "t64h_sdl.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROFILE_ID "ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k"
#define MAX_ROM_BYTES (64u * 1024u * 1024u)
#define STEP_TICKS UINT64_C(65536)  /* at most this many guest ticks between two event pumps */
#define MAX_AUDIO_HZ 192000u
/* The tight RGBA copy of the last frame follows the pump's three buffers in shell->buffers. */
#define PIXELS_OFFSET (T64H_VIDEO_BUFFER_BYTES + T64H_AUDIO_BUFFER_BYTES + T64H_TEXT_BUFFER_BYTES)

/* ---- the allocator, the clock and the ROM file ------------------------------------------- */

/* Aligned by arithmetic over malloc (the public header compiles as C99): the raw pointer is kept
 * in the word before the block the core sees. */
static bool shell_allocate(void *context, size_t size, size_t alignment, t64_host_storage *out) {
  size_t align = alignment;
  uint8_t *raw;
  uintptr_t aligned;

  (void)context;
  if (out == NULL || size == 0u || alignment == 0u || (alignment & (alignment - 1u)) != 0u) {
    return false;
  }
  if (align < sizeof(void *)) {
    align = sizeof(void *);
  }
  raw = (uint8_t *)malloc(size + align + sizeof(void *));
  if (raw == NULL) {
    return false;
  }
  aligned = ((uintptr_t)raw + sizeof(void *) + align - 1u) / align * align;
  ((void **)aligned)[-1] = raw;
  out->data = (void *)aligned;
  out->capacity = size;
  out->alignment = align;
  return true;
}

static void shell_release(void *context, t64_host_storage storage) {
  (void)context;
  if (storage.data != NULL) {
    free(((void **)storage.data)[-1]);
  }
}

/* The pump's only source of time: the monotonic clock of SDL. */
static uint64_t shell_clock(void *context) {
  (void)context;
  return SDL_GetTicksNS();
}

static uint8_t *read_rom(const char *path, size_t *length_out) {
  FILE *file = fopen(path, "rb");
  long size;
  uint8_t *bytes;

  if (file == NULL) {
    return NULL;
  }
  if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) <= 0 ||
      (unsigned long)size > MAX_ROM_BYTES || fseek(file, 0, SEEK_SET) != 0) {
    (void)fclose(file);
    return NULL;
  }
  bytes = (uint8_t *)malloc((size_t)size);
  if (bytes != NULL && fread(bytes, 1u, (size_t)size, file) != (size_t)size) {
    free(bytes);
    bytes = NULL;
  }
  (void)fclose(file);
  *length_out = (size_t)size;
  return bytes;
}

/* ---- the sinks and the input hook of the pump -------------------------------------------- */

/* A video packet: copied (stride and all) into the tight RGBA pixel buffer, wrapped as a surface
 * and blitted to the window surface at an integer scale with nearest sampling. Always consumed:
 * a packet the host can not show is counted, never held (a held packet would stall the pump). */
static int shell_video(void *context, const t64_video_packet_info *info, const uint8_t *bytes,
                       size_t length) {
  t64h_sdl_shell *shell = (t64h_sdl_shell *)context;
  uint8_t *pixels = shell->buffers + PIXELS_OFFSET;
  const size_t pitch = (size_t)info->width * T64H_FRAME_BYTES_PER_PIXEL;
  SDL_Surface *frame = NULL;
  SDL_Surface *screen;
  SDL_Rect target;

  if (t64h_frame_copy(info, bytes, length, pixels, T64H_VIDEO_BUFFER_BYTES, pitch) !=
      T64H_FRAME_OK) {
    shell->bad_frames++;
    return 0;
  }
  screen = SDL_GetWindowSurface(shell->window);
  if (screen != NULL) {
    frame = SDL_CreateSurfaceFrom((int)info->width, (int)info->height, SDL_PIXELFORMAT_RGBA32,
                                  pixels, (int)pitch);
  }
  target.x = 0;
  target.y = 0;
  target.w = (int)info->width * shell->scale;
  target.h = (int)info->height * shell->scale;
  if (frame != NULL && SDL_BlitSurfaceScaled(frame, NULL, screen, &target, SDL_SCALEMODE_NEAREST)) {
    shell->frames++;
    shell->dirty = 1;
    shell->frame_width = info->width;
    shell->frame_height = info->height;
  } else {
    shell->bad_frames++;
  }
  if (frame != NULL) {
    SDL_DestroySurface(frame);
  }
  return 0;
}

/* An audio packet: S16BE stereo queued to an SDL stream whose source rate is the packet's
 * rational rate rounded to whole Hz (a conversion for the device, no fidelity claim). The stream
 * opens at the first packet; SDL plays silence while it holds no data. */
static int shell_audio(void *context, const t64_audio_packet_info *info, const uint8_t *bytes,
                       size_t length) {
  t64h_sdl_shell *shell = (t64h_sdl_shell *)context;
  SDL_AudioSpec spec;
  uint64_t hz = 0u;

  if (info->rate_denominator != 0u) {
    hz = (info->rate_numerator + info->rate_denominator / 2u) / info->rate_denominator;
  }
  if (hz == 0u || hz > MAX_AUDIO_HZ || info->channels != 2u || info->bits_per_sample != 16u ||
      info->encoding != T64_AUDIO_ENCODING_S16BE || length > (size_t)INT_MAX) {
    shell->audio_dropped++;
    return 0;
  }
  spec.format = SDL_AUDIO_S16BE;
  spec.channels = 2;
  spec.freq = (int)hz;
  if (shell->audio == NULL) {
    shell->audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (shell->audio != NULL) {
      (void)SDL_ResumeAudioStreamDevice(shell->audio);
      shell->audio_hz = (uint32_t)hz;
    }
  } else if (shell->audio_hz != (uint32_t)hz && SDL_SetAudioStreamFormat(shell->audio, &spec, NULL)) {
    shell->audio_hz = (uint32_t)hz;
  }
  if (shell->audio != NULL && shell->audio_hz == (uint32_t)hz &&
      SDL_PutAudioStreamData(shell->audio, bytes, (int)length)) {
    shell->audio_packets++;
  } else {
    shell->audio_dropped++;
  }
  return 0;
}

/* Before every advance: give the guest the sampled pad when it changed, stamped with the tick the
 * guest has reached (never behind it). A full queue is not an error: the change is offered again
 * at the next call and the bound is left alone. */
static int shell_input(void *context, t64_machine *machine, uint64_t guest_tick, uint64_t *bound) {
  t64h_sdl_shell *shell = (t64h_sdl_shell *)context;
  t64_status status;

  (void)bound;
  if (shell->pad_now.buttons == shell->pad_queued.buttons && shell->pad_now.x == shell->pad_queued.x &&
      shell->pad_now.y == shell->pad_queued.y) {
    return 0;
  }
  status = t64_machine_queue_input(machine, guest_tick, 0u, shell->pad_now.buttons,
                                   shell->pad_now.x, shell->pad_now.y);
  if (status == T64_STATUS_OK) {
    shell->pad_queued = shell->pad_now;
    shell->input_events++;
    return 0;
  }
  return status == T64_STATUS_QUEUE_FULL ? 0 : 1;
}

/* ---- the pad ----------------------------------------------------------------------------- */

typedef struct {
  SDL_GamepadButton button;
  uint32_t raw;
} pad_button;

/* Every SDL gamepad button has a raw bit; t64h_map_pad decides which of them the core sees. */
static const pad_button pad_buttons[] = {
    {SDL_GAMEPAD_BUTTON_SOUTH, T64H_RAW_A},
    {SDL_GAMEPAD_BUTTON_EAST, T64H_RAW_B},
    {SDL_GAMEPAD_BUTTON_WEST, T64H_RAW_X},
    {SDL_GAMEPAD_BUTTON_NORTH, T64H_RAW_Y},
    {SDL_GAMEPAD_BUTTON_START, T64H_RAW_START},
    {SDL_GAMEPAD_BUTTON_BACK, T64H_RAW_BACK},
    {SDL_GAMEPAD_BUTTON_GUIDE, T64H_RAW_GUIDE},
    {SDL_GAMEPAD_BUTTON_LEFT_STICK, T64H_RAW_LEFT_STICK},
    {SDL_GAMEPAD_BUTTON_RIGHT_STICK, T64H_RAW_RIGHT_STICK},
    {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, T64H_RAW_LEFT_SHOULDER},
    {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, T64H_RAW_RIGHT_SHOULDER},
    {SDL_GAMEPAD_BUTTON_DPAD_UP, T64H_RAW_DPAD_UP},
    {SDL_GAMEPAD_BUTTON_DPAD_DOWN, T64H_RAW_DPAD_DOWN},
    {SDL_GAMEPAD_BUTTON_DPAD_LEFT, T64H_RAW_DPAD_LEFT},
    {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, T64H_RAW_DPAD_RIGHT},
};

t64h_pad t64h_sdl_sample_pad(t64h_sdl_shell *shell) {
  t64h_raw_pad raw;
  size_t i;

  (void)memset(&raw, 0, sizeof raw);
  SDL_UpdateGamepads();
  if (shell->pad != NULL && SDL_GamepadConnected(shell->pad)) {
    raw.present = 1;
    for (i = 0u; i < sizeof pad_buttons / sizeof pad_buttons[0]; i++) {
      if (SDL_GetGamepadButton(shell->pad, pad_buttons[i].button)) {
        raw.buttons |= pad_buttons[i].raw;
      }
    }
    raw.left_x = SDL_GetGamepadAxis(shell->pad, SDL_GAMEPAD_AXIS_LEFTX);
    raw.left_y = SDL_GetGamepadAxis(shell->pad, SDL_GAMEPAD_AXIS_LEFTY);
    raw.right_x = SDL_GetGamepadAxis(shell->pad, SDL_GAMEPAD_AXIS_RIGHTX);
    raw.right_y = SDL_GetGamepadAxis(shell->pad, SDL_GAMEPAD_AXIS_RIGHTY);
    raw.left_trigger = SDL_GetGamepadAxis(shell->pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    raw.right_trigger = SDL_GetGamepadAxis(shell->pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
  }
  return t64h_map_pad(&raw);
}

const uint8_t *t64h_sdl_frame_pixels(const t64h_sdl_shell *shell) {
  return shell->buffers + PIXELS_OFFSET;
}

/* ---- open, turn, close ------------------------------------------------------------------- */

static int open_fail(t64h_sdl_shell *shell, const char *text) {
  (void)fprintf(stderr, "t64h_sdl: %s\n", text);
  t64h_sdl_close(shell);
  return 1;
}

int t64h_sdl_open(t64h_sdl_shell *shell, const char *rom_path, int scale) {
  const t64_profile_descriptor *profile = NULL;
  size_t rom_length = 0u;
  t64h_config config;
  SDL_JoystickID *ids;
  int count = 0;

  (void)memset(shell, 0, sizeof *shell);
  if (scale < 1 || scale > 8) {
    return open_fail(shell, "the scale must be 1 to 8");
  }
  shell->scale = scale;
  shell->rom = read_rom(rom_path, &rom_length);
  shell->buffers = (uint8_t *)calloc(1u, 2u * T64H_VIDEO_BUFFER_BYTES + T64H_AUDIO_BUFFER_BYTES +
                                             T64H_TEXT_BUFFER_BYTES);
  if (shell->rom == NULL || shell->buffers == NULL) {
    return open_fail(shell, "cannot read the ROM file");
  }
  shell->allocator = (t64_host_allocator)T64_HOST_ALLOCATOR_INIT(shell_allocate, shell_release, NULL);
  if (t64_profile_lookup(PROFILE_ID, &profile) != T64_STATUS_OK ||
      t64_machine_create(&(t64_machine_create_info)T64_MACHINE_CREATE_INFO_INIT(&shell->allocator,
                                                                               profile),
                         &shell->machine) != T64_STATUS_OK) {
    shell->machine = NULL;
    return open_fail(shell, "cannot create the machine");
  }
  if (t64_machine_admit_rom(shell->machine, shell->rom, rom_length) != T64_STATUS_OK) {
    return open_fail(shell, "the machine refused the ROM");
  }
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS)) {
    return open_fail(shell, SDL_GetError());
  }
  shell->window = SDL_CreateWindow(T64H_SDL_TITLE, T64H_SDL_SCREEN_WIDTH * scale,
                                   T64H_SDL_SCREEN_HEIGHT * scale, 0);
  if (shell->window == NULL) {
    return open_fail(shell, SDL_GetError());
  }
  ids = SDL_GetGamepads(&count);
  if (ids != NULL && count > 0) {
    shell->pad = SDL_OpenGamepad(ids[0]);
  }
  SDL_free(ids);
  (void)memset(&config, 0, sizeof config);
  config.machine = shell->machine;
  config.clock = shell_clock;
  config.sinks.context = shell;
  config.sinks.video = shell_video;
  config.sinks.audio = shell_audio;
  config.input = shell_input;
  config.input_context = shell;
  config.video_buffer = shell->buffers;
  config.video_capacity = T64H_VIDEO_BUFFER_BYTES;
  config.audio_buffer = shell->buffers + T64H_VIDEO_BUFFER_BYTES;
  config.audio_capacity = T64H_AUDIO_BUFFER_BYTES;
  config.text_buffer = shell->buffers + T64H_VIDEO_BUFFER_BYTES + T64H_AUDIO_BUFFER_BYTES;
  config.text_capacity = T64H_TEXT_BUFFER_BYTES;
  if (t64h_pump_init(&shell->pump, &config) != T64H_OK) {
    return open_fail(shell, "cannot start the pump");
  }
  return 0;
}

int t64h_sdl_turn(t64h_sdl_shell *shell) {
  SDL_Event event;
  t64h_step_result result;
  const uint64_t before = shell->pump.tick;
  int queued;

  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
      shell->quit = 1;
    }
  }
  shell->pad_now = t64h_sdl_sample_pad(shell);
  shell->status = t64h_pump_step(&shell->pump, STEP_TICKS, &result);
  shell->stop_reason = result.stop_reason;
  if (shell->dirty) {
    (void)SDL_UpdateWindowSurface(shell->window);
    shell->dirty = 0;
  }
  if (shell->audio != NULL) {
    queued = SDL_GetAudioStreamQueued(shell->audio);
    if (queued > 0) {
      shell->audio_ready = 1;
    } else if (shell->audio_ready) {
      shell->audio_ready = 0;
      shell->underruns++;
    }
  }
  if (shell->status == T64H_TERMINAL) {
    return 1;
  }
  if (shell->status != T64H_OK) {
    (void)fprintf(stderr, "t64h_sdl: the pump ended the run: %s\n", t64h_status_name(shell->status));
    return -1;
  }
  if (result.tick - before < STEP_TICKS) {
    SDL_Delay(1);  /* caught up with the clock: do not spin */
  }
  return shell->quit;
}

void t64h_sdl_close(t64h_sdl_shell *shell) {
  if (shell->audio != NULL) {
    SDL_DestroyAudioStream(shell->audio);
  }
  if (shell->pad != NULL) {
    SDL_CloseGamepad(shell->pad);
  }
  if (shell->window != NULL) {
    SDL_DestroyWindow(shell->window);
  }
  if (shell->machine != NULL) {
    t64_machine_destroy(shell->machine);
  }
  free(shell->rom);
  free(shell->buffers);
  SDL_Quit();
  (void)memset(shell, 0, sizeof *shell);
}
