/*
 * The executable of the optional SDL3 shell (Phase 12, plan 12-09, API-05, decision DR-17):
 *   t64h_sdl --rom PATH [--scale N]      N is 1 to 8, the default is 4
 * It plays the title ROM in a window until the window is closed or the guest ends (the title ends
 * when the controller presses Start) and prints one summary line. Controller only: keyboard events
 * are never read. See t64h_sdl.h and scripts/check-host.sh (the sdl lane).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "t64h_sdl.h"

int main(int argc, char **argv) {
  const char *rom = NULL;
  int scale = 4;
  int i;
  int turn = 0;
  t64h_sdl_shell shell;

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--rom") == 0 && i + 1 < argc) {
      rom = argv[++i];
    } else if (strcmp(argv[i], "--scale") == 0 && i + 1 < argc) {
      scale = atoi(argv[++i]);
    } else {
      rom = NULL;
      break;
    }
  }
  if (rom == NULL) {
    (void)fprintf(stderr, "usage: t64h_sdl --rom PATH [--scale N]  (N is 1 to 8, default 4)\n");
    return 2;
  }
  if (t64h_sdl_open(&shell, rom, scale) != 0) {
    return 1;
  }
  while (turn == 0) {
    turn = t64h_sdl_turn(&shell);
  }
  (void)printf("t64h_sdl: frames %llu, audio packets %llu, underruns %llu, ended by %s\n",
               (unsigned long long)shell.frames, (unsigned long long)shell.audio_packets,
               (unsigned long long)shell.underruns,
               shell.status == T64H_TERMINAL ? "the end of the guest"
               : shell.quit                  ? "the window"
                                             : "a pump error");
  t64h_sdl_close(&shell);
  return turn < 0 ? 1 : 0;
}
