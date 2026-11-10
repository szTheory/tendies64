# Shared host-side library for the tools and the runner.
add_library(t64_common STATIC
  t64_sha256.c
  t64_utf8.c
  t64_jsonw.c)
target_include_directories(t64_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
t64_target_warnings(t64_common)

add_executable(test_common tests/test_common.c)
target_link_libraries(test_common PRIVATE t64_common)
# Tests include the shared header tests/t64_test.h by name.
target_include_directories(test_common PRIVATE ${CMAKE_SOURCE_DIR}/tests)
t64_target_warnings(test_common)

add_test(NAME unit.common COMMAND test_common)
set_tests_properties(unit.common PROPERTIES LABELS unit)
