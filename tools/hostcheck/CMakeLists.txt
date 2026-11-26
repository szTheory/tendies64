# Script-only CTest registrations of the minimal host (Phase 12, plan 12-07, API-05,
# decisions DR-10 and DR-27). The host itself is the standalone project of host/:
# it is never part of this build (the root CMakeLists.txt holds no host/ path and
# the gate's build does not build it); these names only run scripts/check-host.sh,
# which configures host/ against the moved prefix of install.setup and runs one
# unit case of the pump with a fake clock.
#
# host.pump.build is the setup test of the fixture t64_host and requires t64_install,
# so every host.pump.* name also pulls in install.setup in a -R match (the count of
# ^host\.pump\. is the names plus one). The cases that run the title guest need the
# authored title ROM, which the gate build produces through
# t64-installed-consumer-fixtures: REQUIRED_FILES makes a missing ROM show as Not Run
# naming the path, never as a pump failure. COST is the measured native seconds
# (W-11); TIMEOUT is a literal and follows F-108 (at least 3.5 times the native
# seconds and at least twice the COST).
set(T64_HOST_TITLE_ROM "${CMAKE_SOURCE_DIR}/build/fixtures/phase12-title-s32k.z64")

add_test(NAME host.pump.build
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-host.sh
          --build ${CMAKE_BINARY_DIR} build
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(host.pump.build PROPERTIES
  LABELS "host;phase12"
  TIMEOUT 120
  COST 0.50
  FIXTURES_REQUIRED t64_install
  FIXTURES_SETUP t64_host)

# host.pump.drain-order: the title's short script through the pump with a fake clock;
# every drain pass delivers video, then audio, then text, one pass holds all three,
# the counts equal the oracle's and the first video packet equals the expected frame.
add_test(NAME host.pump.drain-order
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-host.sh
          --build ${CMAKE_BINARY_DIR} run drain-order
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(host.pump.drain-order PROPERTIES
  LABELS "host;phase12"
  TIMEOUT 60
  COST 0.45
  FIXTURES_REQUIRED t64_host
  REQUIRED_FILES "${T64_HOST_TITLE_ROM}")

# host.pump.clock: the tick conversion (0, 15 and 16 ns, the maximum), the carry kept
# from the origin across steps of any size, a backward clock refused without advancing,
# the max_ticks cap and a bound that never falls behind the guest tick.
add_test(NAME host.pump.clock
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-host.sh
          --build ${CMAKE_BINARY_DIR} run clock
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(host.pump.clock PROPERTIES
  LABELS "host;phase12"
  TIMEOUT 60
  COST 0.12
  FIXTURES_REQUIRED t64_host
  REQUIRED_FILES "${T64_HOST_TITLE_ROM}")

# host.pump.stall: a sink that refuses everything ends a step with "stall without
# progress" after one retry and loses nothing once it accepts again, a sink that refuses
# once lets the step finish, a step after the guest completed is TERMINAL and advances and
# queues nothing, and a video buffer smaller than the packet is an API error.
add_test(NAME host.pump.stall
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-host.sh
          --build ${CMAKE_BINARY_DIR} run stall
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(host.pump.stall PROPERTIES
  LABELS "host;phase12"
  TIMEOUT 60
  COST 1.40
  FIXTURES_REQUIRED t64_host
  REQUIRED_FILES "${T64_HOST_TITLE_ROM}")

# host.pump.title-two-speeds (plan 12-08, SC2, decisions DR-11, DR-13 and DR-35): the title's
# committed t64.input scripts through the pump's scripted source with a fake clock at two
# speeds (240 and 3 Hz of fake time: the clock advances 1e9 / hz ns per step). The binary
# prints one JSON line per script and speed (per-packet video digests, audio packet and
# stream digests, the exported save digest, the transcript digest, the completion tick, the
# stop reason and the final state digest); scripts/check-host-title.py, run by
# check-host.sh, requires them byte-identical across the speeds and equal to
# fixtures/title/title.expected.json and to the recorded pins of title.runner-pins.json.
# No time figure appears in any line.
add_test(NAME host.pump.title-two-speeds
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-host.sh
          --build ${CMAKE_BINARY_DIR} run title-two-speeds
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(host.pump.title-two-speeds PROPERTIES
  LABELS "host;phase12"
  TIMEOUT 60
  COST 3.00
  FIXTURES_REQUIRED t64_host
  REQUIRED_FILES "${T64_HOST_TITLE_ROM}")

# host.pump.input-map (plan 12-08, decision DR-12): the logical input mapping table, over
# every int16 axis value, every mapped and unmapped control, the left trigger threshold and a
# removed pad. No guest, no ROM, no device.
add_test(NAME host.pump.input-map
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-host.sh
          --build ${CMAKE_BINARY_DIR} run input-map
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(host.pump.input-map PROPERTIES
  LABELS "host;phase12"
  TIMEOUT 60
  COST 0.02
  FIXTURES_REQUIRED t64_host)

# host.pump.frames (plan 12-08, decision DR-14, T-12-26): t64h_frame_copy honors stride_bytes
# and the destination pitch, refuses a packet over 320 x 240 RGBA, a short source, a short
# destination (left untouched), a bad geometry and another encoding, and the S16BE sample
# swap. No guest, no ROM, no device.
add_test(NAME host.pump.frames
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-host.sh
          --build ${CMAKE_BINARY_DIR} run frames
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(host.pump.frames PROPERTIES
  LABELS "host;phase12"
  TIMEOUT 60
  COST 0.02
  FIXTURES_REQUIRED t64_host)

# host.pump.script (plan 12-08, decision DR-13, T-12-24 and T-12-25): the strict t64.input
# v1 reader (a seeded refusal per rule, the size and event caps), the committed title
# scripts against the events python3 reads from the same files, the scripted source against
# a model of a two-entry queue (lazy queueing, QUEUE_FULL deferral) and against the title
# (nothing is queued after the guest completed; one short real-core run, so the ROM is a
# required file as for the other cases that run the guest).
add_test(NAME host.pump.script
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-host.sh
          --build ${CMAKE_BINARY_DIR} run script
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(host.pump.script PROPERTIES
  LABELS "host;phase12"
  TIMEOUT 60
  COST 0.38
  FIXTURES_REQUIRED t64_host
  REQUIRED_FILES "${T64_HOST_TITLE_ROM}")

# repo.host-include-guard (plan 12-07, T-12-22, decision DR-16): the pump and its tests
# below host/ include only <tendies64/t64_machine.h>, C99 headers and local headers
# (rule HOST-INCLUDE), no SDL3 include or SDL_ identifier outside host/sdl (rules
# HOST-SDL-INCLUDE and HOST-SDL-IDENT), no <time.h> in the pump (HOST-PUMP-TIME), and the
# host CMake files name no source-tree path (HOST-CMAKE). Script-only, no fixture.
add_test(NAME repo.host-include-guard
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-host-includes.sh
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(repo.host-include-guard PROPERTIES
  LABELS "repo;phase12"
  TIMEOUT 30
  COST 0.05)

# repo.host-include-guard-selftest: seeded copies of the committed host (an internal
# header, another tendies64 header, a relative include out of host/, an SDL3 include and
# an SDL_ identifier in the pump and its tests, <time.h> in the pump, source-tree paths in
# the CMake file), the clean controls and the empty-scan error must be reported or
# accepted exactly as seeded, one rule id each.
add_test(NAME repo.host-include-guard-selftest
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-host-includes.sh --selftest
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(repo.host-include-guard-selftest PROPERTIES
  LABELS "repo;phase12"
  TIMEOUT 60
  COST 1.15)

# repo.host-isolation (plan 12-07, SC3, T-12-23, decision DR-16): the core, runner, tools,
# tests and the root CMakeLists.txt hold no SDL3 package, target, include or identifier
# (the tokens are listed in scripts/check-host-isolation.sh, which this file must not
# repeat), the root CMakeLists.txt names no host path, the core archive of this build tree
# (the file that cmake --install copies) and the installed copy hold no SDL symbol, and
# docs/abi/installed-undefined.txt names none. --with-selftest runs the seeded trees and
# archives first.
add_test(NAME repo.host-isolation
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-host-isolation.sh
          --build ${CMAKE_BINARY_DIR} --with-selftest
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(repo.host-isolation PROPERTIES
  LABELS "repo;phase12"
  TIMEOUT 60
  COST 1.30)

# repo.host-policy (plan 12-09, SC3, T-12-28 to T-12-31, decisions DR-16 to DR-18): the host
# files below host/ fetch, build, vendor or install no SDL3 source (no FetchContent, no
# ExternalProject, no add_subdirectory of an SDL, third_party or vendor path, no third_party or
# vendor directory, no sdl*.c or sdl*.h file, no install rule), carry no application bundle,
# icon or resource file, show no string that holds the product name, create the window with
# exactly the title "t64 minimal host (development)" and use no renderer, GPU path or keyboard
# event. Script-only, no fixture. The shell itself and its smoke lane are never a CTest name:
# the local lane of scripts/check-host.sh builds them outside this build tree.
add_test(NAME repo.host-policy
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-host-policy.sh
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(repo.host-policy PROPERTIES
  LABELS "repo;phase12"
  TIMEOUT 30
  COST 0.08)

# repo.host-policy-selftest: seeded copies of the host (one seed per rule and per way of
# breaking it, 30 failing cases with their rule ids) and the clean controls (the exact title
# through the macro and the literal, comments and string contents that name forbidden things,
# the package lines of the CMake file, the shell's own directory); a rule without a seeded
# case, or an empty scan, fails the selftest.
add_test(NAME repo.host-policy-selftest
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-host-policy.sh --selftest
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(repo.host-policy-selftest PROPERTIES
  LABELS "repo;phase12"
  TIMEOUT 60
  COST 1.60)

# repo.p12-mutant-anchors (plan 12-08, decision DR-32): the mutant table of the Phase 12 host
# pump and title (8 pump mutants, 4 title mutants and 2 controls of scripts/p12-mutants.py):
# every edit anchor occurs exactly once in its tracked file, every killing test is registered
# above and runs a case of the host test binary. The mutant procedure itself (--run, about 12
# host or ROM rebuilds in a throwaway worktree) is a check of the review clone of plan 12-12
# and a qualifier stage, never a CTest name.
add_test(NAME repo.p12-mutant-anchors
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/p12-mutants.py --check-anchors
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(repo.p12-mutant-anchors PROPERTIES
  LABELS "repo;phase12"
  TIMEOUT 60
  COST 0.10
  ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

# repo.api07-evaluation (plan 12-10, API-07, decision DR-22, threats T-12-33 to T-12-35): the API-07
# evaluation guard. The written fit evaluation under docs/evidence is pinned to one 40-hex commit
# that equals the pin of the Phase 12 PIN-IMPACT record, holds the conclusion "no adapter now" in
# its Conclusion section, claims no support of the other project, and no tracked file below core,
# runner, host, tools or tests names that project (no adapter code, no build target, no test).
# --with-selftest first runs the seeded trees (one failing seed per rule, clean controls, a git
# tree that ignores untracked files and the empty-scan error). Script-only, no fixture.
add_test(NAME repo.api07-evaluation
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-api07-evaluation.sh --with-selftest
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(repo.api07-evaluation PROPERTIES
  LABELS "repo;phase12"
  TIMEOUT 30
  COST 1.25)
