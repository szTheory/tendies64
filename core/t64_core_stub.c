/*
 * Allowlisted placeholder core (CONTEXT A-1). It has no machine behavior; it
 * exists so the build and the determinism lint always have a core file to
 * process. Behavioral core code is not permitted until ADR-0002 is accepted.
 */
#include "tendies64/t64_facade.h"

const char *t64_core_identity(void) { return "tendies64-core-stub"; }
