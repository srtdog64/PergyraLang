/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM MIR match-condition lowering.
 */

#ifdef PGY_LLVM_ENABLED

#include "codegen_match_variant_policy.h"
#include "llvm_internal.h"

#include <string.h>

static ASTNode *
llvm_mir_find_match_subject_for_case(ASTNode *node, ASTNode *case_node)
{
    if (node == NULL || case_node == NULL)
        return NULL;

    if (node->type == AST_MATCH_STMT) {
        for (size_t i = 0; i < ast_match_case_count(node); i++) {
            if (ast_match_case_at(node, i) == case_node)
                return ast_match_subject(node);
        }
        if (ast_match_default_body(node) != NULL) {
            ASTNode *found = llvm_mir_find_match_subject_for_case(
                ast_match_default_body(node), case_node);
            if (found != NULL)
                return found;
        }
    }

    switch (node->type) {
    case AST_BLOCK:
        for (size_t i = 0; i < ast_block_statement_count(node); i++) {
            ASTNode *found = llvm_mir_find_match_subject_for_case(
                ast_block_statement(node, i), case_node);
            if (found != NULL)
                return found;
        }
        break;
    case AST_IF_STMT: {
        ASTNode *found = llvm_mir_find_match_subject_for_case(
            ast_if_then_branch(node), case_node);
        if (found != NULL)
            return found;
        return llvm_mir_find_match_subject_for_case(
            ast_if_else_branch(node), case_node);
    }
    case AST_FOR_LOOP:
        return llvm_mir_find_match_subject_for_case(
            ast_for_body(node), case_node);
    case AST_WHILE_LOOP:
        return llvm_mir_find_match_subject_for_case(
            ast_while_body(node), case_node);
    case AST_WITH_STMT:
        return llvm_mir_find_match_subject_for_case(
            ast_with_body(node), case_node);
    default:
        break;
    }
    return NULL;
}

