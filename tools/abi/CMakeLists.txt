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
