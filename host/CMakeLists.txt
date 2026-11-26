# The minimal host (Phase 12, plan 12-07, API-05, decision DR-10). A separate
# top-level project, like tests/installed-consumer: the root build never adds it
# (the root CMakeLists.txt holds no host/ path and the gate's build does not build
# it), it sees the library only through find_package and the public header of an
# installed prefix, and scripts/check-host.sh configures it with
# CMAKE_PREFIX_PATH pointing at the moved prefix of the install check. It has no
# install rule.
#
#   host/pump    the pure-C pump: only <tendies64/t64_machine.h> and C99 headers
#   host/tests   the fake-clock unit cases (t64h_tests) and their SHA-256
#   host/sdl     the optional SDL3 shell, only with -DT64H_SDL=ON (plan 12-09)
cmake_minimum_required(VERSION 3.25)
project(t64h LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# The package version the host requests. The final bind of Phase 12 moves it
# together with the package (PIN-IMPACT B-33).
find_package(tendies64 0.1 CONFIG REQUIRED)

option(T64H_SDL "Build the optional SDL3 shell" OFF)

add_library(t64h_pump STATIC pump/t64h_pump.c pump/t64h_script.c pump/t64h_input.c pump/t64h_frames.c)
target_include_directories(t64h_pump PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/pump")
target_link_libraries(t64h_pump PUBLIC tendies64::core)

add_executable(t64h_tests tests/t64h_tests.c tests/t64h_sha256.c)
target_link_libraries(t64h_tests PRIVATE t64h_pump)

foreach(t64h_target t64h_pump t64h_tests)
  target_compile_options(${t64h_target} PRIVATE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-prototypes
    -Wmissing-prototypes -Wundef -Wvla -Wcast-qual -Wformat=2 -Wwrite-strings
    -Werror)
endforeach()

if(T64H_SDL)
  # The optional SDL3 shell (plan 12-09, decisions DR-17 and DR-18): link-only against a system
  # SDL3 (never vendored, fetched or copied; docs/evidence/sdl3-host-dependency-audit.md), built
  # only by the local lane `sh scripts/check-host.sh sdl`, never by the gate, and without an
  # install rule.
  find_package(SDL3 3.2.0 CONFIG REQUIRED)

  # The shell's code as a static library for the executable and the smoke.
  add_library(t64h_sdl_shell STATIC sdl/t64h_sdl.c)
  target_include_directories(t64h_sdl_shell PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/sdl")
  target_link_libraries(t64h_sdl_shell PUBLIC t64h_pump SDL3::SDL3)

  # The shell itself: t64h_sdl --rom PATH [--scale N].
  add_executable(t64h_sdl sdl/t64h_sdl_main.c)
  target_link_libraries(t64h_sdl PRIVATE t64h_sdl_shell)

  # The headless self-check of the lane (dummy drivers and a virtual gamepad).
  add_executable(t64h_sdl_smoke sdl/t64h_sdl_smoke.c)
  target_link_libraries(t64h_sdl_smoke PRIVATE t64h_sdl_shell)

  foreach(t64h_target t64h_sdl_shell t64h_sdl t64h_sdl_smoke)
    target_compile_options(${t64h_target} PRIVATE
      -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-prototypes
      -Wmissing-prototypes -Wundef -Wvla -Wcast-qual -Wformat=2 -Wwrite-strings
      -Werror)
  endforeach()
endif()
