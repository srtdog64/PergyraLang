/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Shared call contract lookup and callee escape-summary helpers.
 */

#include "slot_summary.h"
#include "type_checker_internal.h"

#include <string.h>

static ASTNode *
legacy_ast_param_summary_program(SemanticContext *ctx)
{
    return ctx != NULL ? ctx->program_root : NULL;
}

static Type *
semantic_callable_decl_function_type(SemanticContext *ctx,
                                     ASTNode *callee_decl)
{
    const char *name;
    ASTNode *lookup_decl;
    Symbol *sym;

    if (ctx == NULL || callee_decl == NULL
        || callee_decl->type != AST_FUNC_DECL) {
        return NULL;
    }

    name = ast_declaration_name(callee_decl);
    if (name == NULL)
        return NULL;

    lookup_decl = semantic_find_callable_decl_by_name(ctx, name);
    if (lookup_decl != callee_decl)
        return NULL;

    sym = scope_lookup(ctx->scope, name);
    if (sym == NULL || sym->kind != SYMBOL_FUNCTION)
        return NULL;
    if (sym->type == NULL || sym->type->kind != TYPE_KIND_FUNCTION)
        return NULL;
    return sym->type;
}

static bool
semantic_callable_summary_proves_no_ref_escape(SemanticContext *ctx,
                                               ASTNode *callee_decl)
{
    Type *function_type = semantic_callable_decl_function_type(ctx, callee_decl);
    uint32_t summary = type_function_body_summary(function_type);

    return type_function_has_body_summary(function_type)
        && (summary & BODY_SUMMARY_MAY_ESCAPE_REF) == 0;
}

/* True when the callee's body-summary inventory positively proves the named
 * fact bit is NOT in the callee's body. Returns false when summary inventory
 * is absent (so the caller must fall back to AST walk or the legacy summary).
 * The shape mirrors `semantic_callable_summary_proves_no_ref_escape` so
 * consumers can read body-summary bits through one stable seam instead of
 * re-walking the callee body in every analyzer. */
bool
semantic_callable_summary_proves_no_drop_resource(SemanticContext *ctx,
                                                  ASTNode *callee_decl)
{
    Type *function_type = semantic_callable_decl_function_type(ctx, callee_decl);
    uint32_t summary = type_function_body_summary(function_type);

    return type_function_has_body_summary(function_type)
        && (summary & BODY_SUMMARY_DROPS_RESOURCE) == 0;
}

bool
semantic_callable_summary_proves_no_spawn_task(SemanticContext *ctx,
                                               ASTNode *callee_decl)
{
    Type *function_type = semantic_callable_decl_function_type(ctx, callee_decl);
    uint32_t summary = type_function_body_summary(function_type);

    return type_function_has_body_summary(function_type)
        && (summary & BODY_SUMMARY_SPAWNS_TASK) == 0;
}

bool
semantic_callable_summary_proves_no_send_channel(SemanticContext *ctx,
                                                 ASTNode *callee_decl)
{
    Type *function_type = semantic_callable_decl_function_type(ctx, callee_decl);
    uint32_t summary = type_function_body_summary(function_type);

    return type_function_has_body_summary(function_type)
        && (summary & BODY_SUMMARY_SENDS_CHANNEL) == 0;
}

bool
semantic_callable_summary_proves_no_zone_requirement(SemanticContext *ctx,
                                                     ASTNode *callee_decl)
{
    Type *function_type = semantic_callable_decl_function_type(ctx, callee_decl);
    uint32_t summary = type_function_body_summary(function_type);

    return type_function_has_body_summary(function_type)
        && (summary & BODY_SUMMARY_REQUIRES_ZONE) == 0;
}

bool
semantic_callable_summary_has_spawn_task(SemanticContext *ctx,
                                         ASTNode *callee_decl)
{
    Type *function_type = semantic_callable_decl_function_type(ctx, callee_decl);

    return semantic_callable_type_summary_has_spawn_task(function_type);
}

bool
semantic_callable_type_summary_has_spawn_task(const Type *function_type)
{
    uint32_t summary = type_function_body_summary(function_type);

    return type_function_has_body_summary(function_type)
        && (summary & BODY_SUMMARY_SPAWNS_TASK) != 0;
}

bool
semantic_callable_summary_has_send_channel(SemanticContext *ctx,
                                           ASTNode *callee_decl)
{
    Type *function_type = semantic_callable_decl_function_type(ctx, callee_decl);

    return semantic_callable_type_summary_has_send_channel(function_type);
}

