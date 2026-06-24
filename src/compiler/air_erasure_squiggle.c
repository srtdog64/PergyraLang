#include "air_erasure_squiggle.h"

#include "../parser/ast.h"

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
