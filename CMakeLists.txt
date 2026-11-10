cmake_minimum_required(VERSION 3.25)
project(tendies64 VERSION 0.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Warnings are errors by default. Turning this off is a reviewed one-line change
# (a compiler-drift fix), never a gate bypass (RESEARCH Pitfall 11).
option(T64_WERROR "Treat compiler warnings as errors" ON)

enable_testing()

# Curated warning list, present in gcc 13, clang 18 and Apple clang 21.
function(t64_target_warnings tgt)
  target_compile_options(${tgt} PRIVATE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-prototypes
    -Wmissing-prototypes -Wundef -Wvla -Wcast-qual -Wformat=2 -Wwrite-strings)
  if(T64_WERROR)
    target_compile_options(${tgt} PRIVATE -Werror)
  endif()
endfunction()

# CONTEXT A-1: until ADR-0002 is accepted, core/ holds exactly
# core/t64_core_stub.c and core/include/tendies64/t64_facade.h, so the library
# is declared here and there is no core/CMakeLists.txt.
# Never define _POSIX_C_SOURCE for t64_core (determinism boundary).
add_library(t64_core STATIC core/t64_core_stub.c)
target_include_directories(t64_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/core/include)
t64_target_warnings(t64_core)
# Core-only flags (RESEARCH Pitfall 3): no implicit float promotion, and stack
# frames small enough to stay below Apple's ___chkstk_darwin threshold.
target_compile_options(t64_core PRIVATE -Wdouble-promotion -Wframe-larger-than=2048)

add_subdirectory(tools/common)

# Later plans add tools by creating tools/<name>/CMakeLists.txt; no edit here.
file(GLOB T64_TOOL_CMAKELISTS CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/tools/*/CMakeLists.txt)
foreach(t64_tool_file IN LISTS T64_TOOL_CMAKELISTS)
  get_filename_component(t64_tool_dir "${t64_tool_file}" DIRECTORY)
  get_filename_component(t64_tool_name "${t64_tool_dir}" NAME)
  if(NOT t64_tool_name STREQUAL "common")
    add_subdirectory(tools/${t64_tool_name})
  endif()
endforeach()

add_subdirectory(runner)
add_subdirectory(tests/controls)
