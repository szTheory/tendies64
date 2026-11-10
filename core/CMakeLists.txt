# Production core modules are top-level t64_*.c files. Tests stay separate so
# host-only test code never enters the deterministic production library.
file(GLOB T64_CORE_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/t64_*.c")

add_library(t64_core STATIC ${T64_CORE_SOURCES})
target_include_directories(t64_core
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${CMAKE_SOURCE_DIR}/tools/common")
t64_target_warnings(t64_core)
# Core-only flags: no implicit float promotion and bounded stack usage.
target_compile_options(t64_core PRIVATE -Wdouble-promotion
  -Wframe-larger-than=2048)
target_link_libraries(t64_core PRIVATE t64_sha256)

add_executable(test_tracer tests/test_tracer.c)
target_include_directories(test_tracer PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_tracer PRIVATE t64_core)
t64_target_warnings(test_tracer)
add_test(NAME unit.core.tracer COMMAND test_tracer)
set_tests_properties(unit.core.tracer PROPERTIES LABELS unit)
