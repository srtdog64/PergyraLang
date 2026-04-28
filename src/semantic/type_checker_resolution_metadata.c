#include <stdlib.h>
#include <string.h>

#include "type_checker_internal.h"

static Type *
metadata_scope_named_type(SemanticContext *ctx, ASTNode *type_node)
{
    const char *name;
    Symbol *sym;

    if (ctx == NULL || type_node == NULL || type_node->type != AST_TYPE)
        return NULL;
    if (type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0)
        return NULL;

    name = type_node->data.type.name;
    if (name == NULL)
        return NULL;

    {
        Type *alias_type = semantic_type_resolution_metadata_alias_type(ctx,
                                                                        type_node);
        if (alias_type != NULL)
            return alias_type;
    }

    sym = scope_lookup(ctx->scope, name);
    if (sym == NULL || sym->type == NULL || sym->type == TYPE_UNKNOWN)
        return NULL;

    if (sym->kind == SYMBOL_CLASS) {
        ASTNode *decl = ctx->program_root != NULL
            ? find_type_decl_by_name(ctx->program_root, name)
            : NULL;
        if (decl != NULL && decl->type == AST_CLASS_DECL
            && decl->data.class_decl.generic_params != NULL
            && decl->data.class_decl.generic_params->count > 0) {
            return NULL;
        }
        semantic_type_resolution_record_named_dependency(
            ctx, type_node, name, TYPE_RES_NODE_DECL, decl, name,
            "metadata named-type lookup");
        semantic_type_resolution_record_resolved_type(ctx, type_node, sym->type);
        return sym->type;
    }

    if (sym->kind != SYMBOL_TYPE_PARAM) {
        semantic_type_resolution_record_named_dependency(
            ctx, type_node, name, TYPE_RES_NODE_DECL, NULL, name,
            "metadata scope-type lookup");
        semantic_type_resolution_record_resolved_type(ctx, type_node, sym->type);
        return sym->type;
    }

    semantic_type_resolution_record_named_dependency(
        ctx, type_node, name, TYPE_RES_NODE_GENERIC_PARAM, NULL, name,
        "metadata scope-type lookup");
    semantic_type_resolution_record_resolved_type(ctx, type_node, sym->type);
    return sym->type;
}

static void
semantic_type_resolution_record_resolved_type_impl(SemanticContext *ctx,
                                                   ASTNode *type_node,
                                                   Type *resolved_type,
                                                   bool owned)
{
    void **new_keys;
    void **new_values;
    bool *new_owned;

    if (ctx == NULL || type_node == NULL || resolved_type == NULL
        || resolved_type == TYPE_UNKNOWN) {
        return;
    }

    for (size_t i = 0; i < ctx->type_resolution_metadata.count; i++) {
        if (ctx->type_resolution_metadata.keys[i] == (void *)type_node) {
            if (ctx->type_resolution_metadata.owned[i]
                && ctx->type_resolution_metadata.values[i] != resolved_type) {
                semantic_type_resolution_free_owned_type(
                    (Type *)ctx->type_resolution_metadata.values[i]);
            }
            ctx->type_resolution_metadata.values[i] = resolved_type;
            ctx->type_resolution_metadata.owned[i] = owned;
            return;
        }
    }

    if (ctx->type_resolution_metadata.count
        == ctx->type_resolution_metadata.capacity) {
        size_t new_cap = ctx->type_resolution_metadata.capacity == 0
            ? 128
            : ctx->type_resolution_metadata.capacity * 2;
        new_keys = malloc(new_cap * sizeof(void *));
        new_values = malloc(new_cap * sizeof(void *));
        new_owned = malloc(new_cap * sizeof(bool));
        if (new_keys == NULL || new_values == NULL || new_owned == NULL) {
            free(new_keys);
            free(new_values);
            free(new_owned);
            return;
        }
        for (size_t i = 0; i < ctx->type_resolution_metadata.count; i++) {
            new_keys[i] = ctx->type_resolution_metadata.keys[i];
            new_values[i] = ctx->type_resolution_metadata.values[i];
            new_owned[i] = ctx->type_resolution_metadata.owned[i];
        }
        free(ctx->type_resolution_metadata.keys);
        free(ctx->type_resolution_metadata.values);
        free(ctx->type_resolution_metadata.owned);
        ctx->type_resolution_metadata.keys = new_keys;
        ctx->type_resolution_metadata.values = new_values;
        ctx->type_resolution_metadata.owned = new_owned;
        ctx->type_resolution_metadata.capacity = new_cap;
    }

    ctx->type_resolution_metadata.keys[ctx->type_resolution_metadata.count] =
        (void *)type_node;
    ctx->type_resolution_metadata.values[ctx->type_resolution_metadata.count] =
        resolved_type;
    ctx->type_resolution_metadata.owned[ctx->type_resolution_metadata.count] =
        owned;
    ctx->type_resolution_metadata.count++;
}

