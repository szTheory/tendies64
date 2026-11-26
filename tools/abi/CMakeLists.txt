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
# renumbered or orphan constant fails it. Since plan 11-20 it also compares the
# hand-written copy of the table that the installed consumer classifies with
# (tests/installed-consumer/t64c_classes.h) with the table: a missing, extra or
# changed class or resume row fails it.
add_test(NAME repo.abi-status-classes
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/check-abi.py classes
          --header ${CMAKE_SOURCE_DIR}/core/include/tendies64/t64_machine.h
          --table ${CMAKE_SOURCE_DIR}/docs/abi/status-classes.txt
          --consumer-table ${CMAKE_SOURCE_DIR}/tests/installed-consumer/t64c_classes.h
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

# repo.abi-freeze-record (plan 11-23, API-03, SC5): the bound freeze-candidate
# record docs/abi/FREEZE-CANDIDATE.md. The check recomputes the four goldens from
# the installed prefix (the SHA-256 of the installed headers after LF
# normalisation against docs/abi/header.sha256, the public symbols of the
# installed archive against docs/abi/public-symbols.txt, the regenerated layout
# pins against docs/abi/layout.c, the status classes against the installed header)
# and compares each golden's own SHA-256, the counts, the versions, the amendment
# log and the padding holes with the machine-read block of the record. Any
# difference prints "freeze-record: KEY differs" and the re-bind procedure: an
# incompatible change is an amendment of the record, never a silent edit. COST is
# the measured native seconds (W-11); TIMEOUT stays far above 3.5 times the hosted
# figure (F-108).
add_test(NAME repo.abi-freeze-record
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/check-abi.py freeze
          --record ${CMAKE_SOURCE_DIR}/docs/abi/FREEZE-CANDIDATE.md
          --prefix "${T64_INSTALL_MOVED_PREFIX}"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(repo.abi-freeze-record PROPERTIES
  LABELS "repo;phase11"
  TIMEOUT 60
  COST 0.16
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

# API-04 on the shipped objects (plan 11-17, T-11-31, T-11-32). The determinism
# lint compiles core sources with its own flags (-fno-stack-protector, no PIC),
# so only the members of the installed archive show what ships. api04.members
# extracts the archive from its own path (the moved prefix, a path with a space:
# RESEARCH Open Question 6) with ar x into a space-free temporary directory, runs
# tools/lint/lint-objects.sh --allow tools/lint/allowlist-installed.txt over all
# members in one invocation, and compares the archive-level undefined set with
# the recorded per-OS rows of docs/abi/installed-undefined.txt, so a new import
# fails even if its name is allowlisted; the control without the installed
# allowlist must name exactly the stack-protector pair on macOS. COST is the
# measured native seconds at host load 15 (two lint runs side by side); TIMEOUT
# is far above 3.5 times the hosted figure (F-108).
add_test(NAME api04.members
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-api04.sh members
          --lib "${T64_INSTALL_MOVED_PREFIX}/lib/libt64_core.a"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(api04.members PROPERTIES
  LABELS "api04;phase11"
  TIMEOUT 180
  COST 6.5
  FIXTURES_REQUIRED t64_install)

# api04.disasm: objdump -d --no-show-raw-insn over every member of the installed
# archive finds no hardware clock or entropy instruction (mrs of CNTVCT, CNTPCT
# or RNDR, rdtsc, rdtscp, rdrand, rdseed, rdpmc), and the scan refuses to pass on
# an empty disassembly.
add_test(NAME api04.disasm
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-api04.sh disasm
          --lib "${T64_INSTALL_MOVED_PREFIX}/lib/libt64_core.a"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(api04.disasm PROPERTIES
  LABELS "api04;phase11"
  TIMEOUT 60
  COST 0.8
  FIXTURES_REQUIRED t64_install)

# api04.headers: the Layer-1 determinism scan (tools/lint/core-lint.sh --files, the
# L1-INCLUDE allowlist and the other Layer-1 rules) over every installed header.
add_test(NAME api04.headers
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-api04.sh headers
          --include "${T64_INSTALL_MOVED_PREFIX}/include"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(api04.headers PROPERTIES
  LABELS "api04;phase11"
  TIMEOUT 60
  COST 0.14
  FIXTURES_REQUIRED t64_install)

# api04.seeded: the checks above can fail. Members plus
# tools/lint/installed-seeds/seed_host_services.c (clock_gettime, fopen,
# getentropy, pthread_create) fail with exactly four L2-UNDEF-SYMBOL lines;
# members plus an object that imports the allowlisted but unrecorded memmove fail
# the expected-set comparison; members plus seed_hw_counter.c fail the disassembly
# scan; an include directory plus seed_banned_include.h fails with
# L1-INCLUDE-BANNED. The seeds live outside tools/lint/selftest, so the per-file
# lint selftest never runs them.
add_test(NAME api04.seeded
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-api04.sh seeded
          --lib "${T64_INSTALL_MOVED_PREFIX}/lib/libt64_core.a"
          --include "${T64_INSTALL_MOVED_PREFIX}/include"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(api04.seeded PROPERTIES
  LABELS "api04;phase11"
  TIMEOUT 180
  COST 6.6
  FIXTURES_REQUIRED t64_install)

# Functional suite of the installed consumer (plan 11-18, VER-05, API-01, API-02).
# The authored composite ROM comes from the existing fixture target; the fixture
# targets are not part of "all" by themselves, so an ALL custom target makes the
# consumer cases independent of which test executables happen to depend on the ROM.
add_custom_target(t64-installed-consumer-fixtures ALL)
add_dependencies(t64-installed-consumer-fixtures t64-phase10-composite-fixture
  t64-supervisor t64-runner-real)
set(T64_CONSUMER_ROM "${CMAKE_SOURCE_DIR}/build/fixtures/phase10-composite-s32k.z64")

# installed.consumer.literals: the constants of the consumer's t64c_composite.h
# equal the committed fixture files (the independent literals of
# composite.expected.json and composite.input.json, and the labeled
# runner-recorded pins of composite.runner-pins.json), the pins file carries
# exactly the label "runner-recorded pin, not independent", the cited Phase 10
# record contains the tick and the digest, and the seeded failures are reported.
add_test(NAME installed.consumer.literals
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/check-consumer-literals.py
          --check --selftest
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(installed.consumer.literals PROPERTIES
  LABELS "installed;phase11"
  TIMEOUT 60
  COST 0.08
  ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

# One CTest name per binary case of tests/installed-consumer/cases.txt, except
# smoke (registered explicitly above, exactly once). COST is the measured native
# seconds of the name (W-11): a case without a recorded cost is a configure error,
# so a new case cannot enter the gate unmeasured. TIMEOUT follows F-108.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/tests/installed-consumer/cases.txt")
file(STRINGS "${CMAKE_SOURCE_DIR}/tests/installed-consumer/cases.txt"
  T64_CONSUMER_CASES REGEX "^[a-z]")
set(T64_CONSUMER_COST_composite 0.13)
set(T64_CONSUMER_COST_ops-lifetime 0.03)
set(T64_CONSUMER_COST_ops-handshake 0.02)
set(T64_CONSUMER_COST_ops-struct-size 0.33)
set(T64_CONSUMER_COST_ops-profile 0.26)
set(T64_CONSUMER_COST_ops-admission 0.37)
set(T64_CONSUMER_COST_ops-input 0.30)
set(T64_CONSUMER_COST_ops-advance 0.65)
set(T64_CONSUMER_COST_ops-reset 0.28)
set(T64_CONSUMER_COST_ops-unload 0.07)
set(T64_CONSUMER_COST_ops-rings 0.45)
set(T64_CONSUMER_COST_ops-text 0.24)
set(T64_CONSUMER_COST_ops-save 0.49)
set(T64_CONSUMER_COST_ops-state 1.10)
set(T64_CONSUMER_COST_ops-queries 0.47)
set(T64_CONSUMER_COST_ops-trace 0.22)
set(T64_CONSUMER_COST_ops-bus 0.35)
set(T64_CONSUMER_COST_ops-poison-query 0.13)
set(T64_CONSUMER_COST_hostloop 0.21)
set(T64_CONSUMER_COST_hostloop-cadence 0.46)
set(T64_CONSUMER_COST_hostloop-delay 0.66)
set(T64_CONSUMER_COST_two-instance 0.30)
set(T64_CONSUMER_COST_sequence 1.20)
foreach(T64_CONSUMER_CASE IN LISTS T64_CONSUMER_CASES)
  if(T64_CONSUMER_CASE STREQUAL "smoke")
    continue()
  endif()
  if(NOT DEFINED T64_CONSUMER_COST_${T64_CONSUMER_CASE})
    message(FATAL_ERROR
      "tests/installed-consumer/cases.txt lists '${T64_CONSUMER_CASE}' with no "
      "measured COST (set T64_CONSUMER_COST_${T64_CONSUMER_CASE} in tools/abi/CMakeLists.txt)")
  endif()
  add_test(NAME installed.consumer.${T64_CONSUMER_CASE}
    COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-install.sh
            --build ${CMAKE_BINARY_DIR} run ${T64_CONSUMER_CASE}
            --rom "${T64_CONSUMER_ROM}"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  set_tests_properties(installed.consumer.${T64_CONSUMER_CASE} PROPERTIES
    LABELS "installed;phase11"
    TIMEOUT 60
    COST ${T64_CONSUMER_COST_${T64_CONSUMER_CASE}}
    FIXTURES_REQUIRED t64_consumer)
endforeach()

# installed.consumer.three-way (plan 11-18, G6): the in-tree runner (the two
# supervised runs of the authored composite ROM of runner.composite-two-run) and
# the installed consumer's composite case agree on every compared value: the final
# state digest, the completion tick, the stop reason and the video packet count
# (runner-recorded pins, labeled so, never independent) and the video, audio, save
# and transcript bytes (which the consumer separately matches to the independent
# literals in the same run). The runner is not re-pointed at the installed package:
# it is the in-tree binary of this build tree. COST is the measured native seconds.
add_test(NAME installed.consumer.three-way
  COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-install.sh
          --build ${CMAKE_BINARY_DIR} three-way --rom "${T64_CONSUMER_ROM}"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(installed.consumer.three-way PROPERTIES
  LABELS "installed;phase11"
  TIMEOUT 120
  COST 0.6
  FIXTURES_REQUIRED t64_consumer)

# repo.installed-case-map (plan 11-22, carry-in T8, W-5): the cases the built consumer
# lists (`t64c --list-cases`), the closed list tests/installed-consumer/cases.txt and the
# registered installed.consumer.* CTest names agree: every case has exactly one name
# installed.consumer.<case>, every other name is one of the three script names that the
# checker hard-codes (build, literals, three-way), and a stray or doubly registered name
# fails with its own text. It reads the consumer built by installed.consumer.build, so it
# requires the t64_consumer fixture; --selftest first runs the seeded defects. COST is the
# measured native seconds (W-11); TIMEOUT follows F-108.
add_test(NAME repo.installed-case-map
  COMMAND python3 -B ${CMAKE_SOURCE_DIR}/scripts/check-installed-case-map.py
          --build ${CMAKE_BINARY_DIR} --selftest
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(repo.installed-case-map PROPERTIES
  LABELS "repo;phase11"
  TIMEOUT 60
  COST 0.12
  FIXTURES_REQUIRED t64_consumer
  ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

# Negative controls of the installed package (plan 11-19, VER-05, T-11-38 to T-11-40):
# one CTest name per row of tests/installed-consumer/controls.tsv, performed by
# `check-install.sh control ID`. Each row fails loudly with a specific expected text or
# status (missing package, version mismatch, visible source tree, doctored header,
# relocated prefix with spaces, symbol collision, plain cc line). A control needs only
# the moved prefix of install.setup (FIXTURES_REQUIRED t64_install) and works in its
# own directory below BUILD/install-check/controls, so the names may run in parallel.
# COST is the measured native seconds of the name (W-11): a row without a recorded cost
# is a configure error. TIMEOUT follows F-108.
set(T64_CONTROLS_TSV "${CMAKE_SOURCE_DIR}/tests/installed-consumer/controls.tsv")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${T64_CONTROLS_TSV}")
file(STRINGS "${T64_CONTROLS_TSV}" T64_CONTROL_LINES)
set(T64_CONTROL_COST_missing-package 0.43)
set(T64_CONTROL_COST_version-mismatch-minor 0.41)
set(T64_CONTROL_COST_version-mismatch-patch 0.44)
set(T64_CONTROL_COST_visible-source-tree 0.44)
set(T64_CONTROL_COST_visible-source-tree-seed 0.44)
set(T64_CONTROL_COST_doctored-header-abi 0.55)
set(T64_CONTROL_COST_doctored-header-size 0.62)
set(T64_CONTROL_COST_relocated-spaces 0.67)
set(T64_CONTROL_COST_symbol-collision 1.5)
set(T64_CONTROL_COST_plain-cc 0.4)
foreach(T64_CONTROL_LINE IN LISTS T64_CONTROL_LINES)
  if(T64_CONTROL_LINE STREQUAL "" OR T64_CONTROL_LINE MATCHES "^#")
    continue()
  endif()
  string(REPLACE "\t" ";" T64_CONTROL_COLS "${T64_CONTROL_LINE}")
  list(GET T64_CONTROL_COLS 0 T64_CONTROL_ID)
  if(NOT DEFINED T64_CONTROL_COST_${T64_CONTROL_ID})
    message(FATAL_ERROR
      "tests/installed-consumer/controls.tsv lists '${T64_CONTROL_ID}' with no "
      "measured COST (set T64_CONTROL_COST_${T64_CONTROL_ID} in tools/abi/CMakeLists.txt)")
  endif()
  add_test(NAME installed.controls.${T64_CONTROL_ID}
    COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/check-install.sh
            --build ${CMAKE_BINARY_DIR} control ${T64_CONTROL_ID}
            --rom "${T64_CONSUMER_ROM}"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  set_tests_properties(installed.controls.${T64_CONTROL_ID} PROPERTIES
    LABELS "installed;controls;phase11"
    TIMEOUT 120
    COST ${T64_CONTROL_COST_${T64_CONTROL_ID}}
    FIXTURES_REQUIRED t64_install)
endforeach()
