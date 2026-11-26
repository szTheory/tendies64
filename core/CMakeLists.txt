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
include("${CMAKE_CURRENT_SOURCE_DIR}/softfloat/sources.cmake")

# Build identity is a digest of the actual production inputs and build policy.
# Only hashes and source-relative names enter the digest; no path or timestamp
# is emitted. Configure dependencies refresh it whenever a production input
# changes, including edits between builds.
set(T64_BUILD_INPUTS
  ${T64_CORE_SOURCES}
  ${T64_CORE_HEADERS}
  ${T64_CORE_INTERNAL_HEADERS}
  ${T64_SOFTFLOAT_SOURCES}
  ${T64_SOFTFLOAT_HEADERS}
  "${CMAKE_CURRENT_SOURCE_DIR}/softfloat/sources.cmake"
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

# The SHA-256 source is folded into the archive (plan 11-16, DR-20): a PRIVATE
# link of the t64_sha256 target would make install(EXPORT) fail, because an
# exported target may not link a target outside the export set. The core's
# private copy is renamed to t64i_sha256_* by the compile definitions below, so
# the installed archive keeps no unprefixed internal name and host tools keep
# linking t64_sha256 with the t64_sha256_* names.
add_library(t64_core STATIC
  ${T64_CORE_SOURCES}
  ${T64_SOFTFLOAT_SOURCES}
  ${T64_SOFTFLOAT_HEADERS}
  "${CMAKE_SOURCE_DIR}/tools/common/t64_sha256.c")
target_include_directories(t64_core
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
         "$<INSTALL_INTERFACE:include>"
  PRIVATE "${CMAKE_SOURCE_DIR}/tools/common"
          "${CMAKE_CURRENT_SOURCE_DIR}/softfloat"
          "${CMAKE_CURRENT_SOURCE_DIR}")
# Internal symbol rename (DR-17, plan 11-11): core/t64_prefix.h is generated by
# scripts/gen-prefix-header.py and reached by #include from t64_internal.h,
# softfloat/platform.h and include/tendies64/t64_facade.h, so every core
# translation unit, white-box test, probe and the lint compile sees one
# header. The core directory is on the include path only for that include.
t64_target_warnings(t64_core)
# Core-only flags: no implicit float promotion and bounded stack usage.
target_compile_options(t64_core PRIVATE -Wdouble-promotion
  -Wframe-larger-than=2048)
target_compile_definitions(t64_core PRIVATE
  "SOFTFLOAT_FAST_INT64=1"
  "T64_BUILD_ID=\"${T64_BUILD_ID}\""
  "T64_SOURCE_REVISION=\"${T64_SOURCE_REVISION}\""
  # The four sha256 names of the folded copy (DR-17, DR-20).
  "t64_sha256_init=t64i_sha256_init"
  "t64_sha256_update=t64i_sha256_update"
  "t64_sha256_final=t64i_sha256_final"
  "t64_sha256_hex=t64i_sha256_hex")

# Installable package (plan 11-16, DR-18 and DR-20): the archive, the one
# public header and the CMake package files, component Development, relocatable
# through _IMPORT_PREFIX. The package version is the ABI candidate number
# (0.1.0, SameMinorVersion for 0.x), a literal and never PROJECT_VERSION:
# project(VERSION 0.0.0) is not a release version and stays untouched. The
# config records the single built configuration and the build id.
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
add_library(tendies64::core ALIAS t64_core)
set_target_properties(t64_core PROPERTIES EXPORT_NAME core)
target_sources(t64_core PUBLIC
  FILE_SET HEADERS
  BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
  FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/tendies64/t64_machine.h")
install(TARGETS t64_core EXPORT tendies64Targets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT Development
  FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT Development)
install(EXPORT tendies64Targets NAMESPACE tendies64:: FILE tendies64Targets.cmake
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/tendies64" COMPONENT Development)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/tendies64ConfigVersion.cmake"
  VERSION 0.1.0 COMPATIBILITY SameMinorVersion)
configure_package_config_file(tendies64Config.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/tendies64Config.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/tendies64")
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/tendies64Config.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/tendies64ConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/tendies64" COMPONENT Development)

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

# Keep the new RDP fixture recipe independently qualified so extending it does
# not change the historical boot/hello builder source pin.
set(T64_RDP_FILL_FIXTURE_OUTPUTS
  "${T64_BOOT_FIXTURE_DIR}/rdp-fill.z64"
  "${T64_BOOT_FIXTURE_DIR}/rdp-fill.payload.bin"
  "${T64_BOOT_FIXTURE_DIR}/rdp-fill.build.json")
add_custom_command(
  OUTPUT ${T64_RDP_FILL_FIXTURE_OUTPUTS}
  COMMAND "${CMAKE_COMMAND}" -E env "CC=${CMAKE_C_COMPILER}"
    sh "${CMAKE_SOURCE_DIR}/scripts/build-rdp-fill-fixture.sh" --build
  DEPENDS
    "${CMAKE_SOURCE_DIR}/fixtures/manifest.json"
    "${CMAKE_SOURCE_DIR}/fixtures/rdp/fill.S"
    "${CMAKE_SOURCE_DIR}/fixtures/rdp/fill.ld"
    "${CMAKE_SOURCE_DIR}/scripts/build-rdp-fill-fixture.sh"
    "${CMAKE_SOURCE_DIR}/tools/fixture/t64_rom_pack.c"
    "${CMAKE_SOURCE_DIR}/tools/common/t64_sha256.c"
    "${CMAKE_SOURCE_DIR}/tools/common/t64_sha256.h"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Building independently authored RDP fill fixture"
  VERBATIM)
add_custom_target(t64-rdp-fill-fixture DEPENDS ${T64_RDP_FILL_FIXTURE_OUTPUTS})
add_dependencies(t64-rdp-fill-fixture t64-boot-fixtures)

set(T64_AUDIO_FIXTURE_OUTPUTS
  "${T64_BOOT_FIXTURE_DIR}/audio-pcm.z64"
  "${T64_BOOT_FIXTURE_DIR}/audio-pcm.elf"
  "${T64_BOOT_FIXTURE_DIR}/audio-pcm.payload.bin"
  "${T64_BOOT_FIXTURE_DIR}/audio-pcm.json"
  "${T64_BOOT_FIXTURE_DIR}/audio-pcm-wrong.z64"
  "${T64_BOOT_FIXTURE_DIR}/audio-pcm-wrong.elf"
  "${T64_BOOT_FIXTURE_DIR}/audio-pcm-wrong.payload.bin"
  "${T64_BOOT_FIXTURE_DIR}/audio-pcm-wrong.json")
add_custom_command(
  OUTPUT ${T64_AUDIO_FIXTURE_OUTPUTS}
  COMMAND "${CMAKE_COMMAND}" -E env "CC=${CMAKE_C_COMPILER}"
    sh "${CMAKE_SOURCE_DIR}/scripts/build-audio-fixture.sh" --build
  DEPENDS
    "${CMAKE_SOURCE_DIR}/fixtures/manifest.json"
    "${CMAKE_SOURCE_DIR}/fixtures/audio/pcm.S"
    "${CMAKE_SOURCE_DIR}/fixtures/audio/pcm.ld"
    "${CMAKE_SOURCE_DIR}/scripts/build-audio-fixture.sh"
    "${CMAKE_SOURCE_DIR}/tools/fixture/t64_rom_pack.c"
    "${CMAKE_SOURCE_DIR}/tools/common/t64_sha256.c"
    "${CMAKE_SOURCE_DIR}/tools/common/t64_sha256.h"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Building authored CPU audio and VI fixtures"
  VERBATIM)
add_custom_target(t64-audio-fixture DEPENDS ${T64_AUDIO_FIXTURE_OUTPUTS})
add_dependencies(t64-audio-fixture t64-boot-fixtures)

# Phase 9 admit family: one authored source builds six ROMs through defsyms (one
# per registered profile id, a wrong-transcript variant and a long-spin
# variant). Kept as its own recipe so the audio and boot builders keep their
# historical source pins.
set(T64_PHASE09_ADMIT_STEMS
  phase09-admit-si-v1 phase09-admit-e4k phase09-admit-e16k phase09-admit-s32k
  phase09-admit-wrong-si-v1 phase09-admit-long-si-v1)
set(T64_PHASE09_ADMIT_OUTPUTS)
foreach(T64_ADMIT_STEM IN LISTS T64_PHASE09_ADMIT_STEMS)
  list(APPEND T64_PHASE09_ADMIT_OUTPUTS
    "${T64_BOOT_FIXTURE_DIR}/${T64_ADMIT_STEM}.z64"
    "${T64_BOOT_FIXTURE_DIR}/${T64_ADMIT_STEM}.elf"
    "${T64_BOOT_FIXTURE_DIR}/${T64_ADMIT_STEM}.payload.bin"
    "${T64_BOOT_FIXTURE_DIR}/${T64_ADMIT_STEM}.json")
endforeach()
add_custom_command(
  OUTPUT ${T64_PHASE09_ADMIT_OUTPUTS}
  COMMAND "${CMAKE_COMMAND}" -E env "CC=${CMAKE_C_COMPILER}"
    sh "${CMAKE_SOURCE_DIR}/scripts/build-phase09-admit-fixture.sh" --build
  DEPENDS
    "${CMAKE_SOURCE_DIR}/fixtures/manifest.json"
    "${CMAKE_SOURCE_DIR}/fixtures/phase09/admit.S"
    "${CMAKE_SOURCE_DIR}/fixtures/phase09/admit.ld"
    "${CMAKE_SOURCE_DIR}/scripts/build-phase09-admit-fixture.sh"
    "${CMAKE_SOURCE_DIR}/tools/fixture/t64_rom_pack.c"
    "${CMAKE_SOURCE_DIR}/tools/common/t64_sha256.c"
    "${CMAKE_SOURCE_DIR}/tools/common/t64_sha256.h"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Building authored Phase 9 admit fixtures"
  VERBATIM)
add_custom_target(t64-phase09-admit-fixture DEPENDS ${T64_PHASE09_ADMIT_OUTPUTS})
add_dependencies(t64-phase09-admit-fixture t64-boot-fixtures)

# Phase 9 pad-poll pair: one authored source builds the baseline ROM and the
# wrong-port-flag ROM through a defsym; its own recipe keeps the admit, audio
# and boot builders on their historical source pins.
set(T64_PHASE09_PAD_STEMS
  phase09-pad-poll-si-v1 phase09-pad-poll-wrong-si-v1)
set(T64_PHASE09_PAD_OUTPUTS)
foreach(T64_PAD_STEM IN LISTS T64_PHASE09_PAD_STEMS)
  list(APPEND T64_PHASE09_PAD_OUTPUTS
    "${T64_BOOT_FIXTURE_DIR}/${T64_PAD_STEM}.z64"
    "${T64_BOOT_FIXTURE_DIR}/${T64_PAD_STEM}.elf"
    "${T64_BOOT_FIXTURE_DIR}/${T64_PAD_STEM}.payload.bin"
    "${T64_BOOT_FIXTURE_DIR}/${T64_PAD_STEM}.json")
endforeach()
add_custom_command(
  OUTPUT ${T64_PHASE09_PAD_OUTPUTS}
  COMMAND "${CMAKE_COMMAND}" -E env "CC=${CMAKE_C_COMPILER}"
    sh "${CMAKE_SOURCE_DIR}/scripts/build-pad-fixture.sh" --build
  DEPENDS
    "${CMAKE_SOURCE_DIR}/fixtures/manifest.json"
    "${CMAKE_SOURCE_DIR}/fixtures/pad/poll.S"
    "${CMAKE_SOURCE_DIR}/fixtures/pad/poll.ld"
    "${CMAKE_SOURCE_DIR}/scripts/build-pad-fixture.sh"
    "${CMAKE_SOURCE_DIR}/tools/fixture/t64_rom_pack.c"
    "${CMAKE_SOURCE_DIR}/tools/common/t64_sha256.c"
    "${CMAKE_SOURCE_DIR}/tools/common/t64_sha256.h"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Building authored Phase 9 pad-poll fixtures"
  VERBATIM)
add_custom_target(t64-phase09-pad-fixture DEPENDS ${T64_PHASE09_PAD_OUTPUTS})
add_dependencies(t64-phase09-pad-fixture t64-boot-fixtures)

# Phase 9 replay-mix ROM (plan 09-07): one authored source builds the single
# replay-mix ROM; its own recipe keeps the admit, audio, pad and boot builders
# on their historical source pins.
set(T64_PHASE09_REPLAY_STEMS phase09-replay-mix-si-v1)
set(T64_PHASE09_REPLAY_OUTPUTS)
foreach(T64_REPLAY_STEM IN LISTS T64_PHASE09_REPLAY_STEMS)
  list(APPEND T64_PHASE09_REPLAY_OUTPUTS
    "${T64_BOOT_FIXTURE_DIR}/${T64_REPLAY_STEM}.z64"
    "${T64_BOOT_FIXTURE_DIR}/${T64_REPLAY_STEM}.elf"
    "${T64_BOOT_FIXTURE_DIR}/${T64_REPLAY_STEM}.payload.bin"
    "${T64_BOOT_FIXTURE_DIR}/${T64_REPLAY_STEM}.json")
endforeach()
add_custom_command(
  OUTPUT ${T64_PHASE09_REPLAY_OUTPUTS}
  COMMAND "${CMAKE_COMMAND}" -E env "CC=${CMAKE_C_COMPILER}"
    sh "${CMAKE_SOURCE_DIR}/scripts/build-replay-fixture.sh" --build
  DEPENDS
    "${CMAKE_SOURCE_DIR}/fixtures/manifest.json"
    "${CMAKE_SOURCE_DIR}/fixtures/replay/mix.S"
    "${CMAKE_SOURCE_DIR}/fixtures/replay/mix.ld"
    "${CMAKE_SOURCE_DIR}/scripts/build-replay-fixture.sh"
    "${CMAKE_SOURCE_DIR}/tools/fixture/t64_rom_pack.c"
    "${CMAKE_SOURCE_DIR}/tools/common/t64_sha256.c"
    "${CMAKE_SOURCE_DIR}/tools/common/t64_sha256.h"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Building authored Phase 9 replay-mix fixture"
  VERBATIM)
add_custom_target(t64-phase09-replay-fixture DEPENDS ${T64_PHASE09_REPLAY_OUTPUTS})
add_dependencies(t64-phase09-replay-fixture t64-boot-fixtures)

# Phase 9 save fixtures (plan 09-04, plan 09-05): two authored sources build
# the three eeprom-rw ROMs and the two sram-rw ROMs through defsyms, and a
# third builds the save-roundtrip ROM; its own recipe keeps the admit, audio,
# pad and boot builders on their historical source pins.
set(T64_PHASE09_SAVE_STEMS
  phase09-eeprom-rw-e4k-v1 phase09-eeprom-rw-e16k-v1
  phase09-eeprom-rw-wrong-e4k-v1 phase09-sram-rw-s32k-v1
  phase09-sram-rw-wrong-s32k-v1 phase09-save-roundtrip-s32k)
set(T64_PHASE09_SAVE_OUTPUTS)
foreach(T64_SAVE_STEM IN LISTS T64_PHASE09_SAVE_STEMS)
  list(APPEND T64_PHASE09_SAVE_OUTPUTS
    "${T64_BOOT_FIXTURE_DIR}/${T64_SAVE_STEM}.z64"
    "${T64_BOOT_FIXTURE_DIR}/${T64_SAVE_STEM}.elf"
    "${T64_BOOT_FIXTURE_DIR}/${T64_SAVE_STEM}.payload.bin"
    "${T64_BOOT_FIXTURE_DIR}/${T64_SAVE_STEM}.json")
endforeach()
add_custom_command(
  OUTPUT ${T64_PHASE09_SAVE_OUTPUTS}
  COMMAND "${CMAKE_COMMAND}" -E env "CC=${CMAKE_C_COMPILER}"
    sh "${CMAKE_SOURCE_DIR}/scripts/build-save-fixtures.sh" --build
  DEPENDS
    "${CMAKE_SOURCE_DIR}/fixtures/manifest.json"
    "${CMAKE_SOURCE_DIR}/fixtures/save/eeprom.S"
    "${CMAKE_SOURCE_DIR}/fixtures/save/eeprom.ld"
    "${CMAKE_SOURCE_DIR}/fixtures/save/sram.S"
    "${CMAKE_SOURCE_DIR}/fixtures/save/sram.ld"
    "${CMAKE_SOURCE_DIR}/fixtures/save/roundtrip.S"
    "${CMAKE_SOURCE_DIR}/fixtures/save/roundtrip.ld"
    "${CMAKE_SOURCE_DIR}/scripts/build-save-fixtures.sh"
    "${CMAKE_SOURCE_DIR}/tools/fixture/t64_rom_pack.c"
    "${CMAKE_SOURCE_DIR}/tools/common/t64_sha256.c"
    "${CMAKE_SOURCE_DIR}/tools/common/t64_sha256.h"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Building authored Phase 9 save fixtures"
  VERBATIM)
add_custom_target(t64-phase09-save-fixture DEPENDS ${T64_PHASE09_SAVE_OUTPUTS})
add_dependencies(t64-phase09-save-fixture t64-boot-fixtures)

# Phase 10 composite fixture (plan 10-14, RUN-07): one authored guest builds the
# accepted ROM and its one-defsym wrong-byte control; its own builder keeps the
# Phase 9 builders on their historical source pins.
set(T64_PHASE10_COMPOSITE_STEMS
  phase10-composite-s32k phase10-composite-wrong-s32k)
set(T64_PHASE10_COMPOSITE_OUTPUTS)
foreach(T64_COMPOSITE_STEM IN LISTS T64_PHASE10_COMPOSITE_STEMS)
  list(APPEND T64_PHASE10_COMPOSITE_OUTPUTS
    "${T64_BOOT_FIXTURE_DIR}/${T64_COMPOSITE_STEM}.z64"
    "${T64_BOOT_FIXTURE_DIR}/${T64_COMPOSITE_STEM}.elf"
    "${T64_BOOT_FIXTURE_DIR}/${T64_COMPOSITE_STEM}.payload.bin"
    "${T64_BOOT_FIXTURE_DIR}/${T64_COMPOSITE_STEM}.json")
endforeach()
add_custom_command(
  OUTPUT ${T64_PHASE10_COMPOSITE_OUTPUTS}
  COMMAND "${CMAKE_COMMAND}" -E env "CC=${CMAKE_C_COMPILER}"
    sh "${CMAKE_SOURCE_DIR}/scripts/build-composite-fixture.sh" --build
  DEPENDS
    "${CMAKE_SOURCE_DIR}/fixtures/manifest.json"
    "${CMAKE_SOURCE_DIR}/fixtures/composite/composite.S"
    "${CMAKE_SOURCE_DIR}/fixtures/composite/composite.ld"
    "${CMAKE_SOURCE_DIR}/scripts/build-composite-fixture.sh"
    "${CMAKE_SOURCE_DIR}/tools/fixture/t64_rom_pack.c"
    "${CMAKE_SOURCE_DIR}/tools/common/t64_sha256.c"
    "${CMAKE_SOURCE_DIR}/tools/common/t64_sha256.h"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Building authored Phase 10 composite fixtures"
  VERBATIM)
add_custom_target(t64-phase10-composite-fixture
  DEPENDS ${T64_PHASE10_COMPOSITE_OUTPUTS})
add_dependencies(t64-phase10-composite-fixture t64-boot-fixtures)

add_executable(test_tracer tests/test_tracer.c)
target_include_directories(test_tracer PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_tracer PRIVATE t64_core t64_sha256)
t64_target_warnings(test_tracer)
add_dependencies(test_tracer t64-boot-fixtures)
add_test(NAME unit.core.tracer COMMAND test_tracer)
set_tests_properties(unit.core.tracer PROPERTIES LABELS unit)

add_executable(test_rdp tests/test_rdp.c)
target_include_directories(test_rdp PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_rdp PRIVATE t64_core)
t64_target_warnings(test_rdp)
add_dependencies(test_rdp t64-rdp-fill-fixture)
add_test(NAME unit.core.rdp.fill COMMAND test_rdp)
set_tests_properties(unit.core.rdp.fill PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_RDP_FILL_PATH=${T64_BOOT_FIXTURE_DIR}/rdp-fill.z64")

add_executable(test_rdp_copy tests/test_rdp_copy.c)
target_include_directories(test_rdp_copy PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_rdp_copy PRIVATE t64_core)
t64_target_warnings(test_rdp_copy)
add_dependencies(test_rdp_copy t64-rdp-fill-fixture)
add_test(NAME unit.core.rdp.copy COMMAND test_rdp_copy)
set_tests_properties(unit.core.rdp.copy PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_RDP_FILL_PATH=${T64_BOOT_FIXTURE_DIR}/rdp-fill.z64")

add_executable(test_rdp_triangles tests/test_rdp_triangles.c)
target_include_directories(test_rdp_triangles PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_rdp_triangles PRIVATE t64_core)
t64_target_warnings(test_rdp_triangles)
add_dependencies(test_rdp_triangles t64-rdp-fill-fixture)
add_test(NAME unit.core.rdp.triangles COMMAND test_rdp_triangles)
set_tests_properties(unit.core.rdp.triangles PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_RDP_FILL_PATH=${T64_BOOT_FIXTURE_DIR}/rdp-fill.z64")

add_executable(test_rdp_texture tests/test_rdp_texture.c)
target_include_directories(test_rdp_texture PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_rdp_texture PRIVATE t64_core)
t64_target_warnings(test_rdp_texture)
add_dependencies(test_rdp_texture t64-rdp-fill-fixture)
add_test(NAME unit.core.rdp.texture COMMAND test_rdp_texture)
set_tests_properties(unit.core.rdp.texture PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_RDP_FILL_PATH=${T64_BOOT_FIXTURE_DIR}/rdp-fill.z64")

add_executable(test_rdp_combiner tests/test_rdp_combiner.c)
target_include_directories(test_rdp_combiner PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_rdp_combiner PRIVATE t64_core)
t64_target_warnings(test_rdp_combiner)
add_dependencies(test_rdp_combiner t64-rdp-fill-fixture)
add_test(NAME unit.core.rdp.combiner COMMAND test_rdp_combiner)
set_tests_properties(unit.core.rdp.combiner PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_RDP_FILL_PATH=${T64_BOOT_FIXTURE_DIR}/rdp-fill.z64")

add_executable(test_rdp_blender tests/test_rdp_blender.c)
target_include_directories(test_rdp_blender PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_rdp_blender PRIVATE t64_core)
t64_target_warnings(test_rdp_blender)
add_dependencies(test_rdp_blender t64-rdp-fill-fixture)
add_test(NAME unit.core.rdp.blender COMMAND test_rdp_blender)
set_tests_properties(unit.core.rdp.blender PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_RDP_FILL_PATH=${T64_BOOT_FIXTURE_DIR}/rdp-fill.z64")

add_executable(test_rdp_depth tests/test_rdp_depth.c)
target_include_directories(test_rdp_depth PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_rdp_depth PRIVATE t64_core)
t64_target_warnings(test_rdp_depth)
add_dependencies(test_rdp_depth t64-rdp-fill-fixture)
add_test(NAME unit.core.rdp.depth COMMAND test_rdp_depth)
set_tests_properties(unit.core.rdp.depth PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_RDP_FILL_PATH=${T64_BOOT_FIXTURE_DIR}/rdp-fill.z64")

add_executable(test_dp_transport tests/test_rdp.c)
target_compile_definitions(test_dp_transport PRIVATE T64_TEST_DP_TRANSPORT=1)
target_include_directories(test_dp_transport PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_dp_transport PRIVATE t64_core)
t64_target_warnings(test_dp_transport)
add_dependencies(test_dp_transport t64-rdp-fill-fixture)
add_test(NAME unit.core.dp.transport COMMAND test_dp_transport)
set_tests_properties(unit.core.dp.transport PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_RDP_FILL_PATH=${T64_BOOT_FIXTURE_DIR}/rdp-fill.z64")

add_executable(test_vi tests/test_vi.c)
target_include_directories(test_vi PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_vi PRIVATE t64_core)
t64_target_warnings(test_vi)
add_dependencies(test_vi t64-rdp-fill-fixture)
add_test(NAME unit.core.vi.fill COMMAND test_vi --suite fill)
add_test(NAME unit.core.vi.timing COMMAND test_vi --suite vi-timing)
add_test(NAME unit.core.vi.filters COMMAND test_vi --suite vi-filters)
set_tests_properties(unit.core.vi.fill PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_RDP_FILL_PATH=${T64_BOOT_FIXTURE_DIR}/rdp-fill.z64")
set_tests_properties(unit.core.vi.timing PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_RDP_FILL_PATH=${T64_BOOT_FIXTURE_DIR}/rdp-fill.z64")
set_tests_properties(unit.core.vi.filters PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_RDP_FILL_PATH=${T64_BOOT_FIXTURE_DIR}/rdp-fill.z64")

add_executable(test_video_output tests/test_video_output.c)
target_include_directories(test_video_output PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_video_output PRIVATE t64_core)
t64_target_warnings(test_video_output)
add_dependencies(test_video_output t64-rdp-fill-fixture)
add_test(NAME unit.core.vi.output COMMAND test_video_output --suite video-output)
set_tests_properties(unit.core.vi.output PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_RDP_FILL_PATH=${T64_BOOT_FIXTURE_DIR}/rdp-fill.z64")

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
add_test(NAME integration.cold-determinism COMMAND test_boot_determinism --cold)
set_tests_properties(integration.cold-determinism PROPERTIES
  LABELS integration TIMEOUT 180 RUN_SERIAL TRUE
  ENVIRONMENT
    "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64;T64_TEST_INPUT_PATH=${T64_BOOT_FIXTURE_DIR}/hello.input.json;T64_BOOT_FIXTURES_DIR=${T64_BOOT_FIXTURE_DIR}")
set_tests_properties(integration.open-ipl3-determinism PROPERTIES
  LABELS integration TIMEOUT 180
  ENVIRONMENT
    "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64;T64_TEST_INPUT_PATH=${T64_BOOT_FIXTURE_DIR}/hello.input.json;T64_BOOT_FIXTURES_DIR=${T64_BOOT_FIXTURE_DIR}")

add_executable(test_isviewer tests/test_isviewer.c)
target_include_directories(test_isviewer PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tools/common")
target_link_libraries(test_isviewer PRIVATE t64_core t64_sha256)
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

add_executable(test_cpu_link tests/test_cpu_link.c)
target_include_directories(test_cpu_link PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(test_cpu_link PRIVATE t64_core)
t64_target_warnings(test_cpu_link)
add_dependencies(test_cpu_link t64-boot-fixtures)
add_test(NAME unit.core.cpu-link COMMAND test_cpu_link)
set_tests_properties(unit.core.cpu-link PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")

add_executable(test_softfloat tests/test_softfloat.c)
target_include_directories(test_softfloat PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_softfloat PRIVATE t64_core)
t64_target_warnings(test_softfloat)
add_test(NAME unit.core.softfloat32 COMMAND test_softfloat --softfloat32)
add_test(NAME unit.core.softfloat64 COMMAND test_softfloat --softfloat64)
add_test(NAME unit.core.softfloat-isolation COMMAND test_softfloat --isolation)
set_tests_properties(unit.core.softfloat32 unit.core.softfloat64
  unit.core.softfloat-isolation PROPERTIES LABELS unit)

add_executable(test_cpu_cache tests/test_cpu_cache.c)
target_include_directories(test_cpu_cache PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_cpu_cache PRIVATE t64_core)
t64_target_warnings(test_cpu_cache)
add_dependencies(test_cpu_cache t64-boot-fixtures)
add_test(NAME unit.core.cpu-icache COMMAND test_cpu_cache)
set_tests_properties(unit.core.cpu-icache PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")
add_test(NAME unit.core.cpu-cache-ops COMMAND test_cpu_cache --cache-ops)
set_tests_properties(unit.core.cpu-cache-ops PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")
add_test(NAME unit.core.cpu-dcache COMMAND test_cpu_cache --dcache)
set_tests_properties(unit.core.cpu-dcache PROPERTIES
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
add_dependencies(test_cpu_time t64-boot-fixtures)
add_test(NAME unit.core.cpu-time COMMAND test_cpu_time)
set_tests_properties(unit.core.cpu-time PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")

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

add_executable(test_cpu_mmu tests/test_cpu_mmu.c)
target_include_directories(test_cpu_mmu PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_cpu_mmu PRIVATE t64_core)
t64_target_warnings(test_cpu_mmu)
add_dependencies(test_cpu_mmu t64-boot-fixtures)
add_test(NAME unit.core.cpu-mmu COMMAND test_cpu_mmu)
set_tests_properties(unit.core.cpu-mmu PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")

add_executable(test_cpu_memory tests/test_cpu_memory.c)
target_include_directories(test_cpu_memory PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_cpu_memory PRIVATE t64_core)
t64_target_warnings(test_cpu_memory)
add_dependencies(test_cpu_memory t64-boot-fixtures)
add_test(NAME unit.core.cpu-memory COMMAND test_cpu_memory)
set_tests_properties(unit.core.cpu-memory PROPERTIES
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

add_executable(test_cop1 tests/test_cop1.c)
target_include_directories(test_cop1 PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_cop1 PRIVATE t64_core)
t64_target_warnings(test_cop1)
add_dependencies(test_cop1 t64-boot-fixtures)
add_test(NAME unit.core.cop1-state COMMAND test_cop1)
set_tests_properties(unit.core.cop1-state PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")
add_test(NAME unit.core.cop1-arithmetic COMMAND test_cop1 --arithmetic)
set_tests_properties(unit.core.cop1-arithmetic PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")
add_test(NAME unit.core.cop1-exceptions COMMAND test_cop1 --exceptions)
set_tests_properties(unit.core.cop1-exceptions PROPERTIES
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

add_executable(test_rdram tests/test_rdram.c)
target_include_directories(test_rdram PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_rdram PRIVATE t64_core)
t64_target_warnings(test_rdram)
add_dependencies(test_rdram t64-boot-fixtures)
add_test(NAME unit.core.rdram-protocol COMMAND test_rdram)
set_tests_properties(unit.core.rdram-protocol PROPERTIES LABELS unit)
set_tests_properties(unit.core.rdram-protocol PROPERTIES
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")

add_executable(test_profile tests/test_profile.c)
target_include_directories(test_profile PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_profile PRIVATE t64_core)
t64_target_warnings(test_profile)
add_test(NAME unit.core.profile COMMAND test_profile)
set_tests_properties(unit.core.profile PROPERTIES LABELS unit)

add_executable(test_ai tests/test_ai.c)
target_include_directories(test_ai PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_ai PRIVATE t64_core)
t64_target_warnings(test_ai)
add_dependencies(test_ai t64-boot-fixtures)
add_test(NAME unit.core.ai.tracer
  COMMAND test_ai --case cpu-two-dma-literal-pcm)
set_tests_properties(unit.core.ai.tracer PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")
add_test(NAME unit.core.ai.profile
  COMMAND test_ai --case new-profile-inherited-devices)
add_test(NAME unit.core.ai.old-profile-refusals
  COMMAND test_ai --case old-profile-ai-refusals)
add_test(NAME unit.core.ai.registers
  COMMAND test_ai --suite registers)
add_test(NAME unit.core.ai.cadence
  COMMAND test_ai --suite cadence)
add_test(NAME unit.core.ai.output
  COMMAND test_ai --suite output)
add_test(NAME unit.core.ai.determinism
  COMMAND test_ai --suite determinism)
add_test(NAME unit.core.ai.profile-pins
  COMMAND test_profile --case old-profile-pins)
add_test(NAME unit.core.ai.profile-mutations
  COMMAND test_profile --case descriptor-policy-mutations)
set_tests_properties(unit.core.ai.profile unit.core.ai.old-profile-refusals
  unit.core.ai.registers unit.core.ai.cadence unit.core.ai.output
  unit.core.ai.determinism
  PROPERTIES LABELS unit
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")
set_tests_properties(unit.core.ai.profile-pins
  unit.core.ai.profile-mutations PROPERTIES LABELS unit)

# Phase 9 identity, digest and ledger tests (plan 09-01).
add_executable(test_si_profile tests/test_si_profile.c)
target_include_directories(test_si_profile PRIVATE
  "${CMAKE_SOURCE_DIR}/tests"
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tools/common")
target_link_libraries(test_si_profile PRIVATE t64_core t64_sha256)
t64_target_warnings(test_si_profile)
add_dependencies(test_si_profile t64-boot-fixtures)
add_test(NAME unit.core.profile.predicate-matrix
  COMMAND test_si_profile --case predicate-matrix)
add_test(NAME unit.core.profile.v12-identity
  COMMAND test_si_profile --case si-profile-identity)
add_test(NAME unit.core.profile.quantum-identity
  COMMAND test_si_profile --case quantum-identity)
add_test(NAME unit.core.profile.family-devices
  COMMAND test_si_profile --case family-devices)
set_tests_properties(unit.core.profile.predicate-matrix
  unit.core.profile.v12-identity unit.core.profile.quantum-identity
  unit.core.profile.family-devices PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")

add_executable(test_digest_v25 tests/test_digest_v25.c)
target_include_directories(test_digest_v25 PRIVATE
  "${CMAKE_SOURCE_DIR}/tests"
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tools/common")
target_link_libraries(test_digest_v25 PRIVATE t64_core t64_sha256)
t64_target_warnings(test_digest_v25)
add_dependencies(test_digest_v25 t64-boot-fixtures)
add_test(NAME unit.core.digest.sink-legacy-identity
  COMMAND test_digest_v25 --case sink-legacy-identity)
add_test(NAME unit.core.digest.v25-reference
  COMMAND test_digest_v25 --case v25-reference)
add_test(NAME unit.core.digest.v25-strip-differential
  COMMAND test_digest_v25 --case v25-strip-differential)
set_tests_properties(unit.core.digest.sink-legacy-identity
  unit.core.digest.v25-reference
  unit.core.digest.v25-strip-differential PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")

# K1 characterization (plan 10-02): Z refusal on the shipped composite ids,
# pinned before any edit of the depth predicate.
add_executable(test_k1_refusal tests/test_k1_refusal.c)
target_include_directories(test_k1_refusal PRIVATE
  "${CMAKE_SOURCE_DIR}/tests"
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tools/common")
target_link_libraries(test_k1_refusal PRIVATE t64_core t64_sha256)
t64_target_warnings(test_k1_refusal)
add_dependencies(test_k1_refusal t64-boot-fixtures)
add_test(NAME unit.core.k1.dispatch-refusal
  COMMAND test_k1_refusal --case dispatch-refusal)
add_test(NAME unit.core.k1.apply-refusal
  COMMAND test_k1_refusal --case apply-refusal)
set_tests_properties(unit.core.k1.dispatch-refusal
  unit.core.k1.apply-refusal PROPERTIES
  LABELS "unit;phase10"
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")

# Phase 11 public surface (plan 11-02): the test includes only the public
# header and tests/t64_test.h, so it sees what an installed consumer sees.
# Later shape plans add cases of the unit.core.api family here.
add_executable(test_api_surface tests/test_api_surface.c)
target_include_directories(test_api_surface PRIVATE
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_api_surface PRIVATE t64_core)
t64_target_warnings(test_api_surface)
add_test(NAME unit.core.api.abi-version
  COMMAND test_api_surface --case abi-version)
set_tests_properties(unit.core.api.abi-version PROPERTIES
  LABELS "unit;phase11"
  TIMEOUT 30
  COST 1)

# Defect class and entry guards (plan 11-03). White-box: the test includes
# core/t64_internal.h so it can force a defect and corrupt members the public
# surface cannot reach.
add_executable(test_api_poison tests/test_api_poison.c)
target_include_directories(test_api_poison PRIVATE
  "${CMAKE_SOURCE_DIR}/tests"
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tools/common")
target_link_libraries(test_api_poison PRIVATE t64_core t64_sha256)
t64_target_warnings(test_api_poison)
add_dependencies(test_api_poison t64-boot-fixtures)
add_test(NAME unit.core.api.poison
  COMMAND test_api_poison --case poison)
add_test(NAME unit.core.api.refusals-never-poison
  COMMAND test_api_poison --case refusals-never-poison)
add_test(NAME unit.core.api.reentrant
  COMMAND test_api_poison --case reentrant)
add_test(NAME unit.core.api.null-machine
  COMMAND test_api_poison --case null-machine)
add_test(NAME unit.core.api.defect-sites
  COMMAND test_api_poison --case defect-sites)
add_test(NAME unit.core.api.bus-carve-out
  COMMAND test_api_poison --case bus-carve-out)
set_tests_properties(unit.core.api.poison
  unit.core.api.refusals-never-poison
  unit.core.api.reentrant
  unit.core.api.null-machine
  unit.core.api.defect-sites PROPERTIES
  LABELS "unit;phase11"
  TIMEOUT 60
  COST 1
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")
# The carve-out case snapshots the whole instance around eighteen writes
# (measured 2.2 s native), so it declares its own cost.
set_tests_properties(unit.core.api.bus-carve-out PROPERTIES
  LABELS "unit;phase11"
  TIMEOUT 60
  COST 3
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")

# Lifecycle and output surface (plan 11-04). Public header only: the poisoned,
# reentrant and NULL-machine answers of the new entries are proved with the
# other entries in test_api_poison.
add_executable(test_api_lifecycle tests/test_api_lifecycle.c)
target_include_directories(test_api_lifecycle PRIVATE
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_api_lifecycle PRIVATE t64_core)
t64_target_warnings(test_api_lifecycle)
add_dependencies(test_api_lifecycle t64-boot-fixtures t64-rdp-fill-fixture
  t64-audio-fixture)
add_test(NAME unit.core.api.reset
  COMMAND test_api_lifecycle --case reset)
add_test(NAME unit.core.api.unload
  COMMAND test_api_lifecycle --case unload)
add_test(NAME unit.core.api.not-ready-table
  COMMAND test_api_lifecycle --case not-ready-table)
add_test(NAME unit.core.api.output-status
  COMMAND test_api_lifecycle --case output-status)
set_tests_properties(unit.core.api.reset
  unit.core.api.unload
  unit.core.api.not-ready-table
  unit.core.api.output-status PROPERTIES
  LABELS "unit;phase11"
  TIMEOUT 60
  COST 1
  ENVIRONMENT "T64_BOOT_FIXTURES_DIR=${T64_BOOT_FIXTURE_DIR}")

# Size handshake (plan 11-12): the create-info checks, the registered profile
# hashes against the Wave 0 base and the trace record_size stride. Public
# header only, like test_api_surface. Plan 11-13 adds the struct-size group:
# the floor and larger-caller cases of every in/out entry (it needs the boot
# fixtures and the K1 environment, so its dependencies are explicit).
add_executable(test_api_struct_size tests/test_api_struct_size.c)
target_include_directories(test_api_struct_size PRIVATE
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_api_struct_size PRIVATE t64_core)
t64_target_warnings(test_api_struct_size)
add_dependencies(test_api_struct_size t64-boot-fixtures t64-rdp-fill-fixture
  t64-audio-fixture)
add_test(NAME unit.core.api.handshake COMMAND test_api_struct_size)
set_tests_properties(unit.core.api.handshake PROPERTIES
  LABELS "unit;phase11"
  TIMEOUT 30
  COST 1
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")
add_test(NAME unit.core.api.struct-size
  COMMAND test_api_struct_size --group struct-size)
set_tests_properties(unit.core.api.struct-size PROPERTIES
  LABELS "unit;phase11"
  TIMEOUT 60
  COST 2
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64;T64_BOOT_FIXTURES_DIR=${T64_BOOT_FIXTURE_DIR}")

# Descriptor 13 family (plan 10-09): the four registered si-v2 ids with their
# pinned profile hashes, and ISViewer activity on the eight SI family ids; plan
# 10-10 adds the predicate matrix over all 25 registered ids.
add_executable(test_profile_v26 tests/test_profile_v26.c)
target_include_directories(test_profile_v26 PRIVATE
  "${CMAKE_SOURCE_DIR}/tests"
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tools/common")
target_link_libraries(test_profile_v26 PRIVATE t64_core t64_sha256)
t64_target_warnings(test_profile_v26)
add_dependencies(test_profile_v26 t64-boot-fixtures)
add_test(NAME unit.core.profile.v26-identity
  COMMAND test_profile_v26 --case v26-identity)
add_test(NAME unit.core.profile.v26-isviewer
  COMMAND test_profile_v26 --case v26-isviewer)
add_test(NAME unit.core.profile.v26-predicate-matrix
  COMMAND test_profile_v26 --case v26-predicate-matrix)
set_tests_properties(unit.core.profile.v26-identity
  unit.core.profile.v26-isviewer
  unit.core.profile.v26-predicate-matrix PROPERTIES
  LABELS "unit;phase10"
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")

# Digest v26 (plan 10-09): the completeness flip over every member of the RDP
# block and the tables, and the strip differential against the v25 stream.
add_executable(test_digest_v26 tests/test_digest_v26.c)
target_include_directories(test_digest_v26 PRIVATE
  "${CMAKE_SOURCE_DIR}/tests"
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tools/common")
target_link_libraries(test_digest_v26 PRIVATE t64_core t64_sha256)
t64_target_warnings(test_digest_v26)
add_dependencies(test_digest_v26 t64-boot-fixtures)
add_test(NAME unit.core.digest.v26-completeness
  COMMAND test_digest_v26 --case v26-completeness)
add_test(NAME unit.core.digest.v26-strip-differential
  COMMAND test_digest_v26 --case v26-strip-differential)
set_tests_properties(unit.core.digest.v26-completeness
  unit.core.digest.v26-strip-differential PROPERTIES
  LABELS "unit;phase10"
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")

# Depth on the descriptor 13 family (plan 10-10): accepted effect with exact
# color, depth and sidecar literals, the wrong-depth control, and the four Z
# opcodes, modes and sidecar writers, in a new binary so the oracle-bound
# depth test stays unchanged.
add_executable(test_rdp_depth_composite tests/test_rdp_depth_composite.c)
target_include_directories(test_rdp_depth_composite PRIVATE
  "${CMAKE_SOURCE_DIR}/tests"
  "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(test_rdp_depth_composite PRIVATE t64_core)
t64_target_warnings(test_rdp_depth_composite)
add_dependencies(test_rdp_depth_composite t64-boot-fixtures)
add_test(NAME unit.core.rdp.depth-composite-accepted
  COMMAND test_rdp_depth_composite --case depth-composite-accepted)
add_test(NAME unit.core.rdp.depth-composite-wrong-depth
  COMMAND test_rdp_depth_composite --case depth-composite-wrong-depth)
add_test(NAME unit.core.rdp.depth-composite-sidecar
  COMMAND test_rdp_depth_composite --case depth-composite-sidecar)
set_tests_properties(unit.core.rdp.depth-composite-accepted
  unit.core.rdp.depth-composite-wrong-depth
  unit.core.rdp.depth-composite-sidecar PROPERTIES
  LABELS "unit;phase10"
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")

# VER-06 gap vectors (plans 10-06 and 10-07): VG-01 to VG-05, the Z exponent
# boundaries and the standard-NTSC VI clock vector, one CTest name per case, in
# a new binary so the oracle-bound depth, VI, texture, triangle and blender
# tests stay unchanged.
add_executable(test_vg_gaps tests/test_vg_gaps.c)
target_include_directories(test_vg_gaps PRIVATE
  "${CMAKE_SOURCE_DIR}/tests"
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tools/common")
target_link_libraries(test_vg_gaps PRIVATE t64_core t64_sha256)
t64_target_warnings(test_vg_gaps)
add_dependencies(test_vg_gaps t64-rdp-fill-fixture)
add_test(NAME unit.core.vg.decal-equality
  COMMAND test_vg_gaps --case decal-equality)
add_test(NAME unit.core.vg.primitive-z-saturation
  COMMAND test_vg_gaps --case primitive-z-saturation)
add_test(NAME unit.core.vg.delta-ceiling
  COMMAND test_vg_gaps --case delta-ceiling)
add_test(NAME unit.core.vg.z-exponent-boundaries
  COMMAND test_vg_gaps --case z-exponent-boundaries)
add_test(NAME unit.core.vg.ntsc-clock
  COMMAND test_vg_gaps --case ntsc-clock)
add_test(NAME unit.core.vg.rgba5551-alpha-clear
  COMMAND test_vg_gaps --case rgba5551-alpha-clear)
add_test(NAME unit.core.vg.serrate-filter-parity
  COMMAND test_vg_gaps --case serrate-filter-parity)
add_test(NAME unit.core.vg.perspective-floor
  COMMAND test_vg_gaps --case perspective-floor)
add_test(NAME unit.core.vg.shade-floor-bayer
  COMMAND test_vg_gaps --case shade-floor-bayer)
set_tests_properties(unit.core.vg.decal-equality
  unit.core.vg.primitive-z-saturation
  unit.core.vg.delta-ceiling
  unit.core.vg.z-exponent-boundaries
  unit.core.vg.ntsc-clock
  unit.core.vg.rgba5551-alpha-clear
  unit.core.vg.serrate-filter-parity
  unit.core.vg.perspective-floor
  unit.core.vg.shade-floor-bayer PROPERTIES
  LABELS "unit;phase10"
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64;T64_TEST_RDP_FILL_PATH=${T64_BOOT_FIXTURE_DIR}/rdp-fill.z64")

add_executable(test_sched_ledger tests/test_sched_ledger.c)
target_include_directories(test_sched_ledger PRIVATE
  "${CMAKE_SOURCE_DIR}/tests"
  "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(test_sched_ledger PRIVATE t64_core)
t64_target_warnings(test_sched_ledger)
add_dependencies(test_sched_ledger t64-boot-fixtures)
add_test(NAME unit.core.sched.ledger
  COMMAND test_sched_ledger --case ledger)
add_test(NAME unit.core.sched.characterize-fifo
  COMMAND test_sched_ledger --case characterize-fifo)
set_tests_properties(unit.core.sched.ledger
  unit.core.sched.characterize-fifo PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")

# Phase 9 CORE-02 ordering matrix and the F1 state content record (plan 09-06).
add_executable(test_sched_matrix tests/test_sched_matrix.c)
target_include_directories(test_sched_matrix PRIVATE
  "${CMAKE_SOURCE_DIR}/tests"
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tools/common")
target_link_libraries(test_sched_matrix PRIVATE t64_core t64_sha256)
t64_target_warnings(test_sched_matrix)
add_dependencies(test_sched_matrix t64-boot-fixtures)
add_test(NAME unit.core.sched.matrix
  COMMAND test_sched_matrix --suite matrix)
set_tests_properties(unit.core.sched.matrix PROPERTIES
  LABELS unit
  TIMEOUT 300
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")

# Phase 9 state container export (plan 09-08).
add_executable(test_state_export tests/test_state_export.c)
target_include_directories(test_state_export PRIVATE
  "${CMAKE_SOURCE_DIR}/tests"
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tools/common")
target_link_libraries(test_state_export PRIVATE t64_core t64_sha256)
t64_target_warnings(test_state_export)
add_dependencies(test_state_export t64-boot-fixtures)
add_test(NAME unit.core.state.export
  COMMAND test_state_export)
set_tests_properties(unit.core.state.export PROPERTIES
  LABELS unit
  TIMEOUT 300
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")

# Phase 10 state container export for state 26 (plan 10-11): container version
# word 2, the section 8 depth layout, the derived sizes and the v26 upper bound,
# in a new binary so the phase09-inventoried test_state_export stays unchanged.
# The independent decoder is a separate script, fixtures/state/decode_container_v2.py,
# because fixtures/state/decode_container.py is a pinned Phase 9 oracle; the version
# 1 decoder is passed as well so the test compares both on the same v25 blob.
add_executable(test_state_export_v26 tests/test_state_export_v26.c)
target_include_directories(test_state_export_v26 PRIVATE
  "${CMAKE_SOURCE_DIR}/tests"
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tools/common")
target_link_libraries(test_state_export_v26 PRIVATE t64_core t64_sha256)
t64_target_warnings(test_state_export_v26)
add_dependencies(test_state_export_v26 t64-boot-fixtures)
add_test(NAME unit.core.state.export-v26
  COMMAND test_state_export_v26 --case export-v26-idle)
add_test(NAME unit.core.state.export-v26-bounds
  COMMAND test_state_export_v26 --case export-v26-bounds)
set_tests_properties(unit.core.state.export-v26
  unit.core.state.export-v26-bounds PROPERTIES
  LABELS "unit;phase10"
  TIMEOUT 300
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64;T64_STATE_DECODER=${CMAKE_SOURCE_DIR}/fixtures/state/decode_container_v2.py;T64_STATE_DECODER_V1=${CMAKE_SOURCE_DIR}/fixtures/state/decode_container.py;T64_PHASE10_EVIDENCE_DIR=${CMAKE_BINARY_DIR}/10-11-evidence")

# Phase 10 state container import for state 26 (plan 10-12): the depth scalars
# before the render proof, the hidden table, the mid Z-triangle and mid scan-out
# round trips and the v26 hostile rows, in a new binary so the phase09-inventoried
# test_state_import keeps its case list. The binary links the fuzz campaign of
# core/tests/fuzz_state_import.c (no main there) and calls its v26 entry.
add_executable(test_state_import_v26 tests/test_state_import_v26.c
  tests/fuzz_state_import.c)
target_include_directories(test_state_import_v26 PRIVATE
  "${CMAKE_SOURCE_DIR}/tests"
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tools/common")
target_link_libraries(test_state_import_v26 PRIVATE t64_core t64_sha256)
t64_target_warnings(test_state_import_v26)
add_dependencies(test_state_import_v26 t64-boot-fixtures)
add_test(NAME unit.core.state.import-v26-roundtrip
  COMMAND test_state_import_v26 --case roundtrip)
add_test(NAME unit.core.state.import-v26-hostile
  COMMAND test_state_import_v26 --case hostile)
set_tests_properties(unit.core.state.import-v26-roundtrip
  unit.core.state.import-v26-hostile PROPERTIES
  LABELS "unit;phase10"
  TIMEOUT 300
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64;T64_STATE_MUTANT_INVENTORY_V26=${CMAKE_SOURCE_DIR}/docs/spec/state-mutants-v26.txt")

# Phase 9 state container import (plan 09-09).
add_executable(test_state_import tests/test_state_import.c
  tests/fuzz_state_import.c)
target_include_directories(test_state_import PRIVATE
  "${CMAKE_SOURCE_DIR}/tests"
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tools/common")
target_link_libraries(test_state_import PRIVATE t64_core t64_sha256)
t64_target_warnings(test_state_import)
add_dependencies(test_state_import t64-boot-fixtures t64-rdp-fill-fixture)
add_test(NAME unit.core.state.import
  COMMAND test_state_import)
set_tests_properties(unit.core.state.import PROPERTIES
  LABELS unit
  TIMEOUT 600
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64;T64_TEST_RDP_FILL_PATH=${T64_BOOT_FIXTURE_DIR}/rdp-fill.z64;T64_STATE_MUTANT_INVENTORY=${CMAKE_SOURCE_DIR}/docs/spec/state-mutants.txt")
# Phase 10 GATE-01: a fresh hosted runner has no Testing/Temporary/CTestCostData.txt,
# so ctest starts every test at cost 0 in registration order and this one (210 s on
# the hosted macOS runner, docs/ci-reviews/LOG.md F-111) started late. COST is plain
# decimal seconds, it orders parallel starts only, and it changes no name or inventory.
# It sits outside the sanitizer conditional below so both presets order the same way.
set_tests_properties(unit.core.state.import PROPERTIES COST 210)

# Phase 9 SI, PIF and input-latch tests (plan 09-03).
add_executable(test_si tests/test_si.c)
target_include_directories(test_si PRIVATE
  "${CMAKE_SOURCE_DIR}/tests"
  "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(test_si PRIVATE t64_core)
t64_target_warnings(test_si)
add_dependencies(test_si t64-boot-fixtures)
add_test(NAME unit.core.si.atomic-queue-full
  COMMAND test_si --suite atomic)
add_test(NAME unit.core.cpu.si-store-budget
  COMMAND test_si --case cpu-store-budget)
add_test(NAME unit.core.si.device
  COMMAND test_si --suite device)
add_test(NAME unit.core.input.latch-partition
  COMMAND test_si --suite latch)
set_tests_properties(unit.core.si.atomic-queue-full
  unit.core.cpu.si-store-budget unit.core.si.device
  unit.core.input.latch-partition PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")

# Phase 9 save media tests (plan 09-04): EEPROM on channel 4, SRAM window and
# DMA, selection, refusals, the save-media digest section.
add_executable(test_save tests/test_save.c)
target_include_directories(test_save PRIVATE
  "${CMAKE_SOURCE_DIR}/tests"
  "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(test_save PRIVATE t64_core)
t64_target_warnings(test_save)
add_dependencies(test_save t64-boot-fixtures)
add_test(NAME unit.core.save.device
  COMMAND test_save --suite device)
set_tests_properties(unit.core.save.device PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")

# Phase 9 save import and export tests (plan 09-05): the D-24 check order, the
# byte-order selections, postconditions and replacement of the core save API.
add_executable(test_save_transfer tests/test_save_transfer.c)
target_include_directories(test_save_transfer PRIVATE
  "${CMAKE_SOURCE_DIR}/tests"
  "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(test_save_transfer PRIVATE t64_core t64_sha256)
t64_target_warnings(test_save_transfer)
add_dependencies(test_save_transfer t64-boot-fixtures)
add_test(NAME unit.core.save.import-export
  COMMAND test_save_transfer)
set_tests_properties(unit.core.save.import-export PROPERTIES
  LABELS unit
  TIMEOUT 120
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")

add_executable(test_joybus tests/test_joybus.c)
target_include_directories(test_joybus PRIVATE
  "${CMAKE_SOURCE_DIR}/tests"
  "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(test_joybus PRIVATE t64_core)
t64_target_warnings(test_joybus)
add_dependencies(test_joybus t64-boot-fixtures)
add_test(NAME unit.core.joybus.parser
  COMMAND test_joybus --suite parser)
set_tests_properties(unit.core.joybus.parser PROPERTIES
  LABELS unit
  ENVIRONMENT "T64_TEST_IPL3_PATH=${T64_BOOT_FIXTURE_DIR}/ipl3_compat.z64")

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
add_test(NAME unit.core.rsp-tracer COMMAND test_sp_dma --rsp-tracer)
set_tests_properties(unit.core.rsp-tracer PROPERTIES LABELS unit)

add_executable(test_rsp_control tests/test_rsp_control.c)
target_include_directories(test_rsp_control PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_rsp_control PRIVATE t64_core)
t64_target_warnings(test_rsp_control)
add_test(NAME unit.core.rsp-control COMMAND test_rsp_control)
set_tests_properties(unit.core.rsp-control PROPERTIES LABELS unit)

add_executable(test_rsp_scalar tests/test_rsp_scalar.c)
target_include_directories(test_rsp_scalar PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_rsp_scalar PRIVATE t64_core)
t64_target_warnings(test_rsp_scalar)
add_test(NAME unit.core.rsp-scalar COMMAND test_rsp_scalar)
set_tests_properties(unit.core.rsp-scalar PROPERTIES LABELS unit)

add_executable(test_rsp_transfers tests/test_rsp_transfers.c)
target_include_directories(test_rsp_transfers PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_rsp_transfers PRIVATE t64_core)
t64_target_warnings(test_rsp_transfers)
add_test(NAME unit.core.rsp-transfers COMMAND test_rsp_transfers)
set_tests_properties(unit.core.rsp-transfers PROPERTIES LABELS unit)

add_executable(test_rsp_vector tests/test_rsp_vector.c)
target_include_directories(test_rsp_vector PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_rsp_vector PRIVATE t64_core)
t64_target_warnings(test_rsp_vector)
add_test(NAME unit.core.rsp-vector COMMAND test_rsp_vector)
set_tests_properties(unit.core.rsp-vector PROPERTIES LABELS unit)

add_executable(test_rsp_multiply tests/test_rsp_multiply.c)
target_include_directories(test_rsp_multiply PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_rsp_multiply PRIVATE t64_core)
t64_target_warnings(test_rsp_multiply)
add_test(NAME unit.core.rsp-multiply COMMAND test_rsp_multiply)
set_tests_properties(unit.core.rsp-multiply PROPERTIES LABELS unit)

add_executable(test_rsp_compare tests/test_rsp_compare.c)
target_include_directories(test_rsp_compare PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_rsp_compare PRIVATE t64_core)
t64_target_warnings(test_rsp_compare)
add_test(NAME unit.core.rsp-compare COMMAND test_rsp_compare)
set_tests_properties(unit.core.rsp-compare PROPERTIES LABELS unit)

add_executable(test_rsp_divider tests/test_rsp_divider.c)
target_include_directories(test_rsp_divider PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(test_rsp_divider PRIVATE t64_core)
t64_target_warnings(test_rsp_divider)
add_test(NAME unit.core.rsp-divider COMMAND test_rsp_divider)
set_tests_properties(unit.core.rsp-divider PROPERTIES LABELS unit)

add_executable(test_rsp_fuzz tests/test_rsp_fuzz.c)
target_include_directories(test_rsp_fuzz PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(test_rsp_fuzz PRIVATE t64_core)
t64_target_warnings(test_rsp_fuzz)
add_test(NAME unit.core.rsp-fuzz COMMAND test_rsp_fuzz)
set_tests_properties(unit.core.rsp-fuzz PROPERTIES LABELS unit TIMEOUT 60)
add_test(NAME harness.rsp-divider-audit
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/tests/test_rsp_divider_audit.py)
set_tests_properties(harness.rsp-divider-audit PROPERTIES
  LABELS runner TIMEOUT 60
  ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

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

  # The joybus bounded campaign is a case of its gate-preset name; only the
  # iteration count is raised here (the gate preset keeps the default).
  set_property(TEST unit.core.joybus.parser APPEND PROPERTY ENVIRONMENT
    "T64_JOYBUS_FUZZ_ITERATIONS=250000")
  set_tests_properties(unit.core.joybus.parser PROPERTIES TIMEOUT 600)

  # The state import test hashes a 6.3 MB container several times per input and
  # sanitizer instrumentation multiplies that about 22 times, so the sanitizer
  # preset raises only the hostile-input counts (sweep samples and the seeded
  # campaign) and the timeout; the full boundary matrix adds no memory-safety
  # coverage over the sampled one and stays a native option
  # (T64_STATE_MATRIX_FULL=1), run as one recorded step of the Phase 9 native
  # qualification (plan 09-10, L4 WR-01). Plan 09-09.1 and 09-10 record the
  # measured seconds.
  set_property(TEST unit.core.state.import APPEND PROPERTY ENVIRONMENT
    "T64_STATE_FUZZ_ITERATIONS=120;T64_STATE_SWEEP_HEADER=12;T64_STATE_SWEEP_BODY=12;T64_STATE_SWEEP_BLOBS=2")
  # TIMEOUT: 1,339.5 s measured under the sanitizer on the fix tree (plan 09-10,
  # 1,198 to 1,339 s over three runs at load 8 to 14), so 1800 was 1.34 times;
  # 2400 is 1.8 times, and the qualifier's wrapper (scripts/qualify-rdp-vi.py)
  # stays above the largest TIMEOUT registered here, which a pinned relation checks.
  set_tests_properties(unit.core.state.import PROPERTIES TIMEOUT 2400)

  # Phase 10 plan 10-12: the two state 26 import names, measured under the
  # sanitizer in the plan (PIN-IMPACT.md section 2.3): about 20 times their
  # native seconds. The registered gate TIMEOUT of 300 s already exceeds the
  # measured seconds; these raise the margin for a loaded host to 7 and 5 times.
  set_tests_properties(unit.core.state.import-v26-roundtrip PROPERTIES
    TIMEOUT 600)
  set_tests_properties(unit.core.state.import-v26-hostile PROPERTIES
    TIMEOUT 900)

  add_executable(sanitizer_fault tests/sanitizer_fault.c)
  t64_target_warnings(sanitizer_fault)
  add_test(NAME sanitizer.negative-control
    COMMAND sh "${CMAKE_SOURCE_DIR}/scripts/expect-sanitizer.sh"
      "$<TARGET_FILE:sanitizer_fault>")
  set_tests_properties(sanitizer.negative-control PROPERTIES LABELS sanitizer)
endif()
