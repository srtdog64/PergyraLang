#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../common/env_flags.h"
#include "../common/string_compat.h"
#include "type_checker_internal.h"

static bool
stage_alias_trace_enabled(void)
{
    return pgy_env_value_is_truthy(getenv("PGY_TYPE_RES_ALIAS_TRACE"));
}

static void
stage_alias_discard_quiet_diagnostics(SemanticContext *ctx,
                                      size_t saved_diag,
                                      bool saved_error)
{
    if (ctx == NULL || ctx->diagnostic_count <= saved_diag)
        return;

    for (size_t i = saved_diag; i < ctx->diagnostic_count; i++) {
        if (ctx->diagnostics[i] != NULL) {
            free(ctx->diagnostics[i]->message);
            free(ctx->diagnostics[i]);
            ctx->diagnostics[i] = NULL;
        }
    }
    ctx->diagnostic_count = saved_diag;
    ctx->has_error = saved_error;
}

static Type *
stage_alias_lookup_metadata_target_quiet(ASTNode *target_type,
                                         SemanticContext *ctx)
{
    size_t saved_diag;
    bool saved_error;
    Type *resolved;

    if (target_type == NULL || ctx == NULL)
        return NULL;

    saved_diag = ctx->diagnostic_count;
    saved_error = ctx->has_error;
    resolved = semantic_type_resolution_lookup_metadata_type_ref(ctx, target_type);
    if (ctx->diagnostic_count > saved_diag) {
        stage_alias_discard_quiet_diagnostics(ctx, saved_diag, saved_error);
        return TYPE_UNKNOWN;
    }
    return resolved;
}

Type *
semantic_stage_resolve_alias_target_quiet(ASTNode *alias_decl,
                                          SemanticContext *ctx)
{
    ASTNode *target_type;
    Type *resolved;

    if (alias_decl == NULL || alias_decl->type != AST_TYPE_ALIAS || ctx == NULL)
        return TYPE_UNKNOWN;

    target_type = ast_type_alias_target_type(alias_decl);
    if (target_type == NULL)
        return TYPE_UNKNOWN;

    resolved = stage_alias_lookup_metadata_target_quiet(target_type, ctx);
    if (resolved != NULL && resolved != TYPE_UNKNOWN)
        return resolved;

    return TYPE_UNKNOWN;
}

static bool
stage_alias_diagnostic_name_seen(SemanticContext *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return false;
    for (size_t i = 0; i < ctx->type_resolution_stage_alias_diagnostic_name_count; i++) {
        if (ctx->type_resolution_stage_alias_diagnostic_names[i] != NULL
            && strcmp(ctx->type_resolution_stage_alias_diagnostic_names[i], name) == 0) {
            return true;
        }
    }
    return false;
}

static bool
stage_record_unique_alias_diagnostic(SemanticContext *ctx, const char *name)
{
    char **grown;
    char *copy;
    size_t new_cap;

    if (ctx == NULL || name == NULL)
        return false;
    if (stage_alias_diagnostic_name_seen(ctx, name))
        return false;

    if (ctx->type_resolution_stage_alias_diagnostic_name_count
        == ctx->type_resolution_stage_alias_diagnostic_name_capacity) {
        if (ctx->type_resolution_stage_alias_diagnostic_name_capacity
            > SIZE_MAX / 2) {
            return false;
        }
        new_cap = ctx->type_resolution_stage_alias_diagnostic_name_capacity == 0
            ? 8
            : ctx->type_resolution_stage_alias_diagnostic_name_capacity * 2;
        if (new_cap > SIZE_MAX / sizeof(char *))
            return false;
        grown = realloc(ctx->type_resolution_stage_alias_diagnostic_names,
                        new_cap * sizeof(char *));
        if (grown == NULL)
            return false;
        ctx->type_resolution_stage_alias_diagnostic_names = grown;
        ctx->type_resolution_stage_alias_diagnostic_name_capacity = new_cap;
    }

    copy = pergyra_strdup(name);
    if (copy == NULL)
        return false;
    ctx->type_resolution_stage_alias_diagnostic_names[
        ctx->type_resolution_stage_alias_diagnostic_name_count++] = copy;
    return true;
}

void
semantic_stage_record_alias_diagnostic_unresolved(ASTNode *alias_decl,
                                                  SemanticContext *ctx)
{
    const char *alias_name;
    bool unique_diagnostic;

    if (alias_decl == NULL || ctx == NULL || alias_decl->type != AST_TYPE_ALIAS)
        return;

    alias_name = ast_type_alias_name(alias_decl);
    unique_diagnostic = stage_record_unique_alias_diagnostic(ctx, alias_name);
    if (unique_diagnostic) {
        ctx->type_resolution_stage_alias_diagnostic_unresolved_count++;
        ctx->type_resolution_stage_alias_diagnostic_cycle_count++;
    }

    if (stage_alias_trace_enabled()) {
        fprintf(stderr,
                "[type-res-alias-diagnostic] alias=%s unique=%d line=%u column=%u\n",
                alias_name != NULL ? alias_name : "<alias>",
                unique_diagnostic ? 1 : 0,
                alias_decl->line,
                alias_decl->column);
    }
}

void
semantic_stage_type_alias_decl(ASTNode *decl, SemanticContext *ctx)
{
    Symbol *sym;
    Type *alias_type;

    if (decl == NULL || decl->type != AST_TYPE_ALIAS || ctx == NULL)
        return;
    if (ast_type_alias_name(decl) == NULL)
        return;

    sym = scope_lookup_current(ctx->scope, ast_type_alias_name(decl));
    if (sym == NULL)
        return;

    alias_type = semantic_stage_resolve_alias_target_quiet(decl, ctx);
    if (alias_type != TYPE_UNKNOWN) {
        ctx->type_resolution_stage_alias_materialized_count++;
        sym->type = alias_type;
    } else {
        semantic_stage_record_alias_diagnostic_unresolved(decl, ctx);
        sym->type = TYPE_UNKNOWN;
    }
}
