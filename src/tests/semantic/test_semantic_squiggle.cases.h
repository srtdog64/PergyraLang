/*
 * Semantic squiggle classification policy (docs/140 §3).
 *
 * Pure function over (is_blocking, DiagnosticLayer, code) -> SquiggleClass.
 * These lock in the mapping table so a code->colour drift is caught (CLAUDE.md
 * §11.2: codes/event names are regression-prone).
 */

#include "common/squiggle_class.h"

static void
test_squiggle_class(void)
{
    TEST("blocking diagnostic is always RED (fail-closed)");
    {
        /* Whatever the dimension, if it blocks it is shown as blocking. */
        EXPECT(squiggle_class_classify(true, DIAG_LAYER_TYPE,
                   "PGY_CODE_SEM_TYPE_MISMATCH") == SQUIGGLE_RED);
        EXPECT(squiggle_class_classify(true, DIAG_LAYER_DOMAIN,
                   "PGY_CODE_SEM_WORLD_CONTRACT_INVALID") == SQUIGGLE_RED);
    }

    TEST("axis-dimension codes are AMBER (advisory)");
    {
        EXPECT(squiggle_class_classify(false, DIAG_LAYER_DOMAIN,
                   "PGY_CODE_SEM_INTENT_BOUNDARY_DRIFT") == SQUIGGLE_AMBER);
        EXPECT(squiggle_class_classify(false, DIAG_LAYER_DOMAIN,
                   "PGY_CODE_SEM_INTENT_STEP_INVALID") == SQUIGGLE_AMBER);
        EXPECT(squiggle_class_classify(false, DIAG_LAYER_DOMAIN,
                   "PGY_CODE_SEM_ROLE_CONTRACT_INVALID") == SQUIGGLE_AMBER);
    }

    TEST("authority-dimension codes are VIOLET (advisory)");
    {
        EXPECT(squiggle_class_classify(false, DIAG_LAYER_DOMAIN,
                   "PGY_CODE_SEM_WORLD_CONTRACT_INVALID") == SQUIGGLE_VIOLET);
        EXPECT(squiggle_class_classify(false, DIAG_LAYER_DOMAIN,
                   "PGY_CODE_SEM_ZONE_CONTRACT_INVALID") == SQUIGGLE_VIOLET);
        EXPECT(squiggle_class_classify(false, DIAG_LAYER_DOMAIN,
                   "PGY_CODE_SEM_VISIBILITY_BOUNDARY") == SQUIGGLE_VIOLET);
        /* pin lives in the RESOURCE layer but is an authority concern: the code
         * term wins over the layer fallback. */
        EXPECT(squiggle_class_classify(false, DIAG_LAYER_RESOURCE,
                   "PGY_CODE_SEM_PIN_ESCAPE") == SQUIGGLE_VIOLET);
    }

    TEST("axis term wins over BOUNDARY (intent boundary is axis, not authority)");
    {
        /* INTENT_BOUNDARY contains both an axis term and "_BOUNDARY_"; axis is
         * checked first so it resolves to amber, while VISIBILITY_BOUNDARY (no
         * axis term) resolves to violet. */
        EXPECT(squiggle_class_classify(false, DIAG_LAYER_DOMAIN,
                   "PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING")
               == SQUIGGLE_AMBER);
    }

    TEST("erasure codes are BLUE (reserved dimension)");
    {
        EXPECT(squiggle_class_classify(false, DIAG_LAYER_DOMAIN,
                   "PGY_CODE_SEM_MEANING_ERASURE_VISIBLE") == SQUIGGLE_BLUE);
    }

    TEST("layer fallback when code carries no dimension term");
    {
        EXPECT(squiggle_class_classify(false, DIAG_LAYER_DOMAIN, NULL)
               == SQUIGGLE_AMBER);
        EXPECT(squiggle_class_classify(false, DIAG_LAYER_RESOURCE, NULL)
               == SQUIGGLE_AMBER);
        EXPECT(squiggle_class_classify(false, DIAG_LAYER_CONCURRENCY, NULL)
               == SQUIGGLE_VIOLET);
        /* A plain non-blocking diagnostic in a non-meaning layer gets no
         * semantic squiggle. */
        EXPECT(squiggle_class_classify(false, DIAG_LAYER_TYPE,
                   "PGY_CODE_SEM_SOME_WARNING") == SQUIGGLE_NONE);
    }

    TEST("squiggle_class_name maps to stable lowercase strings");
    {
        EXPECT(strcmp(squiggle_class_name(SQUIGGLE_RED), "red") == 0);
        EXPECT(strcmp(squiggle_class_name(SQUIGGLE_AMBER), "amber") == 0);
        EXPECT(strcmp(squiggle_class_name(SQUIGGLE_VIOLET), "violet") == 0);
        EXPECT(strcmp(squiggle_class_name(SQUIGGLE_BLUE), "blue") == 0);
        EXPECT(strcmp(squiggle_class_name(SQUIGGLE_NONE), "none") == 0);
    }
}
