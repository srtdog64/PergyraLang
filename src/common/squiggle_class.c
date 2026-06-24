#include "squiggle_class.h"

#include <string.h>

/*
 * Squiggle classification policy (docs/140 §3).
 *
 * Code-term driven, with a layer fallback. The code prefix is the primary
 * signal because DiagnosticLayer alone cannot split the DOMAIN layer into the
 * axis (amber) vs authority (violet) dimensions, and because a pin diagnostic
 * lives in the RESOURCE layer yet is an authority concern. The order matters:
 * erasure, then axis, then authority -- so `INTENT_BOUNDARY_*` (which contains
 * both an axis term and "BOUNDARY") classifies as axis, while `WORLD_CONTRACT_*`
 * and `VISIBILITY_BOUNDARY` fall through to authority.
 */

static bool
code_contains_any(const char *code, const char *const *needles,
                  size_t needle_count)
{
    if (code == NULL)
        return false;
    for (size_t i = 0; i < needle_count; i++) {
        if (strstr(code, needles[i]) != NULL)
            return true;
    }
    return false;
}

#define SQUIGGLE_ARRAY_LEN(values) (sizeof(values) / sizeof((values)[0]))

const char *
squiggle_class_name(SquiggleClass cls)
{
    switch (cls) {
    case SQUIGGLE_RED:
        return "red";
    case SQUIGGLE_AMBER:
        return "amber";
    case SQUIGGLE_VIOLET:
        return "violet";
    case SQUIGGLE_BLUE:
        return "blue";
    case SQUIGGLE_NONE:
    default:
        return "none";
    }
}

SquiggleClass
squiggle_class_classify(bool is_blocking, DiagnosticLayer layer,
                        const char *code)
{
    /* Axis dimension: identity/intent/lifecycle meaning (amber). Checked before
     * authority so `_INTENT_BOUNDARY_` resolves to axis, not the boundary
     * (authority) bucket. */
    static const char *const axis_code_terms[] = {
        "_INTENT_", "_ROLE_", "_SUBJECT_", "_VESSEL_",
        "_RELATION_", "_PROJECTION_", "_LIFECYCLE_", "_STATE_",
    };
    /* Authority dimension: world/zone/capability/pin/effect boundary (violet). */
    static const char *const authority_code_terms[] = {
        "_WORLD_", "_ZONE_", "_AUTHORITY_", "_CAPABILITY_",
        "_VISIBILITY_", "_PERMISSION_", "_EFFECT_", "_PIN_", "_BOUNDARY_",
    };
    /* Runtime-erasure dimension (blue). Reserved: no live diagnostic emits these
     * yet -- the AIR erasure data (docs/14) is not wired to the diagnostic path.
     * Kept so the policy table is complete and the wiring slice is a no-op map. */
    static const char *const erasure_code_terms[] = {
        "_ERASURE_", "_ERASED_",
    };

    /* Fail-closed: a blocking diagnostic is shown as blocking (red), whatever
     * dimension it belongs to. */
    if (is_blocking)
        return SQUIGGLE_RED;

    if (code_contains_any(code, erasure_code_terms,
                          SQUIGGLE_ARRAY_LEN(erasure_code_terms)))
        return SQUIGGLE_BLUE;
    if (code_contains_any(code, axis_code_terms,
                          SQUIGGLE_ARRAY_LEN(axis_code_terms)))
        return SQUIGGLE_AMBER;
    if (code_contains_any(code, authority_code_terms,
                          SQUIGGLE_ARRAY_LEN(authority_code_terms)))
        return SQUIGGLE_VIOLET;

    /* Layer fallback when the code is uncoded or carries no dimension term. */
    switch (layer) {
    case DIAG_LAYER_DOMAIN:
    case DIAG_LAYER_RESOURCE:
        return SQUIGGLE_AMBER;
    case DIAG_LAYER_CONCURRENCY:
        return SQUIGGLE_VIOLET;
    default:
        return SQUIGGLE_NONE;
    }
}
