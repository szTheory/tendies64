# ABI checks of the public header (Phase 11, plans 11-02 and later). Script-only
# CTest registrations, no targets: picked up by the tools glob in the root
# CMakeLists.txt. Every name carries the labels "repo;phase11" and is listed in
# the PHASE11_TESTS block of scripts/check-test-inventory.sh.
#
# repo.abi-header compiles the installed public header alone as C99 and as
# C++17 with the pedantic warning set and -Werror, so a host in either language
# can include it first in a translation unit.
add_test(NAME repo.abi-header
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/check-abi.py header
          --include ${CMAKE_SOURCE_DIR}/core/include
          --header tendies64/t64_machine.h
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(repo.abi-header PROPERTIES
  LABELS "repo;phase11"
  TIMEOUT 60
  COST 1
  ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

# repo.abi-status-classes: every T64_STATUS_* and T64_STOP_REASON_* of the
# header has exactly one row in docs/abi/status-classes.txt with the header's
# value and a class from the closed set; an unclassified, duplicated,
# renumbered or orphan constant fails it.
add_test(NAME repo.abi-status-classes
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/check-abi.py classes
          --header ${CMAKE_SOURCE_DIR}/core/include/tendies64/t64_machine.h
          --table ${CMAKE_SOURCE_DIR}/docs/abi/status-classes.txt
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(repo.abi-status-classes PROPERTIES
  LABELS "repo;phase11"
  TIMEOUT 60
  COST 1
  ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

# repo.abi-selftest: seeds one breakage per case into temporary copies of the
# header and the table (under build/) and fails if classes mode does not report
# each with its own finding text.
add_test(NAME repo.abi-selftest
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/check-abi.py --selftest
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(repo.abi-selftest PROPERTIES
  LABELS "repo;phase11"
  TIMEOUT 60
  COST 1
  ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
