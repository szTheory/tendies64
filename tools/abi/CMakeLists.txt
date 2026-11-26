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
# compiler otherwise). Since plan 11-16 it scans every root, the oracle-bound
# test files (call-site-only edits of plan 11-14) and the installed consumer
# included; the rule set and the exempt types are in the script header.
add_test(NAME repo.api-init-guard
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/check-api-init.py
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

# repo.consumer-include-guard (plan 11-16, T-11-28): a consumer of the installed
# package below tests/installed-consumer includes only <tendies64/...>, system
# headers of a fixed list and its own local headers, and its CMakeLists names no
# source-tree path (no add_subdirectory, no include_directories, no CMAKE_SOURCE_DIR,
# no ../ and no core/, runner/ or tools/ path). The rule ids are CONSUMER-INCLUDE
# and CONSUMER-CMAKE.
add_test(NAME repo.consumer-include-guard
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-consumer-includes.sh
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(repo.consumer-include-guard PROPERTIES
  LABELS "repo;phase11"
  TIMEOUT 30
  COST 0.07)

# repo.consumer-include-guard-selftest: seeded copies of the committed consumer
# (an internal include, a relative include out of the directory, a CMake path
# into the source tree and more, plus clean controls and the empty-scan error)
# must be reported or accepted exactly as seeded.
add_test(NAME repo.consumer-include-guard-selftest
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-consumer-includes.sh --selftest
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(repo.consumer-include-guard-selftest PROPERTIES
  LABELS "repo;phase11"
  TIMEOUT 60
  COST 1.01)

# Cheap-first ABI checks (plan 11-17, API-03; CONTEXT "Order of work"): each of
# these runs in well under a second and runs before the consumer suite. The
# generated pins of docs/abi/layout.c (sizeof of every public struct, offsetof
# of every member, every integer T64_* constant, the size of every string
# constant) are compiled against the source headers (repo.abi-layout) and
# against the installed headers (install.abi-layout) as C99, C11 and C++17 with
# the pedantic warning set and -Werror, and regenerated and diffed. The
# install.* names require the t64_install fixture of install.setup and read the
# moved prefix of scripts/check-install.sh (BUILD/install-check/moved prefix, a
# path with a space, passed as one argument). COST is the measured native
# seconds (W-11); TIMEOUT stays far above 3.5 times the hosted figure (F-108).
set(T64_INSTALL_MOVED_PREFIX "${CMAKE_BINARY_DIR}/install-check/moved prefix")

add_test(NAME repo.abi-layout
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/check-abi.py layout
          --check ${CMAKE_SOURCE_DIR}/docs/abi/layout.c
          --include ${CMAKE_SOURCE_DIR}/core/include
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(repo.abi-layout PROPERTIES
  LABELS "repo;phase11"
  TIMEOUT 60
  COST 0.3
  ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

# install.abi-header: the installed public header compiles alone as C99 and
# C++17 (pedantic, -Werror), so the shipped file, not only the source one,
# is self-contained.
add_test(NAME install.abi-header
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/check-abi.py header
          --include "${T64_INSTALL_MOVED_PREFIX}/include"
          --header tendies64/t64_machine.h
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(install.abi-header PROPERTIES
  LABELS "install;phase11"
  TIMEOUT 60
  COST 0.15
  FIXTURES_REQUIRED t64_install
  ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

# install.abi-layout: the same pins hold against the installed header, so the
# installed header cannot drift from the source header in any size, offset or
# constant.
add_test(NAME install.abi-layout
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/check-abi.py layout
          --check ${CMAKE_SOURCE_DIR}/docs/abi/layout.c
          --include "${T64_INSTALL_MOVED_PREFIX}/include"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(install.abi-layout PROPERTIES
  LABELS "install;phase11"
  TIMEOUT 60
  COST 0.3
  FIXTURES_REQUIRED t64_install
  ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

# install.abi-symbols (plan 11-17, API-03): the defined global symbols of the
# installed archive (nm -P -g --defined-only, Darwin underscore stripped) that
# do not start with t64i_ equal docs/abi/public-symbols.txt (the 35 public
# functions), every other global starts with t64i_, and no data symbol exists.
add_test(NAME install.abi-symbols
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/check-abi.py symbols
          --check ${CMAKE_SOURCE_DIR}/docs/abi/public-symbols.txt
          --lib "${T64_INSTALL_MOVED_PREFIX}/lib/libt64_core.a"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(install.abi-symbols PROPERTIES
  LABELS "install;phase11"
  TIMEOUT 60
  COST 0.11
  FIXTURES_REQUIRED t64_install
  ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

# install.abs-paths (plan 11-17, T-11-33): no file of the installed tree and no
# byte of the archive contains the source root, the build root or a per-user
# path prefix. The two roots exist only on this command line; a finding names
# them by position (#1 is the source root, #2 the build root), never by value.
add_test(NAME install.abs-paths
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/check-abi.py abspaths
          --prefix "${T64_INSTALL_MOVED_PREFIX}"
          --forbid "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}"
                   /Users/ /home/ /private/ /var/folders
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(install.abs-paths PROPERTIES
  LABELS "install;phase11"
  TIMEOUT 60
  COST 0.07
  FIXTURES_REQUIRED t64_install
  ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