void
semantic_type_resolution_record_resolved_type(SemanticContext *ctx,
                                              ASTNode *type_node,
                                              Type *resolved_type)
{
    semantic_type_resolution_record_resolved_type_impl(
        ctx, type_node, resolved_type, false);
}

void
semantic_type_resolution_record_owned_resolved_type(SemanticContext *ctx,
                                                    ASTNode *type_node,
                                                    Type *resolved_type)
{
    semantic_type_resolution_record_resolved_type_impl(
        ctx, type_node, resolved_type, true);
}

Type *
semantic_type_resolution_lookup_resolved_type(SemanticContext *ctx,
                                              ASTNode *type_node)
{
    if (ctx == NULL || type_node == NULL)
        return NULL;

    for (size_t i = 0; i < ctx->type_resolution_metadata.count; i++) {
        if (ctx->type_resolution_metadata.keys[i] == (void *)type_node) {
            ctx->type_resolution_metadata_hits++;
            return (Type *)ctx->type_resolution_metadata.values[i];
        }
    }

    ctx->type_resolution_metadata_misses++;
    return NULL;
}

Type *
semantic_type_resolution_lookup_or_materialize(SemanticContext *ctx,
                                               ASTNode *type_node)
{
    Type *resolved;

    if (type_node == NULL)
        return NULL;

    resolved = semantic_type_resolution_lookup_resolved_type(ctx, type_node);
    if (resolved != NULL)
        return resolved;

    if (type_node->type == AST_TYPE)
        resolved = semantic_type_resolution_metadata_builtin_singleton(
            type_node->data.type.name);
    if (resolved != NULL) {
        semantic_type_resolution_record_resolved_type(ctx, type_node, resolved);
        return resolved;
    }

    if (semantic_type_resolution_reject_invalid_stable_shell_arity(ctx,
                                                                   type_node))
        return TYPE_UNKNOWN;

    resolved = metadata_scope_named_type(ctx, type_node);
    if (resolved != NULL)
        return resolved;

    semantic_type_resolution_try_record_stable_constructed_type(ctx,
                                                                type_node);
    resolved = semantic_type_resolution_lookup_resolved_type(ctx, type_node);
    if (resolved != NULL)
        return resolved;

    if (semantic_type_resolution_reject_invalid_stable_constructed_type(ctx,
                                                                        type_node))
        return TYPE_UNKNOWN;

    if (semantic_type_resolution_reject_unknown_bare_named_type(ctx,
                                                               type_node))
        return TYPE_UNKNOWN;

    /* Strict beta keeps unresolved metadata materialization explicit. Current
     * DAG smoke requires this path to be dormant (materializer_fallbacks == 0);
     * resolver-inventory smoke rejects recursive fallback reintroduction.
     */
    semantic_type_resolution_record_materializer_fallback(ctx, type_node);
    return NULL;
}

Type *
semantic_type_resolution_lookup_metadata_type_ref(SemanticContext *ctx,
                                                  ASTNode *type_node)
{
    Type *resolved;

    if (type_node == NULL)
        return NULL;

    resolved = semantic_type_resolution_lookup_resolved_type(ctx, type_node);
    if (resolved != NULL)
        return resolved;

    if (type_node->type == AST_TYPE)
        resolved = semantic_type_resolution_metadata_builtin_singleton(
            type_node->data.type.name);
    if (resolved != NULL) {
        semantic_type_resolution_record_resolved_type(ctx, type_node, resolved);
        return resolved;
    }

    resolved = metadata_scope_named_type(ctx, type_node);
    if (resolved != NULL)
        return resolved;

    return NULL;
}

Type *
semantic_type_resolution_metadata_builtin_singleton(const char *name)
{
    if (name == NULL)
        return NULL;
    if (strcmp(name, "Int") == 0)
        return TYPE_INT;
    if (strcmp(name, "Long") == 0)
        return TYPE_LONG;
    if (strcmp(name, "Float") == 0)
        return TYPE_FLOAT;
    if (strcmp(name, "Double") == 0)
        return TYPE_DOUBLE;
    if (strcmp(name, "Bool") == 0)
        return TYPE_BOOL;
    if (strcmp(name, "String") == 0)
        return TYPE_STRING;
    if (strcmp(name, "QubitSlot") == 0)
        return TYPE_QUBIT;
    if (strcmp(name, "Void") == 0)
        return TYPE_VOID;
    if (strcmp(name, "Allocator") == 0)
        return TYPE_ALLOCATOR;
    return NULL;
}
