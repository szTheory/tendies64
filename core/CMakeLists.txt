# Production core modules are top-level t64_*.c files. Tests stay separate so
# host-only test code never enters the deterministic production library.
if(CMAKE_CONFIGURATION_TYPES)
  message(FATAL_ERROR
    "Tendies64 currently requires a single-config CMake generator because its build identity includes CMAKE_BUILD_TYPE; configure with Ninja, Unix Makefiles, or another single-config generator.")
endif()

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

# The exact IPL3 input and authored hello variants are local, build-only
# fixtures. Keep their producer incremental and make every consuming test
# depend on the complete qualified output set. The producer invokes no CMake
# or Ninja targets, so it cannot recurse into this build.
set(T64_BOOT_FIXTURE_DIR "${CMAKE_SOURCE_DIR}/build/fixtures")
set(T64_BOOT_FIXTURE_OUTPUTS
  "${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64"
  "${T64_BOOT_FIXTURE_DIR}/ipl3_compat.payload.bin"
  "${T64_BOOT_FIXTURE_DIR}/hello.z64"
  "${T64_BOOT_FIXTURE_DIR}/hello-empty-break.z64"
  "${T64_BOOT_FIXTURE_DIR}/hello-wrong-text.z64"
  "${T64_BOOT_FIXTURE_DIR}/hello-empty-loop.z64"
  "${T64_BOOT_FIXTURE_DIR}/hello-incomplete-loop.z64"
  "${T64_BOOT_FIXTURE_DIR}/hello-unsupported.z64"
  "${T64_BOOT_FIXTURE_DIR}/hello-output-overflow.z64"
  "${T64_BOOT_FIXTURE_DIR}/hello.payload.bin"
  "${T64_BOOT_FIXTURE_DIR}/hello.expected.txt"
  "${T64_BOOT_FIXTURE_DIR}/hello.input.json"
  "${T64_BOOT_FIXTURE_DIR}/hello.build.json")
add_custom_command(
  OUTPUT ${T64_BOOT_FIXTURE_OUTPUTS}
  COMMAND "${CMAKE_COMMAND}" -E env "CC=${CMAKE_C_COMPILER}"
    sh "${CMAKE_SOURCE_DIR}/scripts/build-boot-fixtures.sh" --build
  DEPENDS
    "${CMAKE_SOURCE_DIR}/fixtures/boot-contract.json"
    "${CMAKE_SOURCE_DIR}/fixtures/manifest.json"
    "${CMAKE_SOURCE_DIR}/fixtures/hello/hello.S"
    "${CMAKE_SOURCE_DIR}/fixtures/hello/hello.ld"
    "${CMAKE_SOURCE_DIR}/scripts/build-boot-fixtures.sh"
    "${CMAKE_SOURCE_DIR}/tools/fixture/t64_rom_pack.c"
    "${CMAKE_SOURCE_DIR}/tools/common/t64_sha256.c"
    "${CMAKE_SOURCE_DIR}/tools/common/t64_sha256.h"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Building pinned boot and authored hello fixtures"
  VERBATIM)
add_custom_target(t64-boot-fixtures DEPENDS ${T64_BOOT_FIXTURE_OUTPUTS})

add_executable(test_tracer tests/test_tracer.c)
target_include_directories(test_tracer PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_tracer PRIVATE t64_core)
t64_target_warnings(test_tracer)
add_dependencies(test_tracer t64-boot-fixtures)
add_test(NAME unit.core.tracer COMMAND test_tracer)
set_tests_properties(unit.core.tracer PROPERTIES LABELS unit)

add_executable(test_boot_entry tests/test_boot_entry.c)
target_include_directories(test_boot_entry PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests"
  "${CMAKE_SOURCE_DIR}/tools/common")
target_link_libraries(test_boot_entry PRIVATE t64_core t64_sha256)
t64_target_warnings(test_boot_entry)
add_dependencies(test_boot_entry t64-boot-fixtures)
add_test(NAME unit.core.boot-entry COMMAND test_boot_entry)
set_tests_properties(unit.core.boot-entry PROPERTIES LABELS unit)
set_tests_properties(unit.core.tracer unit.core.boot-entry PROPERTIES
  ENVIRONMENT
    "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64;T64_BOOT_FIXTURES_DIR=${T64_BOOT_FIXTURE_DIR};T64_BOOT_CONTRACT_PATH=${CMAKE_SOURCE_DIR}/fixtures/boot-contract.json")

