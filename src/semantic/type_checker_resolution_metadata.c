#include <stdlib.h>
#include <string.h>

#include "type_checker_internal.h"

static void
semantic_type_resolution_free_owned_type(Type *type)
{
    if (type == NULL)
        return;
    free(type->name);
    if (type->kind == TYPE_KIND_CONSTRUCTED)
        free(type->data.constructed.args);
    free(type);
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

void
semantic_type_resolution_free_metadata(SemanticContext *ctx)
{
    if (ctx == NULL)
        return;
    for (size_t i = 0; i < ctx->type_resolution_metadata.count; i++) {
        if (ctx->type_resolution_metadata.owned[i]) {
            semantic_type_resolution_free_owned_type(
                (Type *)ctx->type_resolution_metadata.values[i]);
        }
    }
    free(ctx->type_resolution_metadata.keys);
    free(ctx->type_resolution_metadata.values);
    free(ctx->type_resolution_metadata.owned);
    ctx->type_resolution_metadata.keys = NULL;
    ctx->type_resolution_metadata.values = NULL;
    ctx->type_resolution_metadata.owned = NULL;
    ctx->type_resolution_metadata.count = 0;
    ctx->type_resolution_metadata.capacity = 0;
}

static Type *
stable_constructed_constructor(const char *name, size_t argc)
{
    if (name == NULL)
        return NULL;
    if (argc == 1 && strcmp(name, "List") == 0)
        return TYPE_LIST;
    if (argc == 1 && strcmp(name, "Set") == 0)
        return TYPE_SET;
    if (argc == 2 && strcmp(name, "HashMap") == 0)
        return TYPE_HASHMAP;
    if (argc == 1 && strcmp(name, "Option") == 0)
        return TYPE_OPTION;
    if ((argc == 1 || argc == 2) && strcmp(name, "Result") == 0)
        return TYPE_RESULT;
    return NULL;
}

static Type *
metadata_builtin_singleton(const char *name)
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
    return NULL;
}

void
semantic_type_resolution_try_record_stable_constructed_type(SemanticContext *ctx,
                                                            ASTNode *type_node)
{
    GenericParams *args_node;
    Type *constructor;
    Type *args[2];
    Type *result;

    if (ctx == NULL || type_node == NULL || type_node->type != AST_TYPE)
        return;
    args_node = type_node->data.type.generic_args;
    if (args_node == NULL || args_node->count == 0 || args_node->count > 2)
        return;

    constructor = stable_constructed_constructor(
        type_node->data.type.name,
        args_node->count);
    if (constructor == NULL)
        return;

    for (size_t i = 0; i < args_node->count; i++) {
        GenericParam *gp = args_node->params[i];
        ASTNode *arg_type = gp != NULL ? gp->constraint : NULL;
        args[i] = semantic_type_resolution_lookup_resolved_type(ctx, arg_type);
        if (args[i] == NULL && gp != NULL)
            args[i] = metadata_builtin_singleton(gp->name);
        if (args[i] == NULL)
            return;
    }

    if (constructor == TYPE_HASHMAP
        && !type_equals(args[0], TYPE_STRING)
        && !type_equals(args[0], TYPE_INT)) {
        return;
    }

    result = type_create_constructed(constructor, args, args_node->count);
    if (result == NULL)
        return;
    semantic_type_resolution_record_owned_resolved_type(ctx, type_node, result);
}
