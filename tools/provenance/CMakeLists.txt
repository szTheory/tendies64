# GOV-02 per-copy provenance checker and its CTest tests (label gov). Picked up
# by the tools glob in the root CMakeLists.txt, so the root file is not edited.
# The drivers find the binary through T64_BUILD_DIR and run from the repo root.
set(T64_PROVENANCE_DEFS _POSIX_C_SOURCE=200809L)
if(APPLE)
  # _POSIX_C_SOURCE alone hides O_NOFOLLOW on macOS (measured in plan 01-02).
  list(APPEND T64_PROVENANCE_DEFS _DARWIN_C_SOURCE)
endif()

add_executable(t64-provenance-check t64_provenance_check.c)
target_compile_definitions(t64-provenance-check PRIVATE ${T64_PROVENANCE_DEFS})
target_link_libraries(t64-provenance-check PRIVATE t64_common)
t64_target_warnings(t64-provenance-check)

# The real records: every file git knows about, checked against
# provenance/copies.json in every gate run.
add_test(NAME gov.provenance.repo
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-provenance.sh
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# One temporary tree per rule id, each asserted by the exact set of rule ids
# printed; the checker is passed explicitly, not found through T64_BUILD_DIR.
add_test(NAME gov.provenance.selftest
  COMMAND sh ${CMAKE_SOURCE_DIR}/tests/seeded/provenance-selftest.sh
    --bin $<TARGET_FILE:t64-provenance-check>
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

set_tests_properties(gov.provenance.repo gov.provenance.selftest PROPERTIES
  LABELS gov
  ENVIRONMENT "T64_BUILD_DIR=${CMAKE_BINARY_DIR}")