bool
semantic_callable_type_summary_has_send_channel(const Type *function_type)
{
    uint32_t summary = type_function_body_summary(function_type);

    return type_function_has_body_summary(function_type)
        && (summary & BODY_SUMMARY_SENDS_CHANNEL) != 0;
}

bool
semantic_callable_type_requires_intent_authority(const Type *function_type)
{
    uint32_t summary;

    if (function_type == NULL || function_type->kind != TYPE_KIND_FUNCTION)
        return false;
    if (type_effect_mask_requires_authority(
            type_function_effects(function_type))) {
        return true;
    }
    if (!type_function_has_body_summary(function_type))
        return false;
    summary = type_function_body_summary(function_type);
    return (summary & (BODY_SUMMARY_REQUIRES_ZONE
                       | BODY_SUMMARY_CAUSES_EFFECT)) != 0;
}

ASTNode *
semantic_lookup_function_param_contract(SemanticContext *ctx,
                                        const char *display_name,
                                        size_t arg_index,
                                        ParamMode *mode_out)
{
    Symbol *symbol;
    Type *function_type = NULL;

    if (mode_out != NULL)
        *mode_out = PARAM_MODE_DEFAULT;

    if (ctx == NULL || display_name == NULL)
        return NULL;

    ASTNode *stmt = semantic_find_function_decl_by_name(ctx, display_name);
    if (stmt == NULL)
        return NULL;

    /* The resolved scope symbol owns the callable boundary contract.  The
     * host-declaration index is an AST lookup aid and may not carry the same
     * imported declaration identity while an import-composed program is
     * being checked.  Requiring pointer identity there silently downgraded
     * declared `ref` parameters to PARAM_MODE_DEFAULT in large self-host
     * roots, so safe read-only forwarding was diagnosed as a value escape. */
    symbol = scope_lookup(ctx->scope, display_name);
    if (symbol != NULL && symbol->kind == SYMBOL_FUNCTION
        && symbol->type != NULL
        && symbol->type->kind == TYPE_KIND_FUNCTION) {
        function_type = symbol->type;
    }
    if (function_type == NULL)
        function_type = semantic_callable_decl_function_type(ctx, stmt);

    if (function_type == NULL
        || arg_index >= type_function_param_count(function_type))
        return NULL;

    if (mode_out != NULL)
        *mode_out = type_function_param_mode(function_type, arg_index);
    return stmt;
}

unsigned
semantic_callable_param_escape_summary(ASTNode *callee_decl,
                                       size_t arg_index,
                                       SemanticContext *ctx)
{
    Type *function_type = semantic_callable_decl_function_type(ctx, callee_decl);

    if (callee_decl == NULL
        || callee_decl->type != AST_FUNC_DECL
        || ast_func_body(callee_decl) == NULL
        || arg_index >= ast_func_param_count(callee_decl)) {
        return 0u;
    }

    if (type_function_has_param_escape_summary(function_type, arg_index))
        return type_function_param_escape_summary(function_type, arg_index);

    if (semantic_callable_summary_proves_no_ref_escape(ctx, callee_decl))
        return 0u;

    unsigned mask = slot_analyze_legacy_ast_param_summary_in_program(
        callee_decl, arg_index, legacy_ast_param_summary_program(ctx), ctx);

    /* P0 #1 PR2 consumer migration: when the callee's body-summary
     * inventory positively proves no channel send happens, strip the
     * legacy AST analyzer's CHANNEL_ESCAPE bit. The legacy analyzer
     * can over-approximate on channel-receive-only call shapes; the
     * body-summary inventory is more precise on this dimension. */
    if (semantic_callable_summary_proves_no_send_channel(ctx, callee_decl))
        mask &= ~SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE;

    return mask;
}

bool
semantic_param_summary_has_any_escape(unsigned summary_mask)
{
    return (summary_mask & (SLOT_PARAM_SUMMARY_RETURN_ESCAPE
                            | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE
                            | SLOT_PARAM_SUMMARY_CALL_ESCAPE)) != 0;
}

bool
semantic_param_summary_has_return_escape(unsigned summary_mask)
{
    return (summary_mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0;
}

bool
semantic_param_summary_has_channel_escape(unsigned summary_mask)
{
    return (summary_mask & SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE) != 0;
}

bool
semantic_param_summary_has_call_escape(unsigned summary_mask)
{
    return (summary_mask & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0;
}
