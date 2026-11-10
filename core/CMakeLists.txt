# Production core modules are top-level t64_*.c files. Tests stay separate so
# host-only test code never enters the deterministic production library.
file(GLOB T64_CORE_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/t64_*.c")
file(GLOB T64_CORE_HEADERS CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/include/tendies64/*.h")
file(GLOB T64_CORE_INTERNAL_HEADERS CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/t64_*.h")

# Build identity is a digest of the actual production inputs and build policy.
# Only hashes and source-relative names enter the digest; no path or timestamp
# is emitted. Configure dependencies refresh it whenever a production input
# changes, including edits between builds.
set(T64_BUILD_INPUTS
  ${T64_CORE_SOURCES}
  ${T64_CORE_HEADERS}
  ${T64_CORE_INTERNAL_HEADERS}
  "${CMAKE_SOURCE_DIR}/CMakeLists.txt"
  "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt"
  "${CMAKE_SOURCE_DIR}/CMakePresets.json"
  "${CMAKE_SOURCE_DIR}/tools/common/CMakeLists.txt"
  "${CMAKE_SOURCE_DIR}/tools/common/t64_sha256.c"
  "${CMAKE_SOURCE_DIR}/tools/common/t64_sha256.h")
list(REMOVE_DUPLICATES T64_BUILD_INPUTS)
list(SORT T64_BUILD_INPUTS)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  ${T64_BUILD_INPUTS})

if(DEFINED CMAKE_C_COMPILER_TARGET)
  set(T64_COMPILER_TARGET "${CMAKE_C_COMPILER_TARGET}")
else()
  set(T64_COMPILER_TARGET "")
endif()
string(CONCAT T64_BUILD_MANIFEST
  "format=t64-build-v1\n"
  "project=${PROJECT_VERSION}\n"
  "cmake=${CMAKE_VERSION}\n"
  "compiler=${CMAKE_C_COMPILER_ID}:${CMAKE_C_COMPILER_VERSION}\n"
  "compiler_target=${T64_COMPILER_TARGET}\n"
  "platform=${CMAKE_SYSTEM_NAME}:${CMAKE_SYSTEM_PROCESSOR}\n"
  "build_type=${CMAKE_BUILD_TYPE}\n"
  "c_standard=${CMAKE_C_STANDARD}:${CMAKE_C_STANDARD_REQUIRED}:${CMAKE_C_EXTENSIONS}\n"
  "warnings_as_errors=${T64_WERROR}\n"
  "c_flags=${CMAKE_C_FLAGS}\n"
  "debug_flags=${CMAKE_C_FLAGS_DEBUG}\n"
  "release_flags=${CMAKE_C_FLAGS_RELEASE}\n"
  "relwithdebinfo_flags=${CMAKE_C_FLAGS_RELWITHDEBINFO}\n"
  "minsizerel_flags=${CMAKE_C_FLAGS_MINSIZEREL}\n")
foreach(T64_BUILD_INPUT IN LISTS T64_BUILD_INPUTS)
  file(SHA256 "${T64_BUILD_INPUT}" T64_INPUT_SHA256)
  file(RELATIVE_PATH T64_INPUT_RELATIVE "${CMAKE_SOURCE_DIR}"
    "${T64_BUILD_INPUT}")
  string(APPEND T64_BUILD_MANIFEST
    "${T64_INPUT_RELATIVE}=${T64_INPUT_SHA256}\n")
endforeach()
string(SHA256 T64_BUILD_FINGERPRINT "${T64_BUILD_MANIFEST}")
set(T64_BUILD_ID "t64-build-v1-${T64_BUILD_FINGERPRINT}")

execute_process(
  COMMAND git -C "${CMAKE_SOURCE_DIR}" rev-parse --verify HEAD
  RESULT_VARIABLE T64_GIT_REVISION_STATUS
  OUTPUT_VARIABLE T64_SOURCE_REVISION
  ERROR_QUIET
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT T64_GIT_REVISION_STATUS EQUAL 0 OR T64_SOURCE_REVISION STREQUAL "")
  set(T64_SOURCE_REVISION "unrecorded")
endif()
execute_process(
  COMMAND git -C "${CMAKE_SOURCE_DIR}" rev-parse --absolute-git-dir
  RESULT_VARIABLE T64_GIT_DIR_STATUS
  OUTPUT_VARIABLE T64_GIT_DIR
  ERROR_QUIET
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if(T64_GIT_DIR_STATUS EQUAL 0 AND NOT T64_GIT_DIR STREQUAL "")
  set(T64_REVISION_DEPENDENCIES
    "${T64_GIT_DIR}/HEAD"
    "${T64_GIT_DIR}/packed-refs")
  execute_process(
    COMMAND git -C "${CMAKE_SOURCE_DIR}" symbolic-ref --quiet HEAD
    RESULT_VARIABLE T64_SYMBOLIC_HEAD_STATUS
    OUTPUT_VARIABLE T64_SYMBOLIC_HEAD
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(T64_SYMBOLIC_HEAD_STATUS EQUAL 0 AND
     NOT T64_SYMBOLIC_HEAD STREQUAL "")
    list(APPEND T64_REVISION_DEPENDENCIES
      "${T64_GIT_DIR}/${T64_SYMBOLIC_HEAD}")
  endif()
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${T64_REVISION_DEPENDENCIES})
endif()

add_library(t64_core STATIC ${T64_CORE_SOURCES})
target_include_directories(t64_core
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${CMAKE_SOURCE_DIR}/tools/common")
t64_target_warnings(t64_core)
# Core-only flags: no implicit float promotion and bounded stack usage.
target_compile_options(t64_core PRIVATE -Wdouble-promotion
  -Wframe-larger-than=2048)
target_link_libraries(t64_core PRIVATE t64_sha256)
target_compile_definitions(t64_core PRIVATE
  "T64_BUILD_ID=\"${T64_BUILD_ID}\""
  "T64_SOURCE_REVISION=\"${T64_SOURCE_REVISION}\"")

add_executable(test_tracer tests/test_tracer.c)
target_include_directories(test_tracer PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_tracer PRIVATE t64_core)
t64_target_warnings(test_tracer)
add_test(NAME unit.core.tracer COMMAND test_tracer)
set_tests_properties(unit.core.tracer PROPERTIES LABELS unit)

add_executable(test_profile tests/test_profile.c)
target_include_directories(test_profile PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_profile PRIVATE t64_core)
t64_target_warnings(test_profile)
add_test(NAME unit.core.profile COMMAND test_profile)
set_tests_properties(unit.core.profile PROPERTIES LABELS unit)

add_executable(test_rom_admission tests/test_rom_admission.c)
target_include_directories(test_rom_admission PRIVATE
  "${CMAKE_SOURCE_DIR}/tests"
  "${CMAKE_SOURCE_DIR}/tools/common")
target_link_libraries(test_rom_admission PRIVATE t64_core t64_sha256)
t64_target_warnings(test_rom_admission)
add_test(NAME unit.core.rom-admission COMMAND test_rom_admission)
set_tests_properties(unit.core.rom-admission PROPERTIES LABELS unit)

add_executable(test_lifecycle tests/test_lifecycle.c)
target_include_directories(test_lifecycle PRIVATE
  "${CMAKE_SOURCE_DIR}/tests"
  "${CMAKE_SOURCE_DIR}/tools/common")
target_link_libraries(test_lifecycle PRIVATE t64_core t64_sha256)
t64_target_warnings(test_lifecycle)
add_test(NAME unit.core.lifecycle COMMAND test_lifecycle)
set_tests_properties(unit.core.lifecycle PROPERTIES LABELS unit)
