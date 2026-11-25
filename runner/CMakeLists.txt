# Runner executables are host tools, not core: they may use POSIX. On macOS,
# _POSIX_C_SOURCE alone hides O_NOFOLLOW and mkdtemp (measured, Apple clang 21),
# so _DARWIN_C_SOURCE is added there; Linux exposes both under 200809L.
set(T64_HOST_DEFS _POSIX_C_SOURCE=200809L)
if(APPLE)
  list(APPEND T64_HOST_DEFS _DARWIN_C_SOURCE)
endif()

# Bind each real runner binary to its actual local source/build inputs. The
# manifest is generated at configure time and refreshed whenever an input
# changes; no absolute path or timestamp enters the identity.
set(T64_RUNNER_BUILD_INPUTS
  ${CMAKE_CURRENT_SOURCE_DIR}/t64_real.c
  ${CMAKE_CURRENT_SOURCE_DIR}/t64_real_result.c
  ${CMAKE_CURRENT_SOURCE_DIR}/t64_real_result.h
  ${CMAKE_CURRENT_SOURCE_DIR}/t64_real_fixture.c
  ${CMAKE_CURRENT_SOURCE_DIR}/t64_real_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/t64_input.c
  ${CMAKE_CURRENT_SOURCE_DIR}/t64_input.h
  ${CMAKE_CURRENT_SOURCE_DIR}/t64_verified_input.h
  ${CMAKE_CURRENT_SOURCE_DIR}/t64_audio_observation.h
  ${CMAKE_CURRENT_SOURCE_DIR}/t64_video_observation.h
  ${CMAKE_CURRENT_SOURCE_DIR}/t64_raw_capture.c
  ${CMAKE_CURRENT_SOURCE_DIR}/t64_raw_capture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/t64_artifact_publisher.c
  ${CMAKE_CURRENT_SOURCE_DIR}/t64_artifact_publisher.h
  ${CMAKE_CURRENT_SOURCE_DIR}/t64_replay.c
  ${CMAKE_CURRENT_SOURCE_DIR}/t64_replay.h
  ${CMAKE_CURRENT_SOURCE_DIR}/t64_save_flags.h
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  ${T64_RUNNER_BUILD_INPUTS})
string(CONCAT T64_RUNNER_BUILD_MANIFEST
  "format=t64-runner-v1\n"
  "project=${PROJECT_VERSION}\n"
  "compiler=${CMAKE_C_COMPILER_ID}:${CMAKE_C_COMPILER_VERSION}\n"
  "platform=${CMAKE_SYSTEM_NAME}:${CMAKE_SYSTEM_PROCESSOR}\n"
  "build_type=${CMAKE_BUILD_TYPE}\n"
  "c_standard=${CMAKE_C_STANDARD}:${CMAKE_C_STANDARD_REQUIRED}:${CMAKE_C_EXTENSIONS}\n"
  "c_flags=${CMAKE_C_FLAGS}\n"
  "warnings_as_errors=${T64_WERROR}\n")
foreach(T64_RUNNER_INPUT IN LISTS T64_RUNNER_BUILD_INPUTS)
  file(SHA256 "${T64_RUNNER_INPUT}" T64_RUNNER_INPUT_SHA256)
  file(RELATIVE_PATH T64_RUNNER_INPUT_RELATIVE "${CMAKE_SOURCE_DIR}"
    "${T64_RUNNER_INPUT}")
  string(APPEND T64_RUNNER_BUILD_MANIFEST
    "${T64_RUNNER_INPUT_RELATIVE}=${T64_RUNNER_INPUT_SHA256}\n")
endforeach()
string(SHA256 T64_RUNNER_PROD_BUILD_FINGERPRINT
  "${T64_RUNNER_BUILD_MANIFEST}testing=0\n")
string(SHA256 T64_RUNNER_TEST_BUILD_FINGERPRINT
  "${T64_RUNNER_BUILD_MANIFEST}testing=1\n")

