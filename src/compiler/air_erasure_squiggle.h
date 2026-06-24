#ifndef PERGYRA_AIR_ERASURE_SQUIGGLE_H
#define PERGYRA_AIR_ERASURE_SQUIGGLE_H

#include "air.h"
#include "../common/squiggle_class.h"

/*
 * BLUE (erasure) squiggle policy (docs/140 slice 5).
 *
 * Maps an AIR boundary's compression decision to a squiggle class. BLUE means
 * the boundary's domain meaning is erased at runtime — it has no runtime
 * footprint, so a developer who expects it to be observable at run time should
 * see it. This is the faithful source of BLUE: AIR is the only stage that
 * *measures* erasure (docs/14), which is why a semantic-stage guess is refused.
 *
 * Pure: one switch, no I/O, no allocation, no codegen — zero runtime cost.
 */
SquiggleClass air_compression_squiggle_class(AIRCompressionBudget budget,
                                             AIRRetainCause cause);

#endif /* PERGYRA_AIR_ERASURE_SQUIGGLE_H */
