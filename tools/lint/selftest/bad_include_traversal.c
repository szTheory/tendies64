/* EXPECT: L1-INCLUDE-UNLISTED */
/* A quoted include that climbs out with ".." is refused even when the target
 * is a core header; core headers are reached through core/include (WR-16). */
#include "../../../core/include/tendies64/t64_facade.h"

int t64_seed_traversal(void) { return 0; }
