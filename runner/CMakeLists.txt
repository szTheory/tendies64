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
