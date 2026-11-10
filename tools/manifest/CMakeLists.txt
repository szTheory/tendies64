# GOV-03 fixture manifest checker and its CTest tests (label gov). Picked up by
# the tools glob in the root CMakeLists.txt, so the root file is not edited.
# Every test runs from the source directory, where fixtures/ lives.
set(T64_MANIFEST_DEFS _POSIX_C_SOURCE=200809L)
if(APPLE)
  # _POSIX_C_SOURCE alone hides O_NOFOLLOW on macOS (measured in plan 01-02).
  list(APPEND T64_MANIFEST_DEFS _DARWIN_C_SOURCE)
endif()

add_executable(t64-manifest-check t64_manifest_check.c t64_schema.c)
target_compile_definitions(t64-manifest-check PRIVATE ${T64_MANIFEST_DEFS})
target_link_libraries(t64-manifest-check PRIVATE t64_common)
t64_target_warnings(t64-manifest-check)

set(T64_MANIFEST_TOOL "$<TARGET_FILE:t64-manifest-check> --schema fixtures/manifest.schema.json")

# Seeded manifests (directory-owned corpus, CONTEXT A-6): each bad_*.json must
# fail with its rule id; good*.json must pass.
add_test(NAME gov.manifest.seeds
  COMMAND sh ${CMAKE_SOURCE_DIR}/tests/seeded/run-expect.sh
    --tool "${T64_MANIFEST_TOOL}"
    --dir ${CMAKE_SOURCE_DIR}/tests/seeded/manifest --ext .json
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# One seed per schema-engine rule that the entry seeds do not reach on their own.
add_test(NAME gov.manifest.schema-seeds
  COMMAND sh ${CMAKE_SOURCE_DIR}/tests/seeded/run-expect.sh
    --tool "${T64_MANIFEST_TOOL}"
    --dir ${CMAKE_SOURCE_DIR}/tests/seeded/manifest-schema --ext .json
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# The real manifest. scripts/gate.sh exports T64_REQUIRE_FIXTURES=1 under CI=true
# and 0 otherwise; this is the single place it becomes --require-fixtures. The
# argument list is printed so the CTest log proves which mode ran. CTest also
# echoes this command line into the log, so the label is assembled at run time
# ($n below): the log then holds exactly one "manifest-check argv:" line, the
# one printed by the test, and never one made of the script text.
add_test(NAME gov.manifest.repo
  COMMAND sh -c [=[n=manifest-check; a="--schema fixtures/manifest.schema.json"; if [ "${T64_REQUIRE_FIXTURES:-0}" = 1 ]; then a="$a --require-fixtures"; fi; echo "$n argv: $a fixtures/manifest.json"; exec "$0" $a fixtures/manifest.json]=]
    $<TARGET_FILE:t64-manifest-check>
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# --require-fixtures proven in both modes, plus --emit-allowlist and the
# schema-subset refusal, on a temporary root.
add_test(NAME gov.manifest.require-fixtures
  COMMAND sh ${CMAKE_SOURCE_DIR}/tests/seeded/run-require-fixtures.sh
    --bin $<TARGET_FILE:t64-manifest-check>
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

set_tests_properties(gov.manifest.seeds gov.manifest.schema-seeds
  gov.manifest.repo gov.manifest.require-fixtures PROPERTIES
  LABELS gov
  ENVIRONMENT "T64_BUILD_DIR=${CMAKE_BINARY_DIR}")
