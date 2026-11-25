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

# repo.api-init-guard (plan 11-10): every declaration, brace initializer,
# compound literal and memset of a public in/out struct of the header uses the
# T64_*_INIT form (a caller-filled leading struct_size is not visible to the
# compiler otherwise). --exclude-oracle-bound skips the ten oracle-bound test
# files until plan 11-16 removes the flag; the rule set and the exempt types
# are in the script header.
add_test(NAME repo.api-init-guard
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/check-api-init.py
          --exclude-oracle-bound
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(repo.api-init-guard PROPERTIES
  LABELS "repo;phase11"
  TIMEOUT 60
  COST 1
  ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

# repo.api-init-guard-selftest: seeded sources in a temporary tree under build/,
# one per rule (uninitialized, positional, zero, compound literal, memset in
# value and pointer form, array forms, descriptor forms, exempt types, members,
# the oracle-bound flag and the fixed exclusion) must be reported or accepted
# exactly as seeded.
add_test(NAME repo.api-init-guard-selftest
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/check-api-init.py --selftest
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(repo.api-init-guard-selftest PROPERTIES
  LABELS "repo;phase11"
  TIMEOUT 60
  COST 1
  ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
