#include "air_erasure_squiggle.h"

/*
 * BLUE policy (docs/140 §9). Only a fully erased boundary qualifies:
 *
 *   ERASE     -> BLUE   meaning has no runtime footprint ("erased at runtime")
 *   RETAIN    -> NONE   runtime evidence is kept (meaning survives)
 *   SUMMARIZE -> NONE   a runtime digest is kept (meaning partially survives)
 *   FORBID    -> NONE   a constraint, not an erasure of meaning
 *   UNKNOWN   -> NONE   undecided
 *
 * The retain cause refines the *message* a producer writes (A/B/C buckets), not
 * the class — an ERASE boundary carries cause NONE by construction. Erasure is
 * often intended (zero-cost abstraction), so a downstream producer should scope
 * the advisory to developer-written domain annotations to avoid noise; this
 * function is the faithful budget->class map those producers share.
 */
SquiggleClass
air_compression_squiggle_class(AIRCompressionBudget budget,
                               AIRRetainCause cause)
{
    (void)cause;
    return budget == AIR_COMPRESSION_ERASE ? SQUIGGLE_BLUE : SQUIGGLE_NONE;
}
