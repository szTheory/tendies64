# Repository checks that are plain scripts: registered as CTest tests with label
# "repo" so every gate run executes them. No targets are defined here.
# Picked up by the tools glob in the root CMakeLists.txt.
add_test(NAME repo.docs
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-docs.sh
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME repo.docs-selftest
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-docs.sh --selftest
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME repo.ci-yml
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-ci-yml.sh
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME repo.ci-yml-selftest
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-ci-yml.sh --selftest
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(repo.docs repo.docs-selftest repo.ci-yml repo.ci-yml-selftest
  PROPERTIES LABELS repo)
