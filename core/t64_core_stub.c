/*
 * Placeholder core (CONTEXT A-1). It has no machine behavior; it exists so the
 * build and the determinism lint always have a core file to process. ADR-0002
 * is accepted, so behavioral core code is now permitted; it starts in Phase 2,
 * and this file stays until real core code replaces it.
 */
#include "tendies64/t64_facade.h"

const char *t64_core_identity(void) { return "tendies64-core-stub"; }
