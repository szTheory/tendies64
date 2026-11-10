# Shared host-side library for the tools and the runner.
add_library(t64_common STATIC
  t64_sha256.c
  t64_utf8.c
  t64_json.c
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

# Canonical round-trip oracle for JSON files (parse, then emit canonically).
add_executable(t64-json-canon t64_json_canon.c)
target_link_libraries(t64-json-canon PRIVATE t64_common)
t64_target_warnings(t64-json-canon)

# Seeded cases: the shared driver decides which files in the directory are seeds.
add_test(NAME unit.json-seeds
  COMMAND sh ${CMAKE_SOURCE_DIR}/tests/seeded/run-expect.sh
    --tool "$<TARGET_FILE:t64-json-canon> --check"
    --dir ${CMAKE_SOURCE_DIR}/tests/seeded/json)
set_tests_properties(unit.json-seeds PROPERTIES LABELS unit)

# Strict JSON parser: independent negative corpus, then a deterministic mutation run.
add_executable(test_json tests/test_json.c)
target_link_libraries(test_json PRIVATE t64_common)
target_include_directories(test_json PRIVATE ${CMAKE_SOURCE_DIR}/tests)
t64_target_warnings(test_json)
add_test(NAME unit.json COMMAND test_json)

add_executable(test_json_fuzz tests/test_json_fuzz.c)
target_link_libraries(test_json_fuzz PRIVATE t64_common)
target_include_directories(test_json_fuzz PRIVATE ${CMAKE_SOURCE_DIR}/tests)
t64_target_warnings(test_json_fuzz)
add_test(NAME unit.json-fuzz COMMAND test_json_fuzz)
set_tests_properties(unit.json unit.json-fuzz PROPERTIES LABELS unit)
