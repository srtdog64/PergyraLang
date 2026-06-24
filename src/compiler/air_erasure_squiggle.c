#include "air_erasure_squiggle.h"

#include "../parser/ast.h"

/*
 * BLUE policy (docs/140 §9). BLUE marks meaning that is *erasable* at runtime —
 * docs/140's definition is "erase 가능한 의미" (meaning that *can be* erased),
 * not meaning that is already gone. The AIR budget that means exactly this is
 * SUMMARIZE (bucket-B POLICY: kept as a runtime digest but removable by opt-out):
 *
 *   SUMMARIZE -> BLUE   meaning is compressed to a digest, and is erasable
 *   ERASE     -> NONE   already fully erased — nothing is left at the site to flag
 *   RETAIN    -> NONE   bucket-A INHERENT (concurrency/authority): NOT erasable
 *   FORBID    -> NONE   a constraint, not an erasure of meaning
 *   UNKNOWN   -> NONE   undecided
 *
 * Empirically ERASE is unreachable from valid source: every writable intent step
 * binds a zone (a SUMMARIZE boundary) or carries authority (a RETAIN boundary),
 * and boundaries never erase — so keying BLUE on ERASE produced no real trigger.
 * SUMMARIZE is the reachable, faithful "this meaning is erasable" signal.
 *
 * The retain cause is the same information viewed as A/B/C buckets; the budget is
 * the load-bearing input. SUMMARIZE is common (every zone step), so a downstream
 * producer should scope the advisory (e.g. developer-written annotations) to
 * avoid noise — this function is the faithful budget->class map it builds on.
 */
SquiggleClass
air_compression_squiggle_class(AIRCompressionBudget budget,
                               AIRRetainCause cause)
{
    (void)cause;
    return budget == AIR_COMPRESSION_SUMMARIZE ? SQUIGGLE_BLUE : SQUIGGLE_NONE;
}

static bool
erasure_emit(AIRErasureSquiggle *out, size_t cap, size_t *count,
             const ASTNode *ast, const char *source_name, const char *reason)
{
    if (*count >= cap)
        return false;
    out[*count].line = ast != NULL ? ast->line : 0u;
    out[*count].col = ast != NULL ? ast->column : 0u;
    out[*count].source_name = source_name;
    out[*count].reason = reason;
    (*count)++;
    return true;
}

size_t
air_collect_erasure_squiggles(const AIRProgram *air, AIRErasureSquiggle *out,
                              size_t cap)
{
    size_t count = 0;

    if (air == NULL || out == NULL || cap == 0)
        return 0;

    /* Intents: a step whose orchestration meaning is fully erased. */
    for (size_t i = 0; i < air->intent_count; i++) {
        const AIRIntentNode *intent = &air->intents[i];
        if (air_compression_squiggle_class(
                air_intent_compression_budget(air, i),
                AIR_RETAIN_CAUSE_NONE) != SQUIGGLE_BLUE)
            continue;
        if (!erasure_emit(out, cap, &count, intent->ast,
                          intent->intent_owner,
                          air_intent_compression_reason(air, i)))
            return count;
    }

    /* Boundaries: a boundary whose domain meaning is fully erased. */
    for (size_t i = 0; i < air->boundary_count; i++) {
        const AIRBoundaryNode *boundary = &air->boundaries[i];
        if (air_compression_squiggle_class(
                air_boundary_compression_budget(boundary),
                air_boundary_retain_cause(boundary)) != SQUIGGLE_BLUE)
            continue;
        if (!erasure_emit(out, cap, &count, boundary->ast,
                          boundary->source_name,
                          air_boundary_compression_reason(boundary)))
            return count;
    }

    return count;
}
