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

#endif /* PERGYRA_MATCH_VARIANT_POLICY_H */
