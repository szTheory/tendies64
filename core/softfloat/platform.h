#ifndef T64_SOFTFLOAT_PLATFORM_H
#define T64_SOFTFLOAT_PLATFORM_H

#if !defined(SOFTFLOAT_FAST_INT64) || SOFTFLOAT_FAST_INT64 != 1
#error "Tendies64 SoftFloat requires the portable SOFTFLOAT_FAST_INT64 build selection"
#endif

/* The SoftFloat names are renamed to t64i_<name> (DR-17); the vendored
 * primitives no longer guard their own definitions, so the rename applies. */
#include "t64_prefix.h"
#include "softfloat_types.h"

#endif
