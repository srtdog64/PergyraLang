#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diag_codes.h"
#include "type_checker_internal.h"

static void
metadata_record_named_dead_end_diagnostic(SemanticContext *ctx,
                                          ASTNode *type_node)
{
    const char *name;
    Symbol *sym;

    if (ctx == NULL || type_node == NULL || type_node->type != AST_TYPE)
        return;

    name = ast_type_name(type_node);
    if (semantic_type_resolution_metadata_stable_builtin_shell_arity(
            name, NULL, NULL)) {
        ctx->type_resolution_metadata_unresolved_named_builtin_shell++;
        return;
    }

    sym = name != NULL ? scope_lookup(ctx->scope, name) : NULL;
    if (sym != NULL) {
        if (sym->kind == SYMBOL_CLASS) {
            ASTNode *decl = semantic_find_class_decl_by_name(ctx, name);
            GenericParams *class_generics = ast_class_generic_params(decl);
            if (decl != NULL && decl->type == AST_CLASS_DECL
                && ast_generic_param_count(class_generics) > 0) {
                ctx->type_resolution_metadata_unresolved_named_generic_class++;
                return;
            }
        } else if (sym->kind != SYMBOL_TYPE_PARAM) {
            ctx->type_resolution_metadata_unresolved_named_non_class_symbol++;
            return;
        }
    }

    if (semantic_find_type_alias_decl_by_name(ctx, name) != NULL) {
        ctx->type_resolution_metadata_unresolved_named_alias++;
        return;
    }

    ctx->type_resolution_metadata_unresolved_named_missing_symbol++;
}

static bool
metadata_trace_dead_end_diagnostic_enabled(void)
{
    static int cached = -1;

    if (cached < 0) {
        const char *value = getenv("PGY_TYPE_RES_DEAD_END_TRACE");
        cached = value != NULL && value[0] != '\0' && strcmp(value, "0") != 0;
    }
    return cached != 0;
}

static const char *
metadata_dead_end_ast_kind(const ASTNode *type_node)
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
metadata_trace_dead_end_diagnostic(ASTNode *type_node)
{
    const char *name = NULL;
    size_t generic_count = 0;

    if (!metadata_trace_dead_end_diagnostic_enabled())
        return;
    if (type_node != NULL && type_node->type == AST_TYPE) {
        name = ast_type_name(type_node);
        if (ast_type_generic_args(type_node) != NULL)
            generic_count = ast_generic_param_count(
                ast_type_generic_args(type_node));
    }
    fprintf(stderr,
            "[type-res-dead-end] kind=%s name=%s generic_args=%llu line=%u column=%u\n",
            metadata_dead_end_ast_kind(type_node),
            name != NULL ? name : "<none>",
            (unsigned long long)generic_count,
            type_node != NULL ? type_node->line : 0,
            type_node != NULL ? type_node->column : 0);
}

static const char *
metadata_dead_end_type_name(ASTNode *type_node)
{
    if (type_node == NULL)
        return "<null>";
    if (ast_type_name(type_node) != NULL)
        return ast_type_name(type_node);
    return metadata_dead_end_ast_kind(type_node);
}

void
semantic_type_resolution_record_metadata_dead_end_diagnostic(SemanticContext *ctx,
                                                             ASTNode *type_node)
{
    if (ctx == NULL)
        return;

    metadata_trace_dead_end_diagnostic(type_node);
    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_UNKNOWN_TYPE,
        PGY_CAUSE_TYPE_UNKNOWN,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        type_node,
        "Type-resolution DAG could not materialize type metadata for '%s'.\n"
        "Reason: beta type resolution requires a graph-backed metadata fact; recursive resolver fallback is retired.\n"
        "Fix: stage this type in the declaration graph or add an owner-local metadata materializer before using it.",
        metadata_dead_end_type_name(type_node));
    ctx->type_resolution_metadata_dead_ends++;
    if (type_node == NULL) {
        ctx->type_resolution_metadata_unresolved_other++;
        return;
    }

    if (type_node->type == AST_TYPE) {
        if (ast_type_name(type_node) != NULL) {
            GenericParams *args = ast_type_generic_args(type_node);
            if (ast_generic_param_count(args) > 0) {
                ctx->type_resolution_metadata_unresolved_generic_named++;
            } else {
                ctx->type_resolution_metadata_unresolved_named++;
                metadata_record_named_dead_end_diagnostic(ctx, type_node);
            }
            return;
        }
        if (ast_type_tuple_element_count(type_node) > 0) {
            ctx->type_resolution_metadata_unresolved_compound++;
            return;
        }
    }

    if (type_node->type == AST_CHANNEL_TYPE
        || type_node->type == AST_FUTURE_TYPE
        || type_node->type == AST_EVENT_HANDLER_TYPE) {
        ctx->type_resolution_metadata_unresolved_compound++;
        return;
    }

    ctx->type_resolution_metadata_unresolved_other++;
}
