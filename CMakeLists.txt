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

add_subdirectory(tools/common)
add_subdirectory(core)

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
