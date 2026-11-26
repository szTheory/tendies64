# Independent consumer of the installed tendies64 package (Phase 11, plan
# 11-16, VER-05). A separate top-level project: it is never add_subdirectory'd
# by the main build, it names no path of the source tree, and it sees the
# library only through find_package and the public header of the prefix.
# scripts/check-install.sh configures it with CMAKE_PREFIX_PATH pointing at a
# prefix that was moved after install and whose path contains spaces.
cmake_minimum_required(VERSION 3.25)
project(t64_installed_consumer LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# The version the consumer requests. Only the version-mismatch negative controls
# of controls.tsv (plan 11-19) change it, to show that a package older or newer
# than the request is refused by the package's own version file.
set(T64C_REQUIRED_VERSION 0.1 CACHE STRING "tendies64 version the consumer requests")
find_package(tendies64 ${T64C_REQUIRED_VERSION} REQUIRED CONFIG)

set(T64C_SOURCES main.c smoke.c composite.c hostloop.c sequence.c threads.c ops.c t64c_sha256.c)
add_executable(t64c ${T64C_SOURCES})
target_link_libraries(t64c PRIVATE tendies64::core)
# The smoke case alone, built only on request (the negative controls of controls.tsv
# build this target against a doctored or relocated copy of the prefix).
add_executable(t64c_smoke EXCLUDE_FROM_ALL smoke_main.c smoke.c)
target_link_libraries(t64c_smoke PRIVATE tendies64::core)
# The symbol-collision variant (plan 11-19): the same cases plus collision.c, which
# defines the host's own f32_add and t64_sha256_init with wrong results and call
# counters. Built only with -DT64C_COLLISION=ON (the symbol-collision control).
option(T64C_COLLISION "Also build t64c_collision, the consumer with colliding host symbols" OFF)
set(t64c_targets t64c t64c_smoke)
if(T64C_COLLISION)
  add_executable(t64c_collision ${T64C_SOURCES} collision.c)
  target_link_libraries(t64c_collision PRIVATE tendies64::core)
  target_compile_definitions(t64c_collision PRIVATE T64C_COLLISION=1)
  list(APPEND t64c_targets t64c_collision)
endif()
# The two-thread case (plan 11-20, a local lane): threads.c is an empty translation
# unit unless this option is on. The consumer is host code and may link pthreads; the
# library imports none. scripts/check-install.sh tsan turns it on together with
# -fsanitize=thread; the gate never does, so `threads` is not a registered case.
option(T64C_THREADS "Build the two-thread case (a local ThreadSanitizer lane)" OFF)
option(T64C_THREADS_SEED_RACE "With T64C_THREADS: two threads share one instance (the seeded race)" OFF)
if(T64C_THREADS)
  find_package(Threads REQUIRED)
  target_compile_definitions(t64c PRIVATE T64C_THREADS=1)
  target_link_libraries(t64c PRIVATE Threads::Threads)
  if(T64C_THREADS_SEED_RACE)
    target_compile_definitions(t64c PRIVATE T64C_THREADS_SEED_RACE=1)
  endif()
endif()
foreach(t64c_target IN LISTS t64c_targets)
  target_compile_options(${t64c_target} PRIVATE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-prototypes
    -Wmissing-prototypes -Wundef -Wvla -Wcast-qual -Wformat=2 -Wwrite-strings
    -Werror)
  # The build identity the installed package recorded, compared at run time with
  # t64_build_identity() of the linked archive.
  target_compile_definitions(${t64c_target} PRIVATE
    "T64C_EXPECTED_BUILD_ID=\"${tendies64_BUILD_ID}\"")
endforeach()
