/*
 * Copyright (c) 2026 Pergyra Language Project
 * Option/Result variant vocabulary shared by parser, semantic, C, and LLVM.
 */

#include "match_variant_policy.h"

#include <stdlib.h>
#include <string.h>

typedef struct PgyMatchVariantSpec {
    const char *name;
    PgyMatchVariantKind kind;
} PgyMatchVariantSpec;

static int
pgy_match_variant_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const PgyMatchVariantSpec *spec = (const PgyMatchVariantSpec *)entry;

    return strcmp(name, spec->name);
}

PgyMatchVariantKind
pgy_match_variant_lookup(const char *name)
{
    static const PgyMatchVariantSpec specs[] = {
        { "Err", PGY_MATCH_VARIANT_ERR },
        { "None", PGY_MATCH_VARIANT_NONE_CTOR },
        { "Ok", PGY_MATCH_VARIANT_OK },
        { "Some", PGY_MATCH_VARIANT_SOME },
    };
    const PgyMatchVariantSpec *spec;

    if (name == NULL)
        return PGY_MATCH_VARIANT_NONE;
    spec = (const PgyMatchVariantSpec *)bsearch(&name,
        specs,
        sizeof(specs) / sizeof(specs[0]),
        sizeof(specs[0]),
        pgy_match_variant_compare);
    return spec != NULL ? spec->kind : PGY_MATCH_VARIANT_NONE;
}

const char *
pgy_match_variant_name(PgyMatchVariantKind kind)
{
    switch (kind) {
    case PGY_MATCH_VARIANT_ERR:
        return "Err";
    case PGY_MATCH_VARIANT_NONE_CTOR:
        return "None";
    case PGY_MATCH_VARIANT_OK:
        return "Ok";
    case PGY_MATCH_VARIANT_SOME:
        return "Some";
    default:
        return NULL;
    }
}

bool
pgy_match_variant_is_option(PgyMatchVariantKind kind)
{
    return kind == PGY_MATCH_VARIANT_NONE_CTOR
        || kind == PGY_MATCH_VARIANT_SOME;
}

bool
pgy_match_variant_is_result(PgyMatchVariantKind kind)
{
    return kind == PGY_MATCH_VARIANT_ERR
        || kind == PGY_MATCH_VARIANT_OK;
}

bool
pgy_match_variant_is_builtin(PgyMatchVariantKind kind)
{
    return pgy_match_variant_is_option(kind)
        || pgy_match_variant_is_result(kind);
}

const char * const *
pgy_match_variant_option_names(size_t *count_out)
{
    static const char *const option_variants[] = { "Some", "None" };

    if (count_out != NULL)
        *count_out = sizeof(option_variants) / sizeof(option_variants[0]);
    return option_variants;
}

const char * const *
pgy_match_variant_result_names(size_t *count_out)
{
    static const char *const result_variants[] = { "Ok", "Err" };

    if (count_out != NULL)
        *count_out = sizeof(result_variants) / sizeof(result_variants[0]);
    return result_variants;
}