static bool
llvm_mir_is_option_destructor(ASTNode *pat, const char **kind,
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

static bool
llvm_mir_is_result_destructor(ASTNode *pat, const char **kind,
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

LLVMValueRef
llvm_mir_emit_match_case_condition(ASTNode *func_decl, ASTNode *case_node,
                                   LLVMGenCtx *ctx)
{
    ASTNode *subject_node;
    LLVMValueRef subject;
    LLVMValueRef cmp = NULL;

    if (func_decl == NULL || case_node == NULL || ctx == NULL
        || case_node->type != AST_MATCH_CASE) {
        return NULL;
    }

    subject_node = func_decl->type == AST_FUNC_DECL
        ? llvm_mir_find_match_subject_for_case(ast_func_body(func_decl),
              case_node)
        : NULL;
    if (subject_node == NULL)
        return NULL;

    subject = llvm_emit_expression(subject_node, ctx);
    if (subject == NULL)
        return NULL;

    if (ast_match_case_patterns(case_node, NULL) != NULL
        && ast_match_case_pattern_count(case_node) > 1) {
        for (size_t i = 0; i < ast_match_case_pattern_count(case_node); i++) {
            LLVMValueRef pattern = llvm_emit_expression(
                ast_match_case_pattern_at(case_node, i), ctx);
            LLVMValueRef one_cmp;
            if (pattern == NULL)
                continue;
            one_cmp = LLVMBuildICmp(ctx->builder, LLVMIntEQ, subject, pattern,
                                    llvm_tmp_name(ctx));
            cmp = cmp == NULL ? one_cmp
                              : LLVMBuildOr(ctx->builder, cmp, one_cmp,
                                            llvm_tmp_name(ctx));
        }
        return cmp;
    }

    if (ast_match_case_pattern(case_node) == NULL)
        return NULL;

    {
        const char *option_kind = NULL;
        const char *result_kind = NULL;
        const char *binding = NULL;
        ASTNode *pattern_node = ast_match_case_pattern(case_node);
        if (llvm_mir_is_option_destructor(pattern_node,
                                          &option_kind, &binding)) {
            LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, subject, 0,
                llvm_tmp_name(ctx));
            if (binding != NULL) {
                LLVMValueRef payload = LLVMBuildExtractValue(ctx->builder,
                    subject, 1, llvm_tmp_name(ctx));
                LLVMTypeRef payload_ty = LLVMTypeOf(payload);
                LLVMValueRef payload_alloca = llvm_create_entry_alloca(ctx,
                    payload_ty, binding);
                LLVMBuildStore(ctx->builder, payload, payload_alloca);
                llvm_scope_declare(ctx, pergyra_strdup(binding),
                    payload_alloca, payload_ty);
            }
            return LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
                LLVMConstInt(ctx->type_i32,
                    pgy_codegen_match_variant_llvm_tag(
                        pgy_codegen_match_variant_lookup(option_kind)), 0),
                llvm_tmp_name(ctx));
        }
        if (llvm_mir_is_result_destructor(pattern_node,
                                          &result_kind, &binding)) {
            LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, subject, 0,
                llvm_tmp_name(ctx));
            if (binding != NULL) {
                unsigned payload_index =
                    pgy_codegen_match_variant_result_payload_index(
                        pgy_codegen_match_variant_lookup(result_kind));
                LLVMValueRef payload = LLVMBuildExtractValue(ctx->builder,
                    subject, payload_index, llvm_tmp_name(ctx));
                LLVMTypeRef payload_ty = LLVMTypeOf(payload);
                LLVMValueRef payload_alloca = llvm_create_entry_alloca(ctx,
                    payload_ty, binding);
                LLVMBuildStore(ctx->builder, payload, payload_alloca);
                llvm_scope_declare(ctx, pergyra_strdup(binding),
                    payload_alloca, payload_ty);
            }
            return LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
                LLVMConstInt(ctx->type_i32,
                    pgy_codegen_match_variant_llvm_tag(
                        pgy_codegen_match_variant_lookup(result_kind)), 0),
                llvm_tmp_name(ctx));
        }
        /* General enum variant destructor: Circle(r), Empty, etc */
        {
            const char *variant_name = NULL;
            size_t variant_argc = 0;
            ASTNode *callee = NULL;
            if (pattern_node->type == AST_CALL) {
                callee = ast_call_callee(pattern_node);
                if (callee != NULL && callee->type == AST_IDENTIFIER) {
                    variant_name = ast_identifier_name(callee);
                    variant_argc = ast_call_arg_count(pattern_node);
                }
            } else if (pattern_node->type == AST_IDENTIFIER) {
                variant_name = ast_identifier_name(pattern_node);
                variant_argc = 0;
            }
            if (variant_name != NULL) {
                LLVMEnumVariantEntry *variant =
                    llvm_lookup_enum_variant(ctx, variant_name);
                if (variant != NULL) {
                    LLVMClassTypeEntry *enum_cls =
                        llvm_lookup_class(ctx, variant->enum_name);
                    /*
                     * Plain enums (no payload variants) lower the subject as
                     * a bare i32 tag rather than a tagged-union struct, so
                     * compare directly without ExtractValue.
                     */
                    LLVMTypeKind subject_kind =
                        LLVMGetTypeKind(LLVMTypeOf(subject));
                    if (subject_kind != LLVMStructTypeKind) {
                        return LLVMBuildICmp(ctx->builder, LLVMIntEQ,
                            subject,
                            LLVMConstInt(LLVMTypeOf(subject),
                                (unsigned long long)variant->value, 0),
                            llvm_tmp_name(ctx));
                    }
                    LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder,
                        subject, 0, llvm_tmp_name(ctx));
                    if (variant_argc > 0 && enum_cls != NULL) {
                        int field_idx = llvm_class_field_index(enum_cls,
                            variant_name);
                        if (field_idx > 0) {
                            LLVMValueRef payload = LLVMBuildExtractValue(
                                ctx->builder, subject, (unsigned)field_idx,
                                llvm_tmp_name(ctx));
                            for (size_t i = 0; i < variant_argc; i++) {
                                ASTNode *arg = ast_call_argument(pattern_node,
                                    i);
                                if (arg == NULL
                                    || arg->type != AST_IDENTIFIER) {
                                    continue;
                                }
                                const char *binding_name =
                                    ast_identifier_name(arg);
                                if (binding_name == NULL)
                                    continue;
                                LLVMValueRef binding_val =
                                    LLVMBuildExtractValue(ctx->builder,
                                        payload, (unsigned)i,
                                        llvm_tmp_name(ctx));
                                LLVMTypeRef binding_ty =
                                    LLVMTypeOf(binding_val);
                                LLVMValueRef alloca = llvm_create_entry_alloca(
                                    ctx, binding_ty, binding_name);
                                LLVMBuildStore(ctx->builder, binding_val,
                                    alloca);
                                llvm_scope_declare(ctx,
                                    pergyra_strdup(binding_name),
                                    alloca, binding_ty);
                            }
                        }
                    }
                    return LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
                        LLVMConstInt(ctx->type_i32,
                            (unsigned long long)variant->value, 0),
                        llvm_tmp_name(ctx));
                }
            }
        }
        LLVMValueRef pattern = llvm_emit_expression(pattern_node, ctx);
        if (pattern == NULL)
            return NULL;
        return LLVMBuildICmp(ctx->builder, LLVMIntEQ, subject, pattern,
                             llvm_tmp_name(ctx));
    }
}

#endif /* PGY_LLVM_ENABLED */
