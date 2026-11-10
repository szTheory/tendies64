# GOV-04 claims gate: tests only, no targets (the checker is POSIX sh + awk,
# CONTEXT A-5). Picked up by the tools glob in the root CMakeLists.txt.
# Every test runs from the source directory and finds the C evidence tool
# (t64-result-check) through T64_BUILD_DIR.
add_test(NAME gov.claims.selftest
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-claims.sh --selftest
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_test(NAME gov.claims.repo
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-claims.sh
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

set_tests_properties(gov.claims.selftest gov.claims.repo PROPERTIES
  LABELS gov
  ENVIRONMENT "T64_BUILD_DIR=${CMAKE_BINARY_DIR}")
