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
