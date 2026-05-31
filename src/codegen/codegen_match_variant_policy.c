/*
 * Copyright (c) 2026 Pergyra Language Project
 * Codegen tags for the shared Option/Result variant vocabulary.
 */

#include "codegen_match_variant_policy.h"

PgyCodegenMatchVariantKind
pgy_codegen_match_variant_lookup(const char *name)
{
    return pgy_match_variant_lookup(name);
}

const char *
pgy_codegen_match_variant_name(PgyCodegenMatchVariantKind kind)
{
    return pgy_match_variant_name(kind);
}

bool
pgy_codegen_match_variant_is_option(PgyCodegenMatchVariantKind kind)
{
    return pgy_match_variant_is_option(kind);
}

bool
pgy_codegen_match_variant_is_result(PgyCodegenMatchVariantKind kind)
{
    return pgy_match_variant_is_result(kind);
}

bool
pgy_codegen_match_variant_is_builtin(PgyCodegenMatchVariantKind kind)
{
    return pgy_match_variant_is_builtin(kind);
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
