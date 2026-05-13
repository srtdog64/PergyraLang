/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM intent emission support routines.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_intent_internal.h"

bool
llvm_intent_involves_uses_pointer_self(LLVMGenCtx *ctx, ASTNode *involves)
{
    const char *type_name = llvm_intent_involves_type_name(involves);

    if (ctx == NULL || type_name == NULL)
        return false;
    return llvm_type_name_uses_pointer_self(ctx, type_name);
}

const char *
llvm_intent_step_effective_zone_alias(ASTNode *step)
{
    if (step == NULL || step->type != AST_INTENT_STEP)
        return NULL;
    if (ast_intent_step_using_expr(step) != NULL
        && ast_intent_step_using_expr(step)->type == AST_IDENTIFIER) {
        return ast_intent_step_using_expr(step)->data.identifier.name;
    }
    return ast_intent_step_transfer_to_alias(step);
}

static ASTNode *
llvm_find_intent_step_source_by_name(ASTNode *intent, const char *step_name)
{
    if (intent == NULL || intent->type != AST_INTENT_DECL || step_name == NULL)
        return NULL;
    for (size_t i = 0; i < intent->data.intent_decl.step_count; i++) {
        ASTNode *step = intent->data.intent_decl.steps[i];
        if (step == NULL || step->type != AST_INTENT_STEP
            || ast_intent_step_name(step) == NULL) {
            continue;
        }
        if (strcmp(ast_intent_step_name(step), step_name) == 0)
            return step;
    }
    return NULL;
}

ASTNode **
llvm_build_mir_intent_step_sources(ASTNode *intent,
                                   const char **step_names,
                                   size_t step_count,
                                   LLVMGenCtx *ctx)
{
    ASTNode **steps;

    if (step_count == 0 || step_names == NULL || ctx == NULL)
        return NULL;
    steps = pgy_arena_calloc(&ctx->scratch, step_count * sizeof(ASTNode *));
    if (steps == NULL)
        return NULL;
    for (size_t i = 0; i < step_count; i++)
        steps[i] = llvm_find_intent_step_source_by_name(intent, step_names[i]);
    return steps;
}

bool
llvm_intent_action_function_name(LLVMGenCtx *ctx, char *out, size_t out_size,
                                 const char *subject_name,
                                 const char *step_name)
{
    int written;

    if (out == NULL || out_size == 0 || subject_name == NULL || step_name == NULL)
        return false;
    written = snprintf(out, out_size, "%s_%s", subject_name, step_name);
    if (written >= 0 && (size_t)written < out_size)
        return true;
    llvm_set_error(ctx, "intent action function name is too long");
    return false;
}

bool
llvm_intent_reason_name(LLVMGenCtx *ctx, char *out, size_t out_size,
                        const char *prefix, const char *step_name)
{
    int written;

    if (out == NULL || out_size == 0 || prefix == NULL)
        return false;
    written = snprintf(out, out_size, "%s:%s", prefix,
        step_name != NULL ? step_name : "<step>");
    if (written >= 0 && (size_t)written < out_size)
        return true;
    llvm_set_error_with_hints(ctx,
        PGY_CODE_LLVM_SPEC_LIMIT,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_REFACTOR_OR_RAISE_LIMIT,
        "intent failure reason is too long for step '%s'",
        step_name != NULL ? step_name : "<step>");
    return false;
}

#endif
