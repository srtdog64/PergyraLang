#ifndef PERGYRA_MATCH_VARIANT_POLICY_H
#define PERGYRA_MATCH_VARIANT_POLICY_H

#include <stdbool.h>
#include <stddef.h>

typedef enum PgyMatchVariantKind {
    PGY_MATCH_VARIANT_NONE = 0,
    PGY_MATCH_VARIANT_ERR,
    PGY_MATCH_VARIANT_NONE_CTOR,
    PGY_MATCH_VARIANT_OK,
    PGY_MATCH_VARIANT_SOME,
} PgyMatchVariantKind;

PgyMatchVariantKind pgy_match_variant_lookup(const char *name);
const char *pgy_match_variant_name(PgyMatchVariantKind kind);

bool pgy_match_variant_is_option(PgyMatchVariantKind kind);
bool pgy_match_variant_is_result(PgyMatchVariantKind kind);
bool pgy_match_variant_is_builtin(PgyMatchVariantKind kind);

const char * const *pgy_match_variant_option_names(size_t *count_out);
const char * const *pgy_match_variant_result_names(size_t *count_out);

/* Type family of a match subject. Semantic decides it from the subject type
 * and records it on the match case; consumers branch on it, never on a
 * variant spelling, because a user enum may declare variants named Some,
 * None, Ok or Err (docs/205 section 2.3). */
typedef enum PgyMatchSubjectFamily {
    PGY_MATCH_SUBJECT_UNKNOWN = 0,
    PGY_MATCH_SUBJECT_OPTION,
    PGY_MATCH_SUBJECT_RESULT,
    PGY_MATCH_SUBJECT_ENUM,
} PgyMatchSubjectFamily;

const char *pgy_match_subject_family_name(PgyMatchSubjectFamily family);

#endif /* PERGYRA_MATCH_VARIANT_POLICY_H */
