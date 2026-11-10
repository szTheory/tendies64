# GOV-07 rights scan: the byte classifier and the CTest tests that run the
# drivers in every gate run. Picked up by the tools glob in the root file.
set(T64_RIGHTS_DEFS _POSIX_C_SOURCE=200809L)
if(APPLE)
  # _POSIX_C_SOURCE alone hides O_NOFOLLOW on macOS (measured in plan 01-02).
  list(APPEND T64_RIGHTS_DEFS _DARWIN_C_SOURCE)
endif()

add_executable(t64-blobscan t64_blobscan.c)
target_compile_definitions(t64-blobscan PRIVATE ${T64_RIGHTS_DEFS})
target_link_libraries(t64-blobscan PRIVATE t64_common)
t64_target_warnings(t64-blobscan)

# The drivers find the binary through T64_BUILD_DIR and run from the repo root.
add_test(NAME gov.rights.selftest
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/rights-scan.sh --selftest
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME gov.rights.repo
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/rights-scan.sh
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(gov.rights.selftest gov.rights.repo PROPERTIES
  LABELS gov
  ENVIRONMENT "T64_BUILD_DIR=${CMAKE_BINARY_DIR}")

# PII and private-path scan: tree, history diffs, messages and commit metadata.
# It runs in every gate run and before every push (AUTONOMY section 4).
add_test(NAME gov.pii.selftest
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/pii-scan.sh --selftest
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME gov.pii.repo
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/pii-scan.sh
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(gov.pii.selftest gov.pii.repo PROPERTIES LABELS gov)
