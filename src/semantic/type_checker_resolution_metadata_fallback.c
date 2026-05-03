#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diag_codes.h"
#include "type_checker_internal.h"

static void
metadata_record_named_materializer_fallback(SemanticContext *ctx,
                                            ASTNode *type_node)
{
    const char *name;
    Symbol *sym;

    if (ctx == NULL || type_node == NULL || type_node->type != AST_TYPE)
        return;

    name = type_node->data.type.name;
    if (semantic_type_resolution_metadata_stable_builtin_shell_arity(
            name, NULL, NULL)) {
        ctx->type_resolution_metadata_fallback_named_builtin_shell++;
        return;
    }

    sym = name != NULL ? scope_lookup(ctx->scope, name) : NULL;
    if (sym != NULL) {
        if (sym->kind == SYMBOL_CLASS) {
            ASTNode *decl = ctx->program_root != NULL
                ? find_type_decl_by_name(ctx->program_root, name)
                : NULL;
            if (decl != NULL && decl->type == AST_CLASS_DECL
                && decl->data.class_decl.generic_params != NULL
                && decl->data.class_decl.generic_params->count > 0) {
                ctx->type_resolution_metadata_fallback_named_generic_class++;
                return;
            }
        } else if (sym->kind != SYMBOL_TYPE_PARAM) {
            ctx->type_resolution_metadata_fallback_named_non_class_symbol++;
            return;
        }
    }

    if (ctx->program_root != NULL && find_type_alias_decl(ctx->program_root, name) != NULL) {
        ctx->type_resolution_metadata_fallback_named_alias++;
        return;
    }

    ctx->type_resolution_metadata_fallback_named_missing_symbol++;
}

static bool
metadata_trace_materializer_fallback_enabled(void)
{
    static int cached = -1;

    if (cached < 0) {
        const char *value = getenv("PGY_TYPE_RES_FALLBACK_TRACE");
        cached = value != NULL && value[0] != '\0' && strcmp(value, "0") != 0;
    }
    return cached != 0;
}

static const char *
metadata_fallback_ast_kind(const ASTNode *type_node)
{
    if (type_node == NULL)
        return "<null>";
    switch (type_node->type) {
    case AST_TYPE:
        return "AST_TYPE";
    case AST_CHANNEL_TYPE:
        return "AST_CHANNEL_TYPE";
    case AST_FUTURE_TYPE:
        return "AST_FUTURE_TYPE";
    case AST_EVENT_HANDLER_TYPE:
        return "AST_EVENT_HANDLER_TYPE";
    default:
        return "AST_OTHER";
    }
}

static void
metadata_trace_materializer_fallback(ASTNode *type_node)
{
    const char *name = NULL;
    size_t generic_count = 0;

    if (!metadata_trace_materializer_fallback_enabled())
        return;
    if (type_node != NULL && type_node->type == AST_TYPE) {
        name = type_node->data.type.name;
        if (type_node->data.type.generic_args != NULL)
            generic_count = type_node->data.type.generic_args->count;
    }
    fprintf(stderr,
            "[type-res-fallback] kind=%s name=%s generic_args=%llu line=%u column=%u\n",
            metadata_fallback_ast_kind(type_node),
            name != NULL ? name : "<none>",
            (unsigned long long)generic_count,
            type_node != NULL ? type_node->line : 0,
            type_node != NULL ? type_node->column : 0);
}

static const char *
metadata_fallback_type_name(ASTNode *type_node)
{
    if (type_node == NULL)
        return "<null>";
    if (type_node->type == AST_TYPE && type_node->data.type.name != NULL)
        return type_node->data.type.name;
    return metadata_fallback_ast_kind(type_node);
}

void
semantic_type_resolution_record_metadata_dead_end_diagnostic(SemanticContext *ctx,
                                                             ASTNode *type_node)
{
    if (ctx == NULL)
        return;

    metadata_trace_materializer_fallback(type_node);
    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_UNKNOWN_TYPE,
        PGY_CAUSE_TYPE_UNKNOWN,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        type_node,
        "Type-resolution DAG could not materialize type metadata for '%s'.\n"
        "Reason: beta type resolution requires a graph-backed metadata fact; recursive resolver fallback is retired.\n"
        "Fix: stage this type in the declaration graph or add an owner-local metadata materializer before using it.",
        metadata_fallback_type_name(type_node));
    ctx->type_resolution_metadata_materializer_fallbacks++;
    if (type_node == NULL) {
        ctx->type_resolution_metadata_fallback_other++;
        return;
    }

    if (type_node->type == AST_TYPE) {
        if (type_node->data.type.name != NULL) {
            GenericParams *args = type_node->data.type.generic_args;
            if (args != NULL && args->count > 0) {
                ctx->type_resolution_metadata_fallback_generic_named++;
            } else {
                ctx->type_resolution_metadata_fallback_named++;
                metadata_record_named_materializer_fallback(ctx, type_node);
            }
            return;
        }
        if (type_node->data.type.tuple_elements != NULL
            && type_node->data.type.tuple_element_count > 0) {
            ctx->type_resolution_metadata_fallback_compound++;
            return;
        }
    }

    if (type_node->type == AST_CHANNEL_TYPE
        || type_node->type == AST_FUTURE_TYPE
        || type_node->type == AST_EVENT_HANDLER_TYPE) {
        ctx->type_resolution_metadata_fallback_compound++;
        return;
    }

    ctx->type_resolution_metadata_fallback_other++;
}
