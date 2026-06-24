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

/*
 * One BLUE erasure site collected from an AIRProgram (docs/140 slice 5b).
 * `line`/`col` are 0 when the AIR node has no source AST (synthetic programs);
 * a producer should skip line==0 records since they have no editor location.
 * `source_name` and `reason` are borrowed from the AIR name pool / static text.
 */
typedef struct
{
    uint32_t    line;
    uint32_t    col;
    const char *source_name;
    const char *reason;
} AIRErasureSquiggle;

/*
 * Walk an AIRProgram's intents and boundaries, and for each whose compression is
 * BLUE (fully erased) write an AIRErasureSquiggle into `out` (up to `cap`).
 * Returns the number written. Pure read-only walk — no allocation, no mutation
 * of the AIR. The caller (LSP path only) turns these into BLUE advisories.
 */
size_t air_collect_erasure_squiggles(const AIRProgram *air,
                                     AIRErasureSquiggle *out, size_t cap);

#endif /* PERGYRA_AIR_ERASURE_SQUIGGLE_H */
