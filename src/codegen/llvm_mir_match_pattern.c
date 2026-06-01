/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM MIR match-pattern identity helpers.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_match_pattern.h"

#include <stdint.h>
#include <stdio.h>

#include "codegen_match_variant_policy.h"
#include "parser/ast_api.h"

void
llvm_mir_match_payload_alloca_name(ASTNode *match_case,
                                   const char *binding,
                                   char *buffer,
                                   size_t buffer_size)
{
    uint32_t stable_id;

    if (buffer == NULL || buffer_size == 0)
        return;
    buffer[0] = '\0';
    if (binding == NULL) {
        snprintf(buffer, buffer_size, "mir.match.payload");
        return;
    }
    stable_id = ast_node_stable_id(match_case);
    if (stable_id == 0) {
        snprintf(buffer, buffer_size, "%s.mir.match", binding);
        return;
    }
    snprintf(buffer, buffer_size, "%s.mir.match.%u", binding, stable_id);
}

bool
llvm_mir_is_option_destructor(ASTNode *pat,
                              const char **kind,
                              const char **binding)
{
    ASTNode *callee;
    ASTNode *payload;
    size_t arg_count;

    if (kind != NULL)
        *kind = NULL;
    if (binding != NULL)
        *binding = NULL;
    if (pat == NULL)
        return false;

    if (pat->type == AST_IDENTIFIER) {
        const char *name = ast_identifier_name(pat);
        PgyCodegenMatchVariantKind variant =
            pgy_codegen_match_variant_lookup(name);
        if (variant == PGY_MATCH_VARIANT_NONE_CTOR) {
            if (kind != NULL)
                *kind = pgy_codegen_match_variant_name(variant);
            return true;
        }
        return false;
    }

    callee = ast_call_callee(pat);
    arg_count = ast_call_arg_count(pat);
    if (pat->type != AST_CALL
        || callee == NULL
        || callee->type != AST_IDENTIFIER) {
        return false;
    }

    const char *name = ast_identifier_name(callee);
    PgyCodegenMatchVariantKind variant =
        pgy_codegen_match_variant_lookup(name);
    if (name == NULL)
        return false;

    if (variant == PGY_MATCH_VARIANT_NONE_CTOR && arg_count == 0) {
        if (kind != NULL)
            *kind = pgy_codegen_match_variant_name(variant);
        return true;
    }
    if (variant == PGY_MATCH_VARIANT_SOME && arg_count == 1) {
        if (kind != NULL)
            *kind = pgy_codegen_match_variant_name(variant);
        payload = ast_call_argument(pat, 0);
        if (binding != NULL
            && payload != NULL
            && payload->type == AST_IDENTIFIER) {
            *binding = ast_identifier_name(payload);
        }
        return true;
    }

    return false;
}

bool
llvm_mir_is_result_destructor(ASTNode *pat,
                              const char **kind,
                              const char **binding)
{
    ASTNode *callee;
    ASTNode *payload;
    size_t arg_count;

    if (kind != NULL)
        *kind = NULL;
    if (binding != NULL)
        *binding = NULL;
    if (pat == NULL)
        return false;

    callee = ast_call_callee(pat);
    arg_count = ast_call_arg_count(pat);
    if (pat->type != AST_CALL
        || callee == NULL
        || callee->type != AST_IDENTIFIER) {
        return false;
    }

    const char *name = ast_identifier_name(callee);
    PgyCodegenMatchVariantKind variant =
        pgy_codegen_match_variant_lookup(name);
    if (name == NULL)
        return false;

    if (pgy_codegen_match_variant_is_result(variant)
        && arg_count == 1) {
        if (kind != NULL)
            *kind = pgy_codegen_match_variant_name(variant);
        payload = ast_call_argument(pat, 0);
        if (binding != NULL
            && payload != NULL
            && payload->type == AST_IDENTIFIER) {
            *binding = ast_identifier_name(payload);
        }
        return true;
    }

    return false;
}

#endif /* PGY_LLVM_ENABLED */