add_executable(test_boot_determinism tests/test_boot_determinism.c)
target_include_directories(test_boot_determinism PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests"
  "${CMAKE_SOURCE_DIR}/tools/common")
target_link_libraries(test_boot_determinism PRIVATE t64_core t64_sha256)
t64_target_warnings(test_boot_determinism)
add_dependencies(test_boot_determinism t64-boot-fixtures)
add_test(NAME integration.open-ipl3-determinism COMMAND test_boot_determinism)
set_tests_properties(integration.open-ipl3-determinism PROPERTIES
  LABELS integration TIMEOUT 180
  ENVIRONMENT
    "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64;T64_TEST_INPUT_PATH=${T64_BOOT_FIXTURE_DIR}/hello.input.json;T64_BOOT_FIXTURES_DIR=${T64_BOOT_FIXTURE_DIR}")

add_executable(test_isviewer tests/test_isviewer.c)
target_include_directories(test_isviewer PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tools/common")
target_link_libraries(test_isviewer PRIVATE t64_core)
t64_target_warnings(test_isviewer)
add_dependencies(test_isviewer t64-boot-fixtures)
add_test(NAME unit.core.isviewer COMMAND test_isviewer)
set_tests_properties(unit.core.isviewer PROPERTIES
  LABELS unit
  ENVIRONMENT
    "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64;T64_BOOT_FIXTURES_DIR=${T64_BOOT_FIXTURE_DIR}")

add_executable(test_cpu_path tests/test_cpu_path.c)
target_include_directories(test_cpu_path PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_cpu_path PRIVATE t64_core)
t64_target_warnings(test_cpu_path)
add_dependencies(test_cpu_path t64-boot-fixtures)
add_test(NAME unit.core.cpu-path COMMAND test_cpu_path --cpu-path)
set_tests_properties(unit.core.cpu-path PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")
add_test(NAME unit.core.cp0-cache COMMAND test_cpu_path --cp0-cache)
set_tests_properties(unit.core.cp0-cache PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")

add_executable(test_cpu_cp0 tests/test_cpu_cp0.c)
target_include_directories(test_cpu_cp0 PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_cpu_cp0 PRIVATE t64_core)
t64_target_warnings(test_cpu_cp0)
add_test(NAME unit.core.cpu-cp0 COMMAND test_cpu_cp0)
set_tests_properties(unit.core.cpu-cp0 PROPERTIES LABELS unit)

add_executable(test_cpu_time tests/test_cpu_time.c)
target_include_directories(test_cpu_time PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_cpu_time PRIVATE t64_core)
t64_target_warnings(test_cpu_time)
add_test(NAME unit.core.cpu-time COMMAND test_cpu_time)
set_tests_properties(unit.core.cpu-time PROPERTIES LABELS unit)

add_executable(test_cpu_exceptions tests/test_cpu_exceptions.c)
target_include_directories(test_cpu_exceptions PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_cpu_exceptions PRIVATE t64_core)
t64_target_warnings(test_cpu_exceptions)
add_dependencies(test_cpu_exceptions t64-boot-fixtures)
add_test(NAME unit.core.cpu-exceptions COMMAND test_cpu_exceptions)
set_tests_properties(unit.core.cpu-exceptions PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")

add_executable(test_cpu_integer tests/test_cpu_integer.c)
target_include_directories(test_cpu_integer PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_cpu_integer PRIVATE t64_core)
t64_target_warnings(test_cpu_integer)
add_dependencies(test_cpu_integer t64-boot-fixtures)
add_test(NAME unit.core.cpu-integer COMMAND test_cpu_integer --integer)
set_tests_properties(unit.core.cpu-integer PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")
add_test(NAME unit.core.cpu-control COMMAND test_cpu_integer --control)
set_tests_properties(unit.core.cpu-control PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")

add_executable(test_warm_devices tests/test_warm_devices.c)
target_include_directories(test_warm_devices PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_warm_devices PRIVATE t64_core)
t64_target_warnings(test_warm_devices)
add_dependencies(test_warm_devices t64-boot-fixtures)
add_test(NAME unit.core.warm-device-policy COMMAND test_warm_devices)
set_tests_properties(unit.core.warm-device-policy PROPERTIES
  LABELS unit
  ENVIRONMENT
    "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64;T64_TEST_HELLO_PATH=${T64_BOOT_FIXTURE_DIR}/hello.z64;T64_BOOT_FIXTURES_DIR=${T64_BOOT_FIXTURE_DIR}")

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

add_executable(test_scheduler tests/test_scheduler.c)
target_include_directories(test_scheduler PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_scheduler PRIVATE t64_core)
t64_target_warnings(test_scheduler)
add_test(NAME unit.core.scheduler COMMAND test_scheduler)
set_tests_properties(unit.core.scheduler PROPERTIES LABELS unit)

add_executable(test_digest_trace tests/test_digest_trace.c)
target_include_directories(test_digest_trace PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_digest_trace PRIVATE t64_core)
t64_target_warnings(test_digest_trace)
add_test(NAME unit.core.digest-trace COMMAND test_digest_trace)
set_tests_properties(unit.core.digest-trace PROPERTIES LABELS unit)

add_executable(test_determinism tests/test_determinism.c)
target_include_directories(test_determinism PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_determinism PRIVATE t64_core)
t64_target_warnings(test_determinism)
add_test(NAME unit.core.determinism COMMAND test_determinism)
set_tests_properties(unit.core.determinism PROPERTIES LABELS unit)

add_executable(test_bus tests/test_bus.c)
target_include_directories(test_bus PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_bus PRIVATE t64_core)
t64_target_warnings(test_bus)
add_test(NAME unit.core.bus COMMAND test_bus)
set_tests_properties(unit.core.bus PROPERTIES LABELS unit)

add_executable(test_pi_dma tests/test_pi_dma.c)
target_include_directories(test_pi_dma PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_pi_dma PRIVATE t64_core)
t64_target_warnings(test_pi_dma)
add_test(NAME unit.core.pi-dma COMMAND test_pi_dma)
set_tests_properties(unit.core.pi-dma PROPERTIES LABELS unit)

add_executable(test_sp_dma tests/test_sp_dma.c)
target_include_directories(test_sp_dma PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_sp_dma PRIVATE t64_core)
t64_target_warnings(test_sp_dma)
add_test(NAME unit.core.sp-dma COMMAND test_sp_dma)
set_tests_properties(unit.core.sp-dma PROPERTIES LABELS unit)

# Long admission fuzzing and the intentional fault are only available from the
# existing ASan/UBSan preset. They are deliberately absent from the fast gate.
set(T64_HAS_ADDRESS_AND_UNDEFINED_SANITIZERS FALSE)
if(CMAKE_C_FLAGS MATCHES "-fsanitize=address,undefined" AND
   CMAKE_EXE_LINKER_FLAGS MATCHES "-fsanitize=address,undefined")
  set(T64_HAS_ADDRESS_AND_UNDEFINED_SANITIZERS TRUE)
endif()

if(T64_HAS_ADDRESS_AND_UNDEFINED_SANITIZERS)
  # Sanitizer instrumentation grows existing core stack frames. Keep the
  # project's frame-size diagnostics visible but nonfatal in this preset only.
  target_compile_options(t64_core PRIVATE -Wno-error=frame-larger-than)

  add_executable(test_fuzz_rom_admission tests/fuzz_rom_admission.c)
  target_include_directories(test_fuzz_rom_admission PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/tests")
  target_link_libraries(test_fuzz_rom_admission PRIVATE t64_core)
  t64_target_warnings(test_fuzz_rom_admission)
  add_test(NAME fuzz.rom-admission COMMAND test_fuzz_rom_admission)
  set_tests_properties(fuzz.rom-admission PROPERTIES TIMEOUT 120 LABELS fuzz)

  add_executable(sanitizer_fault tests/sanitizer_fault.c)
  t64_target_warnings(sanitizer_fault)
  add_test(NAME sanitizer.negative-control
    COMMAND sh "${CMAKE_SOURCE_DIR}/scripts/expect-sanitizer.sh"
      "$<TARGET_FILE:sanitizer_fault>")
  set_tests_properties(sanitizer.negative-control PROPERTIES LABELS sanitizer)
endif()
