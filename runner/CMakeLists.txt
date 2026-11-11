# Runner executables are host tools, not core: they may use POSIX. On macOS,
# _POSIX_C_SOURCE alone hides O_NOFOLLOW and mkdtemp (measured, Apple clang 21),
# so _DARWIN_C_SOURCE is added there; Linux exposes both under 200809L.
set(T64_HOST_DEFS _POSIX_C_SOURCE=200809L)
if(APPLE)
  list(APPEND T64_HOST_DEFS _DARWIN_C_SOURCE)
endif()

# Host-side closed logical-input schedule parser.
add_library(t64_input STATIC t64_input.c)
target_include_directories(t64_input PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(t64_input PUBLIC t64_common)
t64_target_warnings(t64_input)

add_executable(test_input tests/test_input.c)
target_compile_definitions(test_input PRIVATE ${T64_HOST_DEFS})
target_link_libraries(test_input PRIVATE t64_input t64_core)
t64_target_warnings(test_input)
add_dependencies(test_input t64-boot-fixtures)
add_test(NAME controls.real.input-partition COMMAND test_input)
set_tests_properties(controls.real.input-partition PROPERTIES
  LABELS controls
  TIMEOUT 120
  ENVIRONMENT
    "T64_TEST_IPL3_PATH=${CMAKE_SOURCE_DIR}/build/fixtures/ipl3_compat.z64;T64_TEST_HELLO_PATH=${CMAKE_SOURCE_DIR}/build/fixtures/hello.z64;T64_BOOT_FIXTURES_DIR=${CMAKE_SOURCE_DIR}/build/fixtures")

# The classifier is pure and needs no POSIX definitions.
add_library(t64_classify STATIC t64_classify.c)
target_include_directories(t64_classify PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(t64_classify PUBLIC t64_common)
t64_target_warnings(t64_classify)

add_executable(test_classify tests/test_classify.c)
target_link_libraries(test_classify PRIVATE t64_classify)
target_include_directories(test_classify PRIVATE ${CMAKE_SOURCE_DIR}/tests)
t64_target_warnings(test_classify)
add_test(NAME unit.classify COMMAND test_classify)
set_tests_properties(unit.classify PROPERTIES LABELS unit)

add_executable(t64-runner-synthetic t64_synthetic.c)
target_compile_definitions(t64-runner-synthetic PRIVATE ${T64_HOST_DEFS})
target_link_libraries(t64-runner-synthetic PRIVATE t64_common t64_core)
t64_target_warnings(t64-runner-synthetic)

add_library(t64_verified_input STATIC t64_verified_input.c)
target_compile_definitions(t64_verified_input PRIVATE ${T64_HOST_DEFS})
target_include_directories(t64_verified_input PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(t64_verified_input PRIVATE t64_common)
t64_target_warnings(t64_verified_input)

# Test-only fault injection stays out of the supervisor's production library.
add_library(t64_verified_input_test STATIC t64_verified_input.c)
target_compile_definitions(t64_verified_input_test PRIVATE
  ${T64_HOST_DEFS} T64_VERIFIED_INPUT_TESTING)
target_include_directories(t64_verified_input_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(t64_verified_input_test PRIVATE t64_common)
t64_target_warnings(t64_verified_input_test)

add_executable(t64-supervisor t64_supervisor.c)
target_compile_definitions(t64-supervisor PRIVATE
  ${T64_HOST_DEFS}
  T64_BUILD_ID=\"${PROJECT_VERSION}+synthetic\")
target_link_libraries(t64-supervisor PRIVATE
  t64_classify t64_common t64_core t64_verified_input)
t64_target_warnings(t64-supervisor)

# First headless real guest producer. It consumes verified inherited snapshots
# and emits a canonical t64.result v2 record.
add_executable(t64-runner-real t64_real.c t64_real_result.c)
target_compile_definitions(t64-runner-real PRIVATE ${T64_HOST_DEFS})
target_link_libraries(t64-runner-real PRIVATE
  t64_input t64_common t64_core)
t64_target_warnings(t64-runner-real)

add_executable(test_fixture_descriptor tests/test_fixture_descriptor.c)
target_compile_definitions(test_fixture_descriptor PRIVATE
  ${T64_HOST_DEFS} T64_VERIFIED_INPUT_TESTING)
target_link_libraries(test_fixture_descriptor PRIVATE
  t64_verified_input_test t64_input t64_core)
t64_target_warnings(test_fixture_descriptor)
add_dependencies(test_fixture_descriptor t64-supervisor t64-boot-fixtures)
add_test(NAME runner.fixture-descriptor COMMAND test_fixture_descriptor)
add_test(NAME controls.real.source-replaced
  COMMAND test_fixture_descriptor --replacement $<TARGET_FILE:t64-supervisor>)
set_tests_properties(runner.fixture-descriptor controls.real.source-replaced
  PROPERTIES LABELS controls TIMEOUT 120
  ENVIRONMENT
    "T64_TEST_HELLO_PATH=${CMAKE_SOURCE_DIR}/build/fixtures/hello.z64;T64_TEST_INPUT_PATH=${CMAKE_SOURCE_DIR}/build/fixtures/hello.input.json")
set_tests_properties(controls.real.source-replaced PROPERTIES
  DEPENDS runner.fixture-descriptor)

# Consumer of t64.result v1 records and t64.suite files (plan 01-07).
add_executable(t64-result-check t64_result_check.c)
target_compile_definitions(t64-result-check PRIVATE ${T64_HOST_DEFS})
target_link_libraries(t64-result-check PRIVATE t64_classify t64_common)
t64_target_warnings(t64-result-check)

# Descriptor-attested actual IPL3/hello positive integration.
add_executable(test_real_runner tests/test_real_runner.c)
target_compile_definitions(test_real_runner PRIVATE ${T64_HOST_DEFS})
target_link_libraries(test_real_runner PRIVATE
  t64_verified_input t64_common)
add_dependencies(test_real_runner
  t64-runner-real t64-result-check t64-supervisor t64-boot-fixtures)
t64_target_warnings(test_real_runner)
add_test(NAME integration.open-ipl3-hello
  COMMAND test_real_runner
    $<TARGET_FILE:t64-runner-real>
    $<TARGET_FILE:t64-result-check>)
set_tests_properties(integration.open-ipl3-hello PROPERTIES
  LABELS integration TIMEOUT 120
  ENVIRONMENT
    "T64_TEST_HELLO_PATH=${CMAKE_SOURCE_DIR}/build/fixtures/hello.z64;T64_TEST_INPUT_PATH=${CMAKE_SOURCE_DIR}/build/fixtures/hello.input.json")

# Strict real t64.result v2 behavior, separate from v1 goldens and model output.
add_executable(test_result_v2 tests/test_result_v2.c)
target_compile_definitions(test_result_v2 PRIVATE ${T64_HOST_DEFS})
add_dependencies(test_result_v2 t64-result-check)
t64_target_warnings(test_result_v2)
add_test(NAME runner.result-v2
  COMMAND test_result_v2 $<TARGET_FILE:t64-result-check>
          ${CMAKE_SOURCE_DIR}/tests/seeded/results/good.json)
set_tests_properties(runner.result-v2 PROPERTIES LABELS runner TIMEOUT 60)

# Native model observations use a distinct schema and never enter t64.result.
add_executable(t64-model-result-check t64_model_result_check.c)
target_compile_definitions(t64-model-result-check PRIVATE ${T64_HOST_DEFS})
target_link_libraries(t64-model-result-check PRIVATE t64_common)
t64_target_warnings(t64-model-result-check)

add_executable(t64-skeleton-probe t64_skeleton_probe.c)
target_compile_definitions(t64-skeleton-probe PRIVATE ${T64_HOST_DEFS})
target_link_libraries(t64-skeleton-probe PRIVATE t64_common t64_core)
t64_target_warnings(t64-skeleton-probe)

add_executable(test_skeleton_probe tests/test_skeleton_probe.c)
target_compile_definitions(test_skeleton_probe PRIVATE ${T64_HOST_DEFS})
target_include_directories(test_skeleton_probe PRIVATE ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(test_skeleton_probe PRIVATE t64_common t64_core)
add_dependencies(test_skeleton_probe
  t64-skeleton-probe t64-model-result-check t64-result-check)
t64_target_warnings(test_skeleton_probe)
add_test(NAME unit.core.profile-result
  COMMAND test_skeleton_probe
    $<TARGET_FILE:t64-skeleton-probe>
    $<TARGET_FILE:t64-model-result-check>
    $<TARGET_FILE:t64-result-check>
    ${CMAKE_SOURCE_DIR}/tests/controls/golden/positive.json
    ${CMAKE_SOURCE_DIR}/tests/seeded/suites/good.jsonl
    ${CMAKE_SOURCE_DIR}/tests/seeded/suites/INVENTORY
    ${CMAKE_SOURCE_DIR}/scripts/check-claims.sh
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR})
set_tests_properties(unit.core.profile-result PROPERTIES LABELS unit
  TIMEOUT 60)

add_executable(test_model_result tests/test_model_result.c)
target_compile_definitions(test_model_result PRIVATE ${T64_HOST_DEFS})
add_dependencies(test_model_result t64-model-result-check)
t64_target_warnings(test_model_result)
add_test(NAME unit.model-result
  COMMAND test_model_result $<TARGET_FILE:t64-model-result-check>
    ${CMAKE_SOURCE_DIR}/tests/controls/golden/positive.json)
set_tests_properties(unit.model-result PROPERTIES LABELS unit TIMEOUT 60)

# Every committed supervisor golden must validate; zero goldens is a failure.
add_test(NAME unit.result-goldens
  COMMAND sh -c [=[
n=0
for f in "$1"/*.json; do
  [ -f "$f" ] || continue
  "$2" result "$f" || exit 1
  n=$((n + 1))
done
echo "goldens validated: $n"
[ "$n" -gt 0 ]
]=] sh ${CMAKE_SOURCE_DIR}/tests/controls/golden $<TARGET_FILE:t64-result-check>)
set_tests_properties(unit.result-goldens PROPERTIES LABELS unit)

# Single-result seeds (directory-owned corpus, driven by the shared driver).
add_test(NAME unit.result-seeds
  COMMAND sh ${CMAKE_SOURCE_DIR}/tests/seeded/run-expect.sh
    --tool "$<TARGET_FILE:t64-result-check> result"
    --dir ${CMAKE_SOURCE_DIR}/tests/seeded/results
    --ext .json)
set_tests_properties(unit.result-seeds PROPERTIES LABELS unit)

# Suite seeds: the pinned INVENTORY is a support file, not a seed.
add_test(NAME unit.suite-seeds
  COMMAND sh ${CMAKE_SOURCE_DIR}/tests/seeded/run-expect.sh
    --tool "$<TARGET_FILE:t64-result-check> suite --inventory ${CMAKE_SOURCE_DIR}/tests/seeded/suites/INVENTORY"
    --dir ${CMAKE_SOURCE_DIR}/tests/seeded/suites
    --ext .jsonl)
set_tests_properties(unit.suite-seeds PROPERTIES LABELS unit)