# Host-side closed logical-input schedule parser.
add_library(t64_input STATIC t64_input.c)
target_include_directories(t64_input PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(t64_input PUBLIC t64_common)
t64_target_warnings(t64_input)

# Shared local CPU diagnostic protocol used by the production adapter and its
# authored parser/aggregation controls.
add_library(t64_diag_common STATIC t64_diag_common.c)
target_include_directories(t64_diag_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(t64_diag_common PUBLIC t64_common)
t64_target_warnings(t64_diag_common)

add_library(t64_cpu_diag_protocol STATIC t64_cpu_diag_protocol.c)
target_include_directories(t64_cpu_diag_protocol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(t64_cpu_diag_protocol PUBLIC t64_diag_common)
t64_target_warnings(t64_cpu_diag_protocol)

add_library(t64_rsp_diag_protocol STATIC t64_rsp_diag_protocol.c)
target_include_directories(t64_rsp_diag_protocol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(t64_rsp_diag_protocol PUBLIC t64_diag_common)
t64_target_warnings(t64_rsp_diag_protocol)

add_library(t64_base_diag_protocol STATIC t64_base_diag_protocol.c)
target_include_directories(t64_base_diag_protocol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(t64_base_diag_protocol PUBLIC t64_diag_common t64_cpu_diag_protocol)
t64_target_warnings(t64_base_diag_protocol)

add_executable(test_base_diag_protocol tests/test_base_diag_protocol.c)
target_link_libraries(test_base_diag_protocol PRIVATE t64_base_diag_protocol)
t64_target_warnings(test_base_diag_protocol)
add_test(NAME runner.base-diagnostic-protocol COMMAND test_base_diag_protocol)
set_tests_properties(runner.base-diagnostic-protocol PROPERTIES
  LABELS runner TIMEOUT 60)

# Full private BASE pre-use verification is exercised only with authored,
# owner-only fixtures. It must pass before normal dispatch can create a run dir.
add_test(NAME harness.base-qualification
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/tests/test_base_qualification.py)
set_tests_properties(harness.base-qualification PROPERTIES
  LABELS runner TIMEOUT 180
  ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

add_test(NAME harness.base-image
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/tests/test_base_image.py)
set_tests_properties(harness.base-image PROPERTIES
  LABELS runner TIMEOUT 60 ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

add_test(NAME harness.base-lanes
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/tests/test_base_lanes.py)
set_tests_properties(harness.base-lanes PROPERTIES
  LABELS runner TIMEOUT 60 ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

add_executable(t64-base-diagnostic t64_base_diagnostic.c)
target_compile_definitions(t64-base-diagnostic PRIVATE
  ${T64_HOST_DEFS}
  T64_BASE_PRIVATE_ROOT=\"${CMAKE_SOURCE_DIR}/build/private-base-diagnostic\")
target_link_libraries(t64-base-diagnostic PRIVATE
  t64_base_diag_protocol t64_cpu_diag_protocol t64_verified_input
  t64_common t64_core)
t64_target_warnings(t64-base-diagnostic)

# Authored host controls can redirect private outputs and exercise a short
# supervisor deadline. Neither hook is compiled into the production adapter.
add_executable(t64-base-diagnostic-controls t64_base_diagnostic.c)
target_compile_definitions(t64-base-diagnostic-controls PRIVATE
  ${T64_HOST_DEFS} T64_BASE_DIAGNOSTIC_TESTING=1
  T64_BASE_PRIVATE_ROOT=\"${CMAKE_SOURCE_DIR}/build/private-base-diagnostic\")
target_link_libraries(t64-base-diagnostic-controls PRIVATE
  t64_base_diag_protocol t64_cpu_diag_protocol t64_verified_input t64_common t64_core)
t64_target_warnings(t64-base-diagnostic-controls)

add_test(NAME harness.base-classification
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/tests/test_base_classification.py)
set_tests_properties(harness.base-classification PROPERTIES
  LABELS runner TIMEOUT 60
  ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1;T64_BASE_TEST_ADAPTER=$<TARGET_FILE:t64-base-diagnostic>")

add_test(NAME harness.base-run
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/tests/test_base_run.py)
set_tests_properties(harness.base-run PROPERTIES
  LABELS runner TIMEOUT 180
  ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1;T64_BASE_CONTROL_ADAPTER=$<TARGET_FILE:t64-base-diagnostic-controls>")

add_test(NAME harness.base-evidence
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/tests/test_base_evidence.py)
set_tests_properties(harness.base-evidence PROPERTIES
  LABELS runner TIMEOUT 60
  ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

add_executable(test_cpu_diag_protocol tests/test_cpu_diag_protocol.c)
target_link_libraries(test_cpu_diag_protocol PRIVATE t64_cpu_diag_protocol)
t64_target_warnings(test_cpu_diag_protocol)
add_test(NAME runner.cpu-diagnostic-protocol COMMAND test_cpu_diag_protocol)
set_tests_properties(runner.cpu-diagnostic-protocol PROPERTIES
  LABELS runner TIMEOUT 60)
add_test(NAME runner.cpu-diagnostic-inventory
  COMMAND test_cpu_diag_protocol --inventory)
set_tests_properties(runner.cpu-diagnostic-inventory PROPERTIES
  LABELS runner TIMEOUT 60)

add_executable(test_rsp_diag_protocol tests/test_rsp_diag_protocol.c)
target_link_libraries(test_rsp_diag_protocol PRIVATE t64_rsp_diag_protocol)
t64_target_warnings(test_rsp_diag_protocol)
add_test(NAME unit.rsp-diag-protocol COMMAND test_rsp_diag_protocol --protocol)
set_tests_properties(unit.rsp-diag-protocol PROPERTIES
  LABELS runner TIMEOUT 60)

# Authored, headless controls for the local-only RSP qualification lifecycle.
# Fixtures are created beneath owner-only build/test-rsp-* roots and never use
# the private external source/toolchain receipts.
add_test(NAME harness.rsp-qualification
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/tests/test_rsp_qualification.py)
add_test(NAME harness.rsp-build
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/tests/test_rsp_build.py)
add_test(NAME harness.rsp-run
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/tests/test_rsp_run.py)
set_tests_properties(harness.rsp-qualification harness.rsp-build harness.rsp-run PROPERTIES
  LABELS runner TIMEOUT 120
  ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

# The evidence contract remains readable after GSD moves completed phase data.
add_test(NAME harness.rsp-evidence-ownership
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/tests/test_rsp_evidence.py
    RspEvidenceTests.test_owned_tables_survive_milestone_archive)
set_tests_properties(harness.rsp-evidence-ownership PROPERTIES
  LABELS runner TIMEOUT 60 ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

add_test(NAME repo.rsp-no-hle
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/tests/test_rsp_no_hle.py)
set_tests_properties(repo.rsp-no-hle PROPERTIES
  LABELS runner TIMEOUT 240
  ENVIRONMENT
    "PYTHONDONTWRITEBYTECODE=1;T64_RSP_NO_HLE_BUILD_DIR=${CMAKE_BINARY_DIR}")
# Phase 10 GATE-01: hosted macOS seconds (F-111) so a runner without cost history
# starts the longest tests first; COST orders only (see core/CMakeLists.txt).
set_tests_properties(repo.rsp-no-hle PROPERTIES COST 48)

# Phase 9 state field-use audit (plan 09-09.1, D-45): every state member used as
# an index, length, shift or loop bound maps to a closed importer rule and a
# named reader of core/t64_state.c; the self-test seeds each fail for their own
# reason. The case-inventory entry names the build-directory copy.
set(T64_STATE_FIELD_AUDIT_COPY
  "${CMAKE_BINARY_DIR}/runner/check_state_field_use")
configure_file(
  "${CMAKE_SOURCE_DIR}/scripts/check-state-field-use.py"
  "${T64_STATE_FIELD_AUDIT_COPY}" COPYONLY)
file(CHMOD "${T64_STATE_FIELD_AUDIT_COPY}"
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
              WORLD_READ WORLD_EXECUTE)
add_test(NAME harness.state-field-use-audit
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/check-state-field-use.py
          --check --selftest --root ${CMAKE_SOURCE_DIR})
set_tests_properties(harness.state-field-use-audit PROPERTIES
  LABELS runner TIMEOUT 240 ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
# Phase 10 GATE-01: hosted macOS seconds (F-111), see repo.rsp-no-hle above.
set_tests_properties(harness.state-field-use-audit PROPERTIES COST 71)

# The version-membership and reservation site list is data; each later plan
# refreshes it in the commit that adds its site.
add_test(NAME repo.si-membership-sites
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/list-membership-sites.py
          --check ${CMAKE_SOURCE_DIR}/docs/spec/profile-membership-sites.txt)
set_tests_properties(repo.si-membership-sites PROPERTIES
  LABELS runner TIMEOUT 60 ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

# The mutant table of the CORE-02 matrix (plan 09-06): every edit anchor must
# still occur exactly once in its tracked file, so a refactor cannot silently
# retire a mutant. The mutant procedure itself is run on demand, not by CTest.
add_test(NAME repo.sched-k5-anchors
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/k5-mutants.py --check-anchors)
set_tests_properties(repo.sched-k5-anchors PROPERTIES
  LABELS runner TIMEOUT 60 ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

# The mutant table of the Phase 10 VG, VID and CORE tests (plan 10-01): every
# edit anchor must still occur exactly once in its tracked file. The mutant
# procedure itself (--run) is run on demand per wave, not by CTest.
add_test(NAME repo.p10-mutant-anchors
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/p10-mutants.py --check-anchors)
set_tests_properties(repo.p10-mutant-anchors PROPERTIES
  LABELS "runner;phase10" TIMEOUT 60 ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

add_executable(test_cpu_diagnostic tests/test_cpu_diagnostic.c)
target_compile_definitions(test_cpu_diagnostic PRIVATE ${T64_HOST_DEFS})
target_include_directories(test_cpu_diagnostic PRIVATE ${CMAKE_SOURCE_DIR}/core)
target_link_libraries(test_cpu_diagnostic PRIVATE
  t64_cpu_diag_protocol t64_verified_input t64_common t64_core)
t64_target_warnings(test_cpu_diagnostic)
add_dependencies(test_cpu_diagnostic t64-boot-fixtures)
add_test(NAME runner.cpu-diagnostic
  COMMAND test_cpu_diagnostic $<TARGET_FILE:t64-cpu-diagnostic>
          $<TARGET_FILE:t64-cpu-diagnostic-test-hooks>
          ${CMAKE_SOURCE_DIR})
set_tests_properties(runner.cpu-diagnostic PROPERTIES
  LABELS runner TIMEOUT 240
  ENVIRONMENT
    "T64_TEST_IPL3_PATH=${CMAKE_SOURCE_DIR}/build/fixtures/ipl3_compat.z64")

add_executable(test_input tests/test_input.c)
target_compile_definitions(test_input PRIVATE ${T64_HOST_DEFS})
target_link_libraries(test_input PRIVATE t64_input t64_core)
t64_target_warnings(test_input)
add_dependencies(test_input t64-boot-fixtures)
add_test(NAME controls.real.input-partition COMMAND test_input)
set_tests_properties(controls.real.input-partition PROPERTIES
  LABELS controls
  TIMEOUT 120
  ENVIRONMENT
    "T64_TEST_IPL3_PATH=${CMAKE_SOURCE_DIR}/build/fixtures/ipl3_compat.z64;T64_TEST_HELLO_PATH=${CMAKE_SOURCE_DIR}/build/fixtures/hello.z64;T64_BOOT_FIXTURES_DIR=${CMAKE_SOURCE_DIR}/build/fixtures")

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

add_library(t64_verified_input STATIC t64_verified_input.c)
target_compile_definitions(t64_verified_input PRIVATE ${T64_HOST_DEFS})
target_include_directories(t64_verified_input PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(t64_verified_input PRIVATE t64_common)
t64_target_warnings(t64_verified_input)

# Separate local CPU adapter; its CLI and authored controls share strict
# protocol, immutable snapshots, and the canonical machine implementation.
add_executable(t64-cpu-diagnostic t64_cpu_diagnostic.c)
target_compile_definitions(t64-cpu-diagnostic PRIVATE ${T64_HOST_DEFS})
target_link_libraries(t64-cpu-diagnostic PRIVATE
  t64_cpu_diag_protocol t64_verified_input t64_common t64_core)
t64_target_warnings(t64-cpu-diagnostic)

# A second copy of the adapter exposes a pipe barrier only to the authored
# replacement control. The executable used by normal production paths has no
# test hook; both targets share the same core/protocol implementation.
add_executable(t64-cpu-diagnostic-test-hooks t64_cpu_diagnostic.c)
target_compile_definitions(t64-cpu-diagnostic-test-hooks PRIVATE
  ${T64_HOST_DEFS} T64_CPU_DIAGNOSTIC_TESTING)
target_link_libraries(t64-cpu-diagnostic-test-hooks PRIVATE
  t64_cpu_diag_protocol t64_verified_input t64_common t64_core)
t64_target_warnings(t64-cpu-diagnostic-test-hooks)
add_dependencies(test_cpu_diagnostic
  t64-cpu-diagnostic t64-cpu-diagnostic-test-hooks)

# Local RSP adapter shares the closed inventory/batch protocol and immutable
# input snapshot path. The production target has no authored test hooks.
add_executable(t64-rsp-diagnostic t64_rsp_diagnostic.c)
target_compile_definitions(t64-rsp-diagnostic PRIVATE ${T64_HOST_DEFS})
target_include_directories(t64-rsp-diagnostic PRIVATE ${CMAKE_SOURCE_DIR}/core)
target_link_libraries(t64-rsp-diagnostic PRIVATE
  t64_rsp_diag_protocol t64_verified_input t64_common t64_core)
t64_target_warnings(t64-rsp-diagnostic)

# Rights-cleared authored CPU/SP/RSP smoke consumer. It takes only the exact
# ROM digest produced by the local authored fixture builder.
add_executable(t64-rsp-smoke t64_rsp_smoke.c)
target_compile_definitions(t64-rsp-smoke PRIVATE ${T64_HOST_DEFS})
target_include_directories(t64-rsp-smoke PRIVATE ${CMAKE_SOURCE_DIR}/core)
target_link_libraries(t64-rsp-smoke PRIVATE t64_common t64_core)
t64_target_warnings(t64-rsp-smoke)
add_test(NAME integration.rsp-rom-smoke
  COMMAND ${CMAKE_COMMAND} -E env
    "T64_RSP_SMOKE_RUNNER=$<TARGET_FILE:t64-rsp-smoke>"
    sh ${CMAKE_SOURCE_DIR}/scripts/test-rsp-smoke.sh)
set_tests_properties(integration.rsp-rom-smoke PROPERTIES
  LABELS integration TIMEOUT 120 RUN_SERIAL TRUE
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

# Test-only fault injection stays out of the supervisor's production library.
add_library(t64_verified_input_test STATIC t64_verified_input.c)
target_compile_definitions(t64_verified_input_test PRIVATE
  ${T64_HOST_DEFS} T64_VERIFIED_INPUT_TESTING)
target_include_directories(t64_verified_input_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(t64_verified_input_test PRIVATE t64_common)
t64_target_warnings(t64_verified_input_test)

add_executable(t64-supervisor t64_supervisor.c t64_real_result.c
  t64_real_fixture.c)
target_compile_definitions(t64-supervisor PRIVATE
  ${T64_HOST_DEFS}
  T64_BUILD_ID=\"${PROJECT_VERSION}+synthetic\")
target_link_libraries(t64-supervisor PRIVATE
  t64_classify t64_input t64_common t64_core t64_verified_input)
t64_target_warnings(t64-supervisor)

# First headless real guest producer. It consumes verified inherited snapshots
# and emits a canonical t64.result v2 record.
add_executable(t64-runner-real t64_real.c t64_real_result.c t64_real_fixture.c
  t64_raw_capture.c t64_artifact_publisher.c t64_replay.c)
target_compile_definitions(t64-runner-real PRIVATE ${T64_HOST_DEFS}
  T64_RUNNER_BUILD_ID=\"t64-runner-v1-${T64_RUNNER_PROD_BUILD_FINGERPRINT}\")
target_link_libraries(t64-runner-real PRIVATE
  t64_input t64_common t64_core)
t64_target_warnings(t64-runner-real)
add_dependencies(t64-supervisor t64-runner-real)

# Test-only twin permits bounded post-guest report/process fault injection.
add_executable(t64-runner-test t64_real.c t64_real_result.c t64_real_fixture.c
  t64_raw_capture.c t64_artifact_publisher.c t64_replay.c)
target_compile_definitions(t64-runner-test PRIVATE
  ${T64_HOST_DEFS}
  T64_RUNNER_BUILD_ID=\"t64-runner-v1-${T64_RUNNER_TEST_BUILD_FINGERPRINT}\"
  T64_REAL_RUNNER_TESTING)
target_link_libraries(t64-runner-test PRIVATE
  t64_input t64_common t64_core)
t64_target_warnings(t64-runner-test)

add_executable(test_fixture_descriptor tests/test_fixture_descriptor.c)
target_compile_definitions(test_fixture_descriptor PRIVATE
  ${T64_HOST_DEFS} T64_VERIFIED_INPUT_TESTING)
target_link_libraries(test_fixture_descriptor PRIVATE
  t64_verified_input_test t64_input t64_core)
t64_target_warnings(test_fixture_descriptor)
add_dependencies(test_fixture_descriptor t64-supervisor t64-boot-fixtures)
add_test(NAME runner.fixture-descriptor COMMAND test_fixture_descriptor)
add_test(NAME controls.real.source-replaced
  COMMAND test_fixture_descriptor --replacement $<TARGET_FILE:t64-supervisor>)
set_tests_properties(runner.fixture-descriptor controls.real.source-replaced
  PROPERTIES LABELS controls TIMEOUT 120
  ENVIRONMENT
    "T64_TEST_HELLO_PATH=${CMAKE_SOURCE_DIR}/build/fixtures/hello.z64;T64_TEST_INPUT_PATH=${CMAKE_SOURCE_DIR}/build/fixtures/hello.input.json")
set_tests_properties(controls.real.source-replaced PROPERTIES
  DEPENDS runner.fixture-descriptor)

# Consumer of t64.result v1 records and t64.suite files (plan 01-07).
add_executable(t64-result-check t64_result_check.c t64_real_fixture.c)
target_compile_definitions(t64-result-check PRIVATE ${T64_HOST_DEFS})
target_link_libraries(t64-result-check PRIVATE t64_classify t64_common)
t64_target_warnings(t64-result-check)
add_dependencies(t64-supervisor t64-result-check)

# Test-only supervisor exposes a barrier after both verified snapshots exist,
# before launching the real producer, for synchronized replacement coverage.
add_executable(t64-supervisor-test t64_supervisor.c t64_real_result.c
  t64_real_fixture.c)
target_compile_definitions(t64-supervisor-test PRIVATE
  ${T64_HOST_DEFS} T64_SUPERVISOR_TESTING)
target_link_libraries(t64-supervisor-test PRIVATE
  t64_classify t64_input t64_common t64_core t64_verified_input)
t64_target_warnings(t64-supervisor-test)
add_dependencies(t64-supervisor-test t64-runner-test t64-result-check)

# Real guest negative controls share the production supervisor and strict
# result checker; fixture paths remain test inputs and never reach the child.
add_executable(test_real_controls tests/test_real_controls.c)
target_compile_definitions(test_real_controls PRIVATE ${T64_HOST_DEFS})
target_link_libraries(test_real_controls PRIVATE t64_common t64_core)
t64_target_warnings(test_real_controls)
add_dependencies(test_real_controls
  t64-supervisor t64-result-check t64-supervisor-test t64-boot-fixtures)
add_test(NAME controls.real.empty-halt
  COMMAND test_real_controls $<TARGET_FILE:t64-supervisor>
    $<TARGET_FILE:t64-result-check> empty-halt)
add_test(NAME controls.real.empty-bound
  COMMAND test_real_controls $<TARGET_FILE:t64-supervisor>
    $<TARGET_FILE:t64-result-check> empty-bound)
add_test(NAME controls.real.wrong-transcript
  COMMAND test_real_controls $<TARGET_FILE:t64-supervisor>
    $<TARGET_FILE:t64-result-check> wrong-transcript)
add_test(NAME controls.real.unsupported-opcode
  COMMAND test_real_controls $<TARGET_FILE:t64-supervisor>
    $<TARGET_FILE:t64-result-check> unsupported-opcode)
add_test(NAME controls.real.output-overflow
  COMMAND test_real_controls $<TARGET_FILE:t64-supervisor>
    $<TARGET_FILE:t64-result-check> output-overflow)
add_test(NAME controls.real.identity-mismatch
  COMMAND test_real_controls $<TARGET_FILE:t64-supervisor>
    $<TARGET_FILE:t64-result-check> identity-mismatch)
set_tests_properties(
  controls.real.empty-halt
  controls.real.empty-bound
  controls.real.wrong-transcript
  controls.real.unsupported-opcode
  controls.real.output-overflow
  controls.real.identity-mismatch PROPERTIES
  LABELS controls TIMEOUT 120
  ENVIRONMENT
    "T64_TEST_FIXTURE_DIR=${CMAKE_SOURCE_DIR}/build/fixtures;T64_TEST_INPUT_PATH=${CMAKE_SOURCE_DIR}/build/fixtures/hello.input.json")

# Actual supervised CPU-authored DPC fill and raw VI packet, with a same-setup
# pixel negative control and a replacement-fixture admission refusal.
add_executable(test_rdp_video tests/test_rdp_video.c)
target_compile_definitions(test_rdp_video PRIVATE ${T64_HOST_DEFS})
target_include_directories(test_rdp_video PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_rdp_video PRIVATE t64_common)
t64_target_warnings(test_rdp_video)
add_dependencies(test_rdp_video t64-supervisor t64-supervisor-test
  t64-result-check t64-rdp-fill-fixture)
add_test(NAME runner.rdp-fill
  COMMAND test_rdp_video accepted-cpu-fill-to-raw-frame
    $<TARGET_FILE:t64-supervisor> $<TARGET_FILE:t64-result-check>
    ${CMAKE_SOURCE_DIR}/build/fixtures/rdp-fill.z64
    ${CMAKE_SOURCE_DIR}/build/fixtures/hello.input.json)
add_test(NAME controls.real.rdp-drain-cadence
  COMMAND test_rdp_video drain-cadence-real-runner
    $<TARGET_FILE:t64-supervisor-test> $<TARGET_FILE:t64-result-check>
    ${CMAKE_SOURCE_DIR}/build/fixtures/rdp-fill.z64
    ${CMAKE_SOURCE_DIR}/build/fixtures/hello.input.json)
add_test(NAME controls.real.rdp-wrong-pixel
  COMMAND test_rdp_video same-setup-wrong-pixel
    $<TARGET_FILE:t64-supervisor> $<TARGET_FILE:t64-result-check>
    ${CMAKE_SOURCE_DIR}/build/fixtures/rdp-fill.z64
    ${CMAKE_SOURCE_DIR}/build/fixtures/hello.input.json)
add_test(NAME controls.real.rdp-replacement-fixture
  COMMAND test_rdp_video replacement-fixture-refused
    $<TARGET_FILE:t64-supervisor> $<TARGET_FILE:t64-result-check>
    ${CMAKE_SOURCE_DIR}/build/fixtures/rdp-fill.z64
    ${CMAKE_SOURCE_DIR}/build/fixtures/hello.input.json)
set_tests_properties(
  runner.rdp-fill
  controls.real.rdp-drain-cadence
  controls.real.rdp-wrong-pixel
  controls.real.rdp-replacement-fixture PROPERTIES
  LABELS "runner;controls" TIMEOUT 120)

# Authored CPU PCM two-DMA trace through the supervised runner and raw APIs.
add_executable(test_audio_capture tests/test_audio_capture.c)
target_compile_definitions(test_audio_capture PRIVATE ${T64_HOST_DEFS})
target_include_directories(test_audio_capture PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/core/include)
target_link_libraries(test_audio_capture PRIVATE t64_common)
t64_target_warnings(test_audio_capture)
add_dependencies(test_audio_capture t64-supervisor t64-supervisor-test
  t64-result-check t64-audio-fixture t64-rdp-fill-fixture)
add_test(NAME runner.audio-pcm
  COMMAND test_audio_capture runner-literal-pcm
    $<TARGET_FILE:t64-supervisor> $<TARGET_FILE:t64-result-check>
    ${CMAKE_SOURCE_DIR}/build/fixtures/audio-pcm.z64
    ${CMAKE_SOURCE_DIR}/build/fixtures/hello.input.json)
add_test(NAME controls.real.audio-wrong-sample
  COMMAND test_audio_capture reached-wrong-sample
    $<TARGET_FILE:t64-supervisor> $<TARGET_FILE:t64-result-check>
    ${CMAKE_SOURCE_DIR}/build/fixtures/audio-pcm-wrong.z64
    ${CMAKE_SOURCE_DIR}/build/fixtures/hello.input.json)
add_test(NAME controls.real.audio-drain-cadence
  COMMAND test_audio_capture runner-drain-pacing
    $<TARGET_FILE:t64-supervisor-test> $<TARGET_FILE:t64-result-check>
    ${CMAKE_SOURCE_DIR}/build/fixtures/audio-pcm.z64
    ${CMAKE_SOURCE_DIR}/build/fixtures/hello.input.json)
add_test(NAME runner.raw-capture
  COMMAND test_audio_capture actual-av-bundle
    $<TARGET_FILE:t64-supervisor> $<TARGET_FILE:t64-supervisor-test>
    $<TARGET_FILE:t64-result-check>
    ${CMAKE_SOURCE_DIR}/build/fixtures/audio-pcm.z64
    ${CMAKE_SOURCE_DIR}/build/fixtures/hello.input.json
    ${CMAKE_SOURCE_DIR}/build/fixtures/rdp-fill.z64)
add_test(NAME controls.real.raw-capture-errors
  COMMAND test_audio_capture short-write-and-eintr
    $<TARGET_FILE:t64-supervisor-test> $<TARGET_FILE:t64-result-check>
    ${CMAKE_SOURCE_DIR}/build/fixtures/audio-pcm.z64
    ${CMAKE_SOURCE_DIR}/build/fixtures/hello.input.json)
add_test(NAME controls.real.raw-capture-target-errors
  COMMAND test_audio_capture capture-target-refusals
    $<TARGET_FILE:t64-supervisor-test> $<TARGET_FILE:t64-result-check>
    ${CMAKE_SOURCE_DIR}/build/fixtures/audio-pcm.z64
    ${CMAKE_SOURCE_DIR}/build/fixtures/hello.input.json)
add_test(NAME controls.real.raw-capture-io-errors
  COMMAND test_audio_capture io-errors
    $<TARGET_FILE:t64-supervisor> $<TARGET_FILE:t64-supervisor-test>
    $<TARGET_FILE:t64-result-check>
    ${CMAKE_SOURCE_DIR}/build/fixtures/audio-pcm.z64
    ${CMAKE_SOURCE_DIR}/build/fixtures/hello.input.json)
add_test(NAME controls.real.raw-capture-caps
  COMMAND test_audio_capture cap-boundaries
    $<TARGET_FILE:t64-supervisor-test> $<TARGET_FILE:t64-result-check>
    ${CMAKE_SOURCE_DIR}/build/fixtures/audio-pcm.z64
    ${CMAKE_SOURCE_DIR}/build/fixtures/hello.input.json)
add_test(NAME controls.real.raw-capture-incomplete
  COMMAND test_audio_capture capture-incomplete
    $<TARGET_FILE:t64-supervisor-test> $<TARGET_FILE:t64-result-check>
    ${CMAKE_SOURCE_DIR}/build/fixtures/audio-pcm.z64
    ${CMAKE_SOURCE_DIR}/build/fixtures/hello.input.json)
add_test(NAME controls.real.raw-capture-interrupted
  COMMAND test_audio_capture capture-interrupted
    $<TARGET_FILE:t64-supervisor-test> $<TARGET_FILE:t64-result-check>
    ${CMAKE_SOURCE_DIR}/build/fixtures/audio-pcm.z64
    ${CMAKE_SOURCE_DIR}/build/fixtures/hello.input.json)
add_test(NAME controls.real.raw-capture-race
  COMMAND test_audio_capture capture-race
    $<TARGET_FILE:t64-supervisor-test> $<TARGET_FILE:t64-result-check>
    ${CMAKE_SOURCE_DIR}/build/fixtures/audio-pcm.z64
    ${CMAKE_SOURCE_DIR}/build/fixtures/hello.input.json)
set_tests_properties(runner.audio-pcm controls.real.audio-wrong-sample
  controls.real.audio-drain-cadence runner.raw-capture
  PROPERTIES LABELS "runner;controls" TIMEOUT 120
  ENVIRONMENT "T64_AUDIO_EVIDENCE_DIR=${CMAKE_BINARY_DIR}/08-01-evidence;T64_REAL_KEEP_WORKSET=1")
set_tests_properties(runner.raw-capture PROPERTIES
  ENVIRONMENT "T64_AUDIO_EVIDENCE_DIR=${CMAKE_BINARY_DIR}/08-04-evidence;T64_REAL_KEEP_WORKSET=1")
set_tests_properties(controls.real.raw-capture-errors PROPERTIES
  LABELS "controls;runner" TIMEOUT 120
  ENVIRONMENT "T64_AUDIO_EVIDENCE_DIR=${CMAKE_BINARY_DIR}/08-04-evidence;T64_REAL_KEEP_WORKSET=1")
set_tests_properties(controls.real.raw-capture-target-errors PROPERTIES
  LABELS "controls;runner" TIMEOUT 120
  ENVIRONMENT "T64_AUDIO_EVIDENCE_DIR=${CMAKE_BINARY_DIR}/08-04-evidence;T64_REAL_KEEP_WORKSET=1")
set_tests_properties(controls.real.raw-capture-io-errors PROPERTIES
  LABELS "controls;runner" TIMEOUT 120
  ENVIRONMENT "T64_AUDIO_EVIDENCE_DIR=${CMAKE_BINARY_DIR}/08-04-evidence;T64_REAL_KEEP_WORKSET=1")
set_tests_properties(controls.real.raw-capture-caps PROPERTIES
  LABELS "controls;runner" TIMEOUT 120
  ENVIRONMENT "T64_AUDIO_EVIDENCE_DIR=${CMAKE_BINARY_DIR}/08-04-evidence;T64_REAL_KEEP_WORKSET=1")
set_tests_properties(controls.real.raw-capture-incomplete PROPERTIES
  LABELS "controls;runner" TIMEOUT 120
  ENVIRONMENT "T64_AUDIO_EVIDENCE_DIR=${CMAKE_BINARY_DIR}/08-04-evidence;T64_REAL_KEEP_WORKSET=1")
set_tests_properties(controls.real.raw-capture-interrupted PROPERTIES
  LABELS "controls;runner" TIMEOUT 120
  ENVIRONMENT "T64_AUDIO_EVIDENCE_DIR=${CMAKE_BINARY_DIR}/08-04-evidence;T64_REAL_KEEP_WORKSET=1")
set_tests_properties(controls.real.raw-capture-race PROPERTIES
  LABELS "controls;runner" TIMEOUT 120
  ENVIRONMENT "T64_AUDIO_EVIDENCE_DIR=${CMAKE_BINARY_DIR}/08-04-evidence;T64_REAL_KEEP_WORKSET=1")

# Phase 9 runner mechanism controls (per-fixture cap and pin, bounded argv,
# admit ROMs, artifact pair, table checks). One binary, one CTest name per case.
add_executable(test_phase09_runner tests/test_phase09_runner.c
  t64_real_fixture.c)
target_compile_definitions(test_phase09_runner PRIVATE ${T64_HOST_DEFS})
target_include_directories(test_phase09_runner PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/core/include)
target_link_libraries(test_phase09_runner PRIVATE t64_common t64_core)
t64_target_warnings(test_phase09_runner)
add_dependencies(test_phase09_runner t64-supervisor t64-supervisor-test
  t64-result-check t64-boot-fixtures t64-audio-fixture
  t64-phase09-admit-fixture)
function(t64_phase09_runner_case TEST_NAME CASE_NAME)
  add_test(NAME ${TEST_NAME}
    COMMAND test_phase09_runner ${CASE_NAME}
      $<TARGET_FILE:t64-supervisor> $<TARGET_FILE:t64-supervisor-test>
      $<TARGET_FILE:t64-result-check> ${CMAKE_SOURCE_DIR})
  set_tests_properties(${TEST_NAME} PROPERTIES
    LABELS "runner;controls" TIMEOUT 120
    ENVIRONMENT "T64_PHASE09_EVIDENCE_DIR=${CMAKE_BINARY_DIR}/09-02-evidence")
endfunction()
t64_phase09_runner_case(runner.phase09-tracer tracer-artifact)
t64_phase09_runner_case(runner.phase09-admit admit-ids)
t64_phase09_runner_case(controls.real.phase09-admit-wrong-transcript admit-wrong-transcript)
t64_phase09_runner_case(runner.guest-tick-cap tick-cap)
t64_phase09_runner_case(runner.fixture-transcript-pin transcript-pin)
t64_phase09_runner_case(runner.supervisor-argv-bound argv-bound)
t64_phase09_runner_case(harness.fsize-artifact-real-fault fsize-artifact)
t64_phase09_runner_case(harness.fsize-report-capture-still-bounded fsize-report-capture)
t64_phase09_runner_case(runner.profile-table-id-length profile-table)

# Phase 9 state container export through the real supervised runner (plan
# 09-08): one binary, one CTest name, every case in order.
add_executable(test_state_runner tests/test_state_runner.c)
target_compile_definitions(test_state_runner PRIVATE ${T64_HOST_DEFS})
target_include_directories(test_state_runner PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/core/include)
target_link_libraries(test_state_runner PRIVATE t64_common t64_core)
t64_target_warnings(test_state_runner)
add_dependencies(test_state_runner t64-supervisor t64-supervisor-test
  t64-runner-real t64-runner-test t64-result-check t64-boot-fixtures
  t64-phase09-admit-fixture)
add_test(NAME runner.state-export
  COMMAND test_state_runner all
    $<TARGET_FILE:t64-supervisor> $<TARGET_FILE:t64-supervisor-test>
    $<TARGET_FILE:t64-result-check> ${CMAKE_SOURCE_DIR})
set_tests_properties(runner.state-export PROPERTIES
  LABELS "runner;controls" TIMEOUT 480
  ENVIRONMENT "T64_PHASE09_EVIDENCE_DIR=${CMAKE_BINARY_DIR}/09-08-evidence;T64_STATE_DECODER=${CMAKE_SOURCE_DIR}/fixtures/state/decode_container.py")

# The RDP fill builder recompiles the shared ROM packer in place on every run,
# and the audio, admit and pad builders only run that packer. Built
# concurrently (as ninja does with parallel jobs) a consumer can find the file
# mid-rewrite and fail with "Permission denied", so every consumer builds after
# the RDP fill target. Hosted run 44631218639 hit this on Linux.
add_dependencies(t64-audio-fixture t64-rdp-fill-fixture)
add_dependencies(t64-phase09-admit-fixture t64-rdp-fill-fixture)
add_dependencies(t64-phase09-pad-fixture t64-rdp-fill-fixture)
add_dependencies(t64-phase09-replay-fixture t64-rdp-fill-fixture)
add_dependencies(t64-phase09-save-fixture t64-rdp-fill-fixture)
add_dependencies(t64-phase10-composite-fixture t64-rdp-fill-fixture)

# Phase 9 pad-poll tracer (plan 09-03): the authored pad-poll ROM through the
# real supervised runner, with the in-process poll timing measurement.
add_executable(test_pad_poll tests/test_pad_poll.c)
target_compile_definitions(test_pad_poll PRIVATE ${T64_HOST_DEFS})
target_include_directories(test_pad_poll PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/core/include)
target_link_libraries(test_pad_poll PRIVATE t64_common t64_core)
t64_target_warnings(test_pad_poll)
add_dependencies(test_pad_poll t64-supervisor t64-supervisor-test
  t64-result-check t64-boot-fixtures t64-phase09-pad-fixture)
add_test(NAME runner.pad-poll
  COMMAND test_pad_poll --suite accepted
    $<TARGET_FILE:t64-supervisor> $<TARGET_FILE:t64-supervisor-test>
    $<TARGET_FILE:t64-result-check> ${CMAKE_SOURCE_DIR})
add_test(NAME controls.real.pad-wrong-value
  COMMAND test_pad_poll --suite controls
    $<TARGET_FILE:t64-supervisor> $<TARGET_FILE:t64-supervisor-test>
    $<TARGET_FILE:t64-result-check> ${CMAKE_SOURCE_DIR})
set_tests_properties(runner.pad-poll controls.real.pad-wrong-value PROPERTIES
  LABELS "runner;controls" TIMEOUT 120
  ENVIRONMENT "T64_PHASE09_EVIDENCE_DIR=${CMAKE_BINARY_DIR}/09-03-evidence")

# Phase 9 replay (plan 09-07): the authored replay-mix ROM, the replay module
# tests (suite unit, in process) and the real supervised record and play
# controls (suite real).
add_executable(test_replay tests/test_replay.c t64_replay.c)
target_compile_definitions(test_replay PRIVATE ${T64_HOST_DEFS})
target_include_directories(test_replay PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/core/include)
target_link_libraries(test_replay PRIVATE t64_input t64_common t64_core)
t64_target_warnings(test_replay)
add_dependencies(test_replay t64-supervisor t64-supervisor-test
  t64-result-check t64-boot-fixtures t64-phase09-replay-fixture)
add_test(NAME unit.replay.suite COMMAND test_replay --suite unit)
add_test(NAME runner.replay-mix
  COMMAND test_replay --suite real
    $<TARGET_FILE:t64-supervisor> $<TARGET_FILE:t64-supervisor-test>
    $<TARGET_FILE:t64-result-check> ${CMAKE_SOURCE_DIR})
set_tests_properties(unit.replay.suite PROPERTIES LABELS unit TIMEOUT 300)
set_tests_properties(runner.replay-mix PROPERTIES LABELS "runner;controls"
  TIMEOUT 300)
set_tests_properties(unit.replay.suite runner.replay-mix PROPERTIES
  ENVIRONMENT "T64_REPLAY_MIX_ROM_PATH=${CMAKE_SOURCE_DIR}/build/fixtures/phase09-replay-mix-si-v1.z64;T64_REPLAY_MIX_INPUT_PATH=${CMAKE_SOURCE_DIR}/fixtures/replay/mix.input.json;T64_REPLAY_EVIDENCE_DIR=${CMAKE_BINARY_DIR}/09-07-evidence")

# Phase 9 save fixtures (plan 09-04): the authored eeprom-rw and sram-rw ROMs
# through the real supervised runner on the explicit -e4k, -e16k and -s32k ids.
add_executable(test_save_fixtures tests/test_save_fixtures.c)
target_compile_definitions(test_save_fixtures PRIVATE ${T64_HOST_DEFS})
target_include_directories(test_save_fixtures PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/core/include)
target_link_libraries(test_save_fixtures PRIVATE t64_common t64_core)
t64_target_warnings(test_save_fixtures)
add_dependencies(test_save_fixtures t64-supervisor t64-supervisor-test
  t64-result-check t64-runner-real t64-boot-fixtures t64-phase09-save-fixture)
add_test(NAME runner.save-fixtures
  COMMAND test_save_fixtures --suite accepted
    $<TARGET_FILE:t64-supervisor> $<TARGET_FILE:t64-supervisor-test>
    $<TARGET_FILE:t64-result-check> ${CMAKE_SOURCE_DIR}
    $<TARGET_FILE:t64-runner-real>)
add_test(NAME controls.real.save-wrong-byte
  COMMAND test_save_fixtures --suite controls
    $<TARGET_FILE:t64-supervisor> $<TARGET_FILE:t64-supervisor-test>
    $<TARGET_FILE:t64-result-check> ${CMAKE_SOURCE_DIR}
    $<TARGET_FILE:t64-runner-real>)
set_tests_properties(runner.save-fixtures controls.real.save-wrong-byte PROPERTIES
  LABELS "runner;controls" TIMEOUT 120
  ENVIRONMENT "T64_PHASE09_EVIDENCE_DIR=${CMAKE_BINARY_DIR}/09-04-evidence")

# Phase 9 save import and export (plan 09-05): the authored save-roundtrip ROM
# through the real supervised runner with a verified import descriptor and the
# no-clobber export publisher. One CTest name runs every case of the binary.
add_executable(test_save_roundtrip tests/test_save_roundtrip.c)
target_compile_definitions(test_save_roundtrip PRIVATE ${T64_HOST_DEFS})
target_include_directories(test_save_roundtrip PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/core/include)
target_link_libraries(test_save_roundtrip PRIVATE t64_common t64_core)
t64_target_warnings(test_save_roundtrip)
add_dependencies(test_save_roundtrip t64-supervisor t64-supervisor-test
  t64-result-check t64-runner-real t64-boot-fixtures t64-phase09-save-fixture)
add_test(NAME runner.save-roundtrip
  COMMAND test_save_roundtrip
    $<TARGET_FILE:t64-supervisor> $<TARGET_FILE:t64-supervisor-test>
    $<TARGET_FILE:t64-result-check> ${CMAKE_SOURCE_DIR}
    $<TARGET_FILE:t64-runner-real>)
set_tests_properties(runner.save-roundtrip PROPERTIES
  LABELS "runner;controls" TIMEOUT 300
  ENVIRONMENT "T64_PHASE09_EVIDENCE_DIR=${CMAKE_BINARY_DIR}/09-05-evidence")

# Phase 10 composite guest (plan 10-15, RUN-07): two supervised runs of the
# authored composite ROM (a raw-capture run and a save-out run) with equal final
# digest, every output against the independent expectation, wrong-value
# controls and the container v2 positive path. One binary, one CTest name per
# case; the evidence directory is per test name and scoped per process, so the
# names are safe under ctest -j.
add_executable(test_composite_two_run tests/test_composite_two_run.c)
target_compile_definitions(test_composite_two_run PRIVATE ${T64_HOST_DEFS})
target_include_directories(test_composite_two_run PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/core/include)
target_link_libraries(test_composite_two_run PRIVATE t64_common t64_core)
t64_target_warnings(test_composite_two_run)
add_dependencies(test_composite_two_run t64-supervisor t64-supervisor-test
  t64-result-check t64-runner-real t64-boot-fixtures
  t64-phase10-composite-fixture)
function(t64_phase10_composite_case TEST_NAME CASE_NAME)
  add_test(NAME ${TEST_NAME}
    COMMAND test_composite_two_run --case ${CASE_NAME}
      $<TARGET_FILE:t64-supervisor> $<TARGET_FILE:t64-supervisor-test>
      $<TARGET_FILE:t64-result-check> ${CMAKE_SOURCE_DIR}
      $<TARGET_FILE:t64-runner-real>)
  set_tests_properties(${TEST_NAME} PROPERTIES
    LABELS "runner;controls;phase10" TIMEOUT 120
    ENVIRONMENT "T64_PHASE10_EVIDENCE_DIR=${CMAKE_BINARY_DIR}/10-15-evidence;T64_COMPOSITE_EXPECTED=${CMAKE_SOURCE_DIR}/fixtures/composite/composite.expected.json;T64_STATE_DECODER=${CMAKE_SOURCE_DIR}/fixtures/state/decode_container_v2.py")
endfunction()
t64_phase10_composite_case(runner.composite-two-run two-run)
t64_phase10_composite_case(controls.real.composite-wrong-rom wrong-rom)
t64_phase10_composite_case(runner.composite-state-v2 state-v2)
# TIMEOUTs (F-108): the helper's literal 120 s against native 0.4 to 0.8 s and
# sanitizer 17.5 s (two-run), 6.4 s (wrong-rom) and 4.5 s (state-v2) at load 12,
# the lowest margin being 6.9 times the sanitizer figure.
# The committed expectation equals the independent oracle byte for byte, so a
# runner test that reads it compares against values the script derived.
add_test(NAME harness.composite-oracle
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/fixtures/composite/composite_oracle.py
    --check ${CMAKE_SOURCE_DIR}/fixtures/composite/composite.expected.json)
set_tests_properties(harness.composite-oracle PROPERTIES
  LABELS "harness;phase10" TIMEOUT 60 ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

# Phase 10 container format strings in the runner contract (plan 10-15,
# CORE-06): the closed set {v1, v2} at the supervisor and the runner parser, the
# profile pairing (v1 with state 25, v2 with state 26) at the runner prelaunch,
# every wrong pair refused at guest tick 0. One binary, one CTest name, every
# case in order.
add_executable(test_state_format_v26 tests/test_state_format_v26.c)
target_compile_definitions(test_state_format_v26 PRIVATE ${T64_HOST_DEFS})
target_include_directories(test_state_format_v26 PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/core/include)
target_link_libraries(test_state_format_v26 PRIVATE t64_common t64_core)
t64_target_warnings(test_state_format_v26)
add_dependencies(test_state_format_v26 t64-supervisor t64-supervisor-test
  t64-result-check t64-runner-real t64-boot-fixtures t64-audio-fixture
  t64-phase09-admit-fixture t64-phase10-composite-fixture)
add_test(NAME runner.state-format-v26
  COMMAND test_state_format_v26
    $<TARGET_FILE:t64-supervisor> $<TARGET_FILE:t64-supervisor-test>
    $<TARGET_FILE:t64-result-check> ${CMAKE_SOURCE_DIR}
    $<TARGET_FILE:t64-runner-real>)
# TIMEOUT by the F-108 rule: 1.4 to 2.0 s native, 33.9 s under the sanitizer
# (load 12), so 300 s is 8.9 times the sanitizer figure; the container blob is
# written and hashed six times in one process.
set_tests_properties(runner.state-format-v26 PROPERTIES
  LABELS "runner;controls;phase10" TIMEOUT 300
  ENVIRONMENT "T64_PHASE10_EVIDENCE_DIR=${CMAKE_BINARY_DIR}/10-15-evidence/state-format")

# Phase 10 qualifier controls (plan 10-17): hermetic python cases of
# scripts/qualify-phase10.py in scratch git repositories with stub stage tables;
# no build and no nested CTest run here (F-113). TIMEOUT by the F-108 rule: about
# 5 s native (the cases are git and interpreter starts, not instrumented code, so
# the sanitizer preset runs them at the same speed) and under 20 s on a hosted
# runner at 3.5 times, so 120 s is more than 20 times the native figure.
add_test(NAME harness.phase10-qualification
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/tests/test_phase10_qualification.py
    --selftest)
set_tests_properties(harness.phase10-qualification PROPERTIES
  LABELS "harness;qualification;phase10" TIMEOUT 120)

# Publisher controls: the module is compiled with the testing hooks, like the
# test twin of the runner, and never reaches the production runner.
add_executable(test_artifact_publisher tests/test_artifact_publisher.c
  t64_artifact_publisher.c)
target_compile_definitions(test_artifact_publisher PRIVATE
  ${T64_HOST_DEFS} T64_REAL_RUNNER_TESTING)
target_include_directories(test_artifact_publisher PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_artifact_publisher PRIVATE t64_common)
t64_target_warnings(test_artifact_publisher)
add_test(NAME runner.artifact-publisher
  COMMAND test_artifact_publisher publish-and-refusals)
add_test(NAME controls.artifact-substitution
  COMMAND test_artifact_publisher substitution)
set_tests_properties(runner.artifact-publisher controls.artifact-substitution
  PROPERTIES LABELS "runner;controls" TIMEOUT 120
  ENVIRONMENT "T64_PHASE09_EVIDENCE_DIR=${CMAKE_BINARY_DIR}/09-02-evidence")

# Phase 9 harness guard and qualifier-stub controls (python, no builds).
add_test(NAME harness.phase09-qualification
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/tests/test_phase09_qualification.py
    --selftest)
set_tests_properties(harness.phase09-qualification PROPERTIES
  LABELS "harness;qualification" TIMEOUT 120)
# Phase 10 GATE-01: hosted macOS seconds (F-111), see repo.rsp-no-hle above.
set_tests_properties(harness.phase09-qualification PROPERTIES COST 35)
# The cross-platform controls built from the two real records of the first
# hosted pair (docs/evidence/phase09-first-hosted-pair.json).
add_test(NAME harness.phase09-collect-real
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/tests/test_phase09_qualification.py
    --suite collect-real)
set_tests_properties(harness.phase09-collect-real PROPERTIES
  LABELS "harness;qualification" TIMEOUT 120)
# The same file, two named subsets: the stub refuses every evidence mode, and
# the upload allowlist admits exactly one manifest.json.
add_test(NAME harness.phase09-stub-fails-closed
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/tests/test_phase09_qualification.py
    --suite stub)
add_test(NAME harness.upload-allowlist-artifacts
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/tests/test_phase09_qualification.py
    --suite allowlist)
set_tests_properties(harness.phase09-stub-fails-closed
  harness.upload-allowlist-artifacts PROPERTIES
  LABELS "harness;qualification" TIMEOUT 120)

# Original-record audio evidence controls. This selftest runs only isolated
# verifier mutations; hosted emission remains a separate post-gate command.
set(T64_AUDIO_QUALIFICATION_TEST
  "${CMAKE_BINARY_DIR}/runner/test_audio_qualification")
configure_file(
  "${CMAKE_SOURCE_DIR}/scripts/tests/test_audio_qualification.py"
  "${T64_AUDIO_QUALIFICATION_TEST}" COPYONLY)
file(CHMOD "${T64_AUDIO_QUALIFICATION_TEST}"
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
              WORLD_READ WORLD_EXECUTE)
add_test(NAME harness.audio-qualification
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/tests/test_audio_qualification.py
    --selftest)
set_tests_properties(harness.audio-qualification PROPERTIES
  LABELS "harness;qualification" TIMEOUT 120)

# Original-record and raw-byte qualification stays in the normal gate. Its
# subprocess controls run the actual accepted consumer before mutation checks.
add_test(NAME harness.rdp-vi-qualification
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/tests/test_rdp_vi_qualification.py)
set_tests_properties(harness.rdp-vi-qualification PROPERTIES
  LABELS runner TIMEOUT 120
  ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

# Oracle authority is a separate fail-closed gate, not differential evidence.
add_test(NAME harness.rdp-oracle-authority
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/tests/test_rdp_oracle_authority.py)
set_tests_properties(harness.rdp-oracle-authority PROPERTIES
  LABELS runner TIMEOUT 120
  ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

# Descriptor-attested actual IPL3/hello positive integration.
add_executable(test_real_runner tests/test_real_runner.c)
target_compile_definitions(test_real_runner PRIVATE ${T64_HOST_DEFS})
target_link_libraries(test_real_runner PRIVATE
  t64_verified_input t64_common t64_core)
add_dependencies(test_real_runner
  t64-runner-real t64-result-check t64-supervisor t64-supervisor-test
  t64-runner-test t64-boot-fixtures)
t64_target_warnings(test_real_runner)
add_test(NAME integration.open-ipl3-hello
  COMMAND test_real_runner
    $<TARGET_FILE:t64-runner-real>
    $<TARGET_FILE:t64-result-check>
    $<TARGET_FILE:t64-supervisor>
    $<TARGET_FILE:t64-supervisor-test>)
set_tests_properties(integration.open-ipl3-hello PROPERTIES
  LABELS integration TIMEOUT 120
  ENVIRONMENT
    "T64_TEST_HELLO_PATH=${CMAKE_SOURCE_DIR}/build/fixtures/hello.z64;T64_TEST_INPUT_PATH=${CMAKE_SOURCE_DIR}/build/fixtures/hello.input.json")

# Bounded cold/T1 startup consumer: this accepts only the cleared authored
# hello and the two exact Phase 6 profile identities. Controls use Tier A;
# the serialized integration probe uses Tier B once and retains its output.
add_executable(t64-cold-boot-probe t64_cold_boot.c)
target_compile_definitions(t64-cold-boot-probe PRIVATE ${T64_HOST_DEFS})
target_include_directories(t64-cold-boot-probe PRIVATE ${CMAKE_SOURCE_DIR}/core)
target_link_libraries(t64-cold-boot-probe PRIVATE t64_common t64_core)
add_dependencies(t64-cold-boot-probe t64-boot-fixtures)
t64_target_warnings(t64-cold-boot-probe)

add_executable(test_cold_boot tests/test_cold_boot.c)
target_compile_definitions(test_cold_boot PRIVATE ${T64_HOST_DEFS})
target_link_libraries(test_cold_boot PRIVATE t64_common)
add_dependencies(test_cold_boot t64-cold-boot-probe t64-boot-fixtures)
t64_target_warnings(test_cold_boot)
add_test(NAME runner.cold-boot-controls
  COMMAND test_cold_boot --controls $<TARGET_FILE:t64-cold-boot-probe>)
set_tests_properties(runner.cold-boot-controls PROPERTIES
  LABELS runner TIMEOUT 120
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  ENVIRONMENT "T64_TEST_HELLO_PATH=${CMAKE_SOURCE_DIR}/build/fixtures/hello.z64")
add_test(NAME integration.cold-startup
  COMMAND test_cold_boot --startup $<TARGET_FILE:t64-cold-boot-probe>
          ${CMAKE_SOURCE_DIR})
set_tests_properties(integration.cold-startup PROPERTIES
  LABELS integration TIMEOUT 120 RUN_SERIAL TRUE
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  ENVIRONMENT "T64_TEST_HELLO_PATH=${CMAKE_SOURCE_DIR}/build/fixtures/hello.z64")
add_test(NAME integration.cold-hello
  COMMAND test_cold_boot --integration $<TARGET_FILE:t64-cold-boot-probe>
          ${CMAKE_SOURCE_DIR})
set_tests_properties(integration.cold-hello PROPERTIES
  LABELS integration TIMEOUT 120 RUN_SERIAL TRUE
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  ENVIRONMENT "T64_TEST_HELLO_PATH=${CMAKE_SOURCE_DIR}/build/fixtures/hello.z64")

add_test(NAME controls.real.open-ipl3-source-replaced
  COMMAND test_real_runner --source-replacement
    $<TARGET_FILE:t64-result-check>
    $<TARGET_FILE:t64-supervisor-test>)
set_tests_properties(controls.real.open-ipl3-source-replaced PROPERTIES
  LABELS controls TIMEOUT 120
  ENVIRONMENT
    "T64_TEST_HELLO_PATH=${CMAKE_SOURCE_DIR}/build/fixtures/hello.z64;T64_TEST_INPUT_PATH=${CMAKE_SOURCE_DIR}/build/fixtures/hello.input.json")

# Strict real t64.result v2 behavior, separate from v1 goldens and model output.
add_executable(test_result_v2 tests/test_result_v2.c)
target_compile_definitions(test_result_v2 PRIVATE ${T64_HOST_DEFS})
add_dependencies(test_result_v2 t64-result-check)
t64_target_warnings(test_result_v2)
add_test(NAME runner.result-v2
  COMMAND test_result_v2 $<TARGET_FILE:t64-result-check>
          ${CMAKE_SOURCE_DIR}/tests/seeded/results/good.json)
set_tests_properties(runner.result-v2 PROPERTIES LABELS runner TIMEOUT 60)

# Native model observations use a distinct schema and never enter t64.result.
add_executable(t64-model-result-check t64_model_result_check.c)
target_compile_definitions(t64-model-result-check PRIVATE ${T64_HOST_DEFS})
target_link_libraries(t64-model-result-check PRIVATE t64_common)
t64_target_warnings(t64-model-result-check)

add_executable(t64-skeleton-probe t64_skeleton_probe.c)
target_compile_definitions(t64-skeleton-probe PRIVATE ${T64_HOST_DEFS})
target_link_libraries(t64-skeleton-probe PRIVATE t64_common t64_core)
t64_target_warnings(t64-skeleton-probe)

add_executable(test_skeleton_probe tests/test_skeleton_probe.c)
target_compile_definitions(test_skeleton_probe PRIVATE ${T64_HOST_DEFS})
target_include_directories(test_skeleton_probe PRIVATE ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(test_skeleton_probe PRIVATE t64_common t64_core)
add_dependencies(test_skeleton_probe
  t64-skeleton-probe t64-model-result-check t64-result-check)
t64_target_warnings(test_skeleton_probe)
add_test(NAME unit.core.profile-result
  COMMAND test_skeleton_probe
    $<TARGET_FILE:t64-skeleton-probe>
    $<TARGET_FILE:t64-model-result-check>
    $<TARGET_FILE:t64-result-check>
    ${CMAKE_SOURCE_DIR}/tests/controls/golden/positive.json
    ${CMAKE_SOURCE_DIR}/tests/seeded/suites/good.jsonl
    ${CMAKE_SOURCE_DIR}/tests/seeded/suites/INVENTORY
    ${CMAKE_SOURCE_DIR}/scripts/check-claims.sh
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR})
set_tests_properties(unit.core.profile-result PROPERTIES LABELS unit
  TIMEOUT 60)

add_executable(test_model_result tests/test_model_result.c)
target_compile_definitions(test_model_result PRIVATE ${T64_HOST_DEFS})
add_dependencies(test_model_result t64-model-result-check)
t64_target_warnings(test_model_result)
add_test(NAME unit.model-result
  COMMAND test_model_result $<TARGET_FILE:t64-model-result-check>
    ${CMAKE_SOURCE_DIR}/tests/controls/golden/positive.json)
set_tests_properties(unit.model-result PROPERTIES LABELS unit TIMEOUT 60)

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
