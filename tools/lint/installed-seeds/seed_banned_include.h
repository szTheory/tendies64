/*
 * Seed for scripts/check-api04.sh (seeded subcommand), plan 11-17: an installed
 * header that includes a banned system header. The seeded run copies the
 * installed include directory, adds this file below tendies64/ and expects the
 * Layer-1 scan (tools/lint/core-lint.sh --files) to fail with L1-INCLUDE-BANNED.
 */
#ifndef T64_SEED_BANNED_INCLUDE_H
#define T64_SEED_BANNED_INCLUDE_H
#include <stdio.h>
#endif
