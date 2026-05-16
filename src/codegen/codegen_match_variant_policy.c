/*
 * Copyright (c) 2026 Pergyra Language Project
 * Match destructor vocabulary shared by C, LLVM, and MIR lowering.
 */

#include "codegen_match_variant_policy.h"

#include <stdlib.h>
#include <string.h>

typedef struct PgyCodegenMatchVariantSpec {
    const char *name;
    PgyCodegenMatchVariantKind kind;
} PgyCodegenMatchVariantSpec;

static int
pgy_codegen_match_variant_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const PgyCodegenMatchVariantSpec *spec =
        (const PgyCodegenMatchVariantSpec *)entry;

    return strcmp(name, spec->name);
}

PgyCodegenMatchVariantKind
pgy_codegen_match_variant_lookup(const char *name)
{
    static const PgyCodegenMatchVariantSpec specs[] = {
        { "Err", PGY_MATCH_VARIANT_ERR },
        { "None", PGY_MATCH_VARIANT_NONE_CTOR },
        { "Ok", PGY_MATCH_VARIANT_OK },
        { "Some", PGY_MATCH_VARIANT_SOME },
    };
    const PgyCodegenMatchVariantSpec *spec;

    if (name == NULL)
        return PGY_MATCH_VARIANT_NONE;
    spec = (const PgyCodegenMatchVariantSpec *)bsearch(&name,
        specs,
        sizeof(specs) / sizeof(specs[0]),
        sizeof(specs[0]),
        pgy_codegen_match_variant_compare);
    return spec != NULL ? spec->kind : PGY_MATCH_VARIANT_NONE;
}

const char *
pgy_codegen_match_variant_name(PgyCodegenMatchVariantKind kind)
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
pgy_codegen_match_variant_is_option(PgyCodegenMatchVariantKind kind)
{
    return kind == PGY_MATCH_VARIANT_NONE_CTOR
        || kind == PGY_MATCH_VARIANT_SOME;
}

bool
pgy_codegen_match_variant_is_result(PgyCodegenMatchVariantKind kind)
{
    return kind == PGY_MATCH_VARIANT_ERR
        || kind == PGY_MATCH_VARIANT_OK;
}

bool
pgy_codegen_match_variant_is_builtin(PgyCodegenMatchVariantKind kind)
{
    return pgy_codegen_match_variant_is_option(kind)
        || pgy_codegen_match_variant_is_result(kind);
}

const char *
pgy_codegen_match_variant_c_option_tag(PgyCodegenMatchVariantKind kind)
{
    if (kind == PGY_MATCH_VARIANT_SOME)
        return "PgyOptionSome";
    if (kind == PGY_MATCH_VARIANT_NONE_CTOR)
        return "PgyOptionNone";
    return NULL;
}

const char *
pgy_codegen_match_variant_c_result_tag(PgyCodegenMatchVariantKind kind)
{
    if (kind == PGY_MATCH_VARIANT_OK)
        return "PgyResultOk";
    if (kind == PGY_MATCH_VARIANT_ERR)
        return "PgyResultErr";
    return NULL;
}

const char *
pgy_codegen_match_variant_c_payload_field(PgyCodegenMatchVariantKind kind)
{
    if (kind == PGY_MATCH_VARIANT_SOME)
        return "value";
    if (kind == PGY_MATCH_VARIANT_OK)
        return "ok";
    if (kind == PGY_MATCH_VARIANT_ERR)
        return "err";
    return NULL;
}

unsigned
pgy_codegen_match_variant_llvm_tag(PgyCodegenMatchVariantKind kind)
{
    return (kind == PGY_MATCH_VARIANT_SOME
            || kind == PGY_MATCH_VARIANT_OK) ? 0u : 1u;
}

unsigned
pgy_codegen_match_variant_result_payload_index(PgyCodegenMatchVariantKind kind)
{
    return kind == PGY_MATCH_VARIANT_ERR ? 2u : 1u;
}
