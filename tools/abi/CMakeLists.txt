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

# repo.abi-prefix (plan 11-11, DR-17): the core archive defines exactly the
# public functions of the header plus names that start with t64i_, every entry
# of the generated rename header core/t64_prefix.h names a symbol the archive
# defines, and regenerating the header from the archive changes nothing. A new
# unprefixed internal symbol, a stale entry or a hand edit fails it. The
# command runs on the t64_core archive of the build tree.
add_test(NAME repo.abi-prefix
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/gen-prefix-header.py
          --check ${CMAKE_BINARY_DIR}/core/libt64_core.a
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(repo.abi-prefix PROPERTIES
  LABELS "repo;phase11"
  TIMEOUT 30
  COST 1
  ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

# repo.stale-freeze-text (plan 11-11, DR-18): no production comment, header
# banner or ADR-0002 line may say that the API freeze is, or comes, at Phase 10
# (core/, runner/, tools/ and docs/adr/0002-language-build-and-abi.md; closed
# Phase 9 and Phase 10 records are outside the scan). The deny phrases are in
# the script header.
add_test(NAME repo.stale-freeze-text
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-stale-freeze-text.sh
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(repo.stale-freeze-text PROPERTIES
  LABELS "repo;phase11"
  TIMEOUT 30
  COST 1)

# repo.stale-freeze-text-selftest: seeded files in a temporary tree (the three
# mandatory seeds plus the before, review, case-insensitive and ADR seeds, two
# clean controls and the empty-scan error) must be reported or accepted exactly
# as seeded.
add_test(NAME repo.stale-freeze-text-selftest
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-stale-freeze-text.sh --selftest
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(repo.stale-freeze-text-selftest PROPERTIES
  LABELS "repo;phase11"
  TIMEOUT 30
  COST 1)

# Install-from-build-tree path (plan 11-16, DR-20, VER-05). One CTest fixture
# chain, no ci.yml edit: install.setup installs the Development component of
# this build tree (no rebuild) into a prefix whose path contains spaces and
# moves it; installed.consumer.build configures and builds the independent
# consumer of tests/installed-consumer against the moved prefix (a separate
# top-level project, never add_subdirectory'd); installed.consumer.smoke runs
# its smoke case. The scratch tree is build/<preset>/install-check.
# COST is the measured native seconds of the name (W-11); TIMEOUT follows F-108:
# at least 3.5 times the slowest expected hosted figure, which is the native
# figure times the hosted multiplier 4.3 of Phase 10 section 6.1, and never
# below 30 s, so the preflight timeout audit (TIMEOUT at least 2 times COST)
# holds with a wide margin.
add_test(NAME install.setup
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-install.sh
          --build ${CMAKE_BINARY_DIR} setup
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(install.setup PROPERTIES
  LABELS "install;phase11"
  TIMEOUT 60
  COST 0.05
  FIXTURES_SETUP t64_install)

add_test(NAME installed.consumer.build
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-install.sh
          --build ${CMAKE_BINARY_DIR} build
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(installed.consumer.build PROPERTIES
  LABELS "installed;phase11"
  TIMEOUT 120
  COST 0.43
  FIXTURES_REQUIRED t64_install
  FIXTURES_SETUP t64_consumer)

add_test(NAME installed.consumer.smoke
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-install.sh
          --build ${CMAKE_BINARY_DIR} run smoke
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(installed.consumer.smoke PROPERTIES
  LABELS "installed;phase11"
  TIMEOUT 60
  COST 0.02
  FIXTURES_REQUIRED t64_consumer)
