# Runner executables are host tools, not core: they may use POSIX. On macOS,
# _POSIX_C_SOURCE alone hides O_NOFOLLOW and mkdtemp (measured, Apple clang 21),
# so _DARWIN_C_SOURCE is added there; Linux exposes both under 200809L.
set(T64_HOST_DEFS _POSIX_C_SOURCE=200809L)
if(APPLE)
  list(APPEND T64_HOST_DEFS _DARWIN_C_SOURCE)
endif()
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

add_executable(t64-supervisor t64_supervisor.c)
target_compile_definitions(t64-supervisor PRIVATE
  ${T64_HOST_DEFS}
  T64_BUILD_ID=\"${PROJECT_VERSION}+synthetic\")
target_link_libraries(t64-supervisor PRIVATE t64_classify t64_common t64_core)
t64_target_warnings(t64-supervisor)

# Consumer of t64.result v1 records and t64.suite files (plan 01-07).
add_executable(t64-result-check t64_result_check.c)
target_compile_definitions(t64-result-check PRIVATE ${T64_HOST_DEFS})
target_link_libraries(t64-result-check PRIVATE t64_classify t64_common)
t64_target_warnings(t64-result-check)

# Native model observations use a distinct schema and never enter t64.result.
add_executable(t64-model-result-check t64_model_result_check.c)
target_compile_definitions(t64-model-result-check PRIVATE ${T64_HOST_DEFS})
target_link_libraries(t64-model-result-check PRIVATE t64_common)
t64_target_warnings(t64-model-result-check)

add_executable(test_skeleton_probe tests/test_skeleton_probe.c)
target_compile_definitions(test_skeleton_probe PRIVATE ${T64_HOST_DEFS})
target_include_directories(test_skeleton_probe PRIVATE ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(test_skeleton_probe PRIVATE t64_common t64_core)
t64_target_warnings(test_skeleton_probe)
add_test(NAME unit.core.profile-result
  COMMAND test_skeleton_probe
    ${CMAKE_CURRENT_BINARY_DIR}/t64-skeleton-probe
    $<TARGET_FILE:t64-model-result-check>)
set_tests_properties(unit.core.profile-result PROPERTIES LABELS unit)

add_executable(test_model_result tests/test_model_result.c)
target_compile_definitions(test_model_result PRIVATE ${T64_HOST_DEFS})
t64_target_warnings(test_model_result)
add_test(NAME unit.model-result
  COMMAND test_model_result $<TARGET_FILE:t64-model-result-check>
    ${CMAKE_SOURCE_DIR}/tests/controls/golden/positive.json)
set_tests_properties(unit.model-result PROPERTIES LABELS unit)

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
