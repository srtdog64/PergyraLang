#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "type_checker_internal.h"

static bool
stage_alias_trace_enabled(void)
{
    static int cached = -1;

    if (cached < 0) {
        const char *value = getenv("PGY_TYPE_RES_ALIAS_TRACE");
        cached = value != NULL && value[0] != '\0' && strcmp(value, "0") != 0;
    }
    return cached != 0;
}

static bool
stage_alias_fallback_name_seen(SemanticContext *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return false;
    for (size_t i = 0; i < ctx->type_resolution_stage_alias_fallback_name_count; i++) {
        if (ctx->type_resolution_stage_alias_fallback_names[i] != NULL
            && strcmp(ctx->type_resolution_stage_alias_fallback_names[i], name) == 0) {
            return true;
        }
    }
    return false;
}

static bool
stage_record_unique_alias_fallback(SemanticContext *ctx, const char *name)
{
    char **grown;
    char *copy;
    size_t new_cap;

    if (ctx == NULL || name == NULL)
        return false;
    if (stage_alias_fallback_name_seen(ctx, name))
        return false;

    if (ctx->type_resolution_stage_alias_fallback_name_count
        == ctx->type_resolution_stage_alias_fallback_name_capacity) {
        new_cap = ctx->type_resolution_stage_alias_fallback_name_capacity == 0
            ? 8
            : ctx->type_resolution_stage_alias_fallback_name_capacity * 2;
        grown = realloc(ctx->type_resolution_stage_alias_fallback_names,
                        new_cap * sizeof(char *));
        if (grown == NULL)
            return false;
        ctx->type_resolution_stage_alias_fallback_names = grown;
        ctx->type_resolution_stage_alias_fallback_name_capacity = new_cap;
    }

    copy = pergyra_strdup(name);
    if (copy == NULL)
        return false;
    ctx->type_resolution_stage_alias_fallback_names[
        ctx->type_resolution_stage_alias_fallback_name_count++] = copy;
    return true;
}

void
semantic_stage_record_alias_diagnostic_fallback(ASTNode *alias_decl,
                                                SemanticContext *ctx)
{
    const char *alias_name;
    bool unique_fallback;

    if (alias_decl == NULL || ctx == NULL || alias_decl->type != AST_TYPE_ALIAS)
        return;

    alias_name = alias_decl->data.type_alias.name;
    unique_fallback = stage_record_unique_alias_fallback(ctx, alias_name);
    if (unique_fallback) {
        ctx->type_resolution_stage_alias_diagnostic_fallback_count++;
        ctx->type_resolution_stage_alias_fallback_unresolved_count++;
    }

    if (stage_alias_trace_enabled()) {
        fprintf(stderr,
                "[type-res-alias-fallback] alias=%s unique=%d line=%u column=%u\n",
                alias_name != NULL ? alias_name : "<alias>",
                unique_fallback ? 1 : 0,
                alias_decl->line,
                alias_decl->column);
    }
}
