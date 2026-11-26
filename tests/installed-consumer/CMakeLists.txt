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

find_package(tendies64 0.1 REQUIRED CONFIG)

add_executable(t64c main.c composite.c ops.c t64c_sha256.c)
target_link_libraries(t64c PRIVATE tendies64::core)
target_compile_options(t64c PRIVATE
  -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-prototypes
  -Wmissing-prototypes -Wundef -Wvla -Wcast-qual -Wformat=2 -Wwrite-strings
  -Werror)
# The build identity the installed package recorded, compared at run time with
# t64_build_identity() of the linked archive.
target_compile_definitions(t64c PRIVATE
  "T64C_EXPECTED_BUILD_ID=\"${tendies64_BUILD_ID}\"")
