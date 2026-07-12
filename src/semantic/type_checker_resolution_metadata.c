#include <stdlib.h>
#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_resolution_metadata_internal.h"

typedef struct TypeNameSlot {
    const char *name;
    Type **slot;
} TypeNameSlot;

static int
metadata_type_name_slot_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const TypeNameSlot *slot = (const TypeNameSlot *)entry;

    return strcmp(name, slot->name);
}

static Type *
metadata_scope_named_type(SemanticContext *ctx, ASTNode *type_node)
{
    const char *name;
    Symbol *sym;

    if (ctx == NULL || type_node == NULL || type_node->type != AST_TYPE)
        return NULL;
    if (!semantic_type_resolution_metadata_type_ref_has_no_generic_args(
            type_node))
        return NULL;

    name = ast_type_name(type_node);
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
        ASTNode *decl = semantic_find_class_decl_by_name(ctx, name);
        GenericParams *class_generics = ast_class_generic_params(decl);
        if (decl != NULL && decl->type == AST_CLASS_DECL
            && ast_generic_param_count(class_generics) > 0) {
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

    {
        size_t existing = 0;
        if (metadata_lookup_entry_index(ctx, type_node, &existing)) {
            size_t i = existing;
            bool keep_owned = owned;
            if (ctx->type_resolution_metadata.owned[i]
                && ctx->type_resolution_metadata.values[i] != resolved_type) {
                semantic_type_resolution_free_owned_type(
                    (Type *)ctx->type_resolution_metadata.values[i]);
            }
            if (ctx->type_resolution_metadata.owned[i]
                && ctx->type_resolution_metadata.values[i] == resolved_type) {
                keep_owned = true;
            }
            ctx->type_resolution_metadata.values[i] = resolved_type;
            ctx->type_resolution_metadata.owned[i] = keep_owned;
            return;
        }
    }

    if (ctx->type_resolution_metadata.count
        == ctx->type_resolution_metadata.capacity) {
        size_t new_cap;
        if (ctx->type_resolution_metadata.capacity > SIZE_MAX / 2)
        {
            if (owned)
                semantic_type_resolution_free_owned_type(resolved_type);
            return;
        }
        new_cap = ctx->type_resolution_metadata.capacity == 0
            ? 128
            : ctx->type_resolution_metadata.capacity * 2;
        if (new_cap > SIZE_MAX / sizeof(void *)
            || new_cap > SIZE_MAX / sizeof(bool)) {
            if (owned)
                semantic_type_resolution_free_owned_type(resolved_type);
            return;
        }
        new_keys = malloc(new_cap * sizeof(void *));
        new_values = malloc(new_cap * sizeof(void *));
        new_owned = malloc(new_cap * sizeof(bool));
        if (new_keys == NULL || new_values == NULL || new_owned == NULL) {
            free(new_keys);
            free(new_values);
            free(new_owned);
            if (owned)
                semantic_type_resolution_free_owned_type(resolved_type);
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

    if (!metadata_ensure_index_capacity(
            ctx, ctx->type_resolution_metadata.count + 1)) {
        if (owned)
            semantic_type_resolution_free_owned_type(resolved_type);
        return;
    }
    if (!metadata_index_insert(ctx,
                               (void *)type_node,
                               ctx->type_resolution_metadata.count)) {
        if (owned)
            semantic_type_resolution_free_owned_type(resolved_type);
        return;
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

    {
        size_t entry = 0;
        if (metadata_lookup_entry_index(ctx, type_node, &entry)) {
            ctx->type_resolution_metadata_hits++;
            return (Type *)ctx->type_resolution_metadata.values[entry];
        }
    }

    ctx->type_resolution_metadata_misses++;
    return NULL;
}

Type *
semantic_type_resolution_lookup_annotation_nullable(SemanticContext *ctx,
                                                    ASTNode *type_node)
{
    return semantic_type_resolution_lookup_resolved_type(ctx, type_node);
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
            ast_type_name(type_node));
    if (resolved != NULL) {
        semantic_type_resolution_record_resolved_type(ctx, type_node, resolved);
        return resolved;
    }

    resolved = metadata_scope_named_type(ctx, type_node);
    if (resolved != NULL)
        return resolved;

    semantic_type_resolution_try_record_stable_constructed_type(ctx, type_node);
    resolved = semantic_type_resolution_lookup_resolved_type(ctx, type_node);
    if (resolved != NULL)
        return resolved;

    return NULL;
}

Type *
semantic_type_resolution_metadata_builtin_singleton(const char *name)
{
    static const TypeNameSlot builtins[] = {
        { "Allocator", &TYPE_ALLOCATOR },
        { "Bool", &TYPE_BOOL },
        { "Double", &TYPE_DOUBLE },
        { "Duration", &TYPE_DURATION },
        { "Float", &TYPE_FLOAT },
        { "Int", &TYPE_INT },
        { "Long", &TYPE_LONG },
        { "QubitSlot", &TYPE_QUBIT },
        { "String", &TYPE_STRING },
        { "TextBuilder", &TYPE_TEXT_BUILDER },
        { "Void", &TYPE_VOID },
        { "projection", &TYPE_PROJECTION },
    };
    const size_t count = sizeof(builtins) / sizeof(builtins[0]);
    const TypeNameSlot *match;

    if (name == NULL)
        return NULL;
    match = (const TypeNameSlot *)bsearch(
        &name, builtins, count, sizeof(builtins[0]),
        metadata_type_name_slot_compare);
    return match != NULL ? *match->slot : NULL;
}

Type *
semantic_type_resolution_metadata_named_builtin_or_shell_singleton(
    const char *name)
{
    static const TypeNameSlot shells[] = {
        { "Array", &TYPE_ARRAY },
        { "Box", &TYPE_BOX },
        { "DeviceSlot", &TYPE_DEVICE_SLOT },
        { "HashMap", &TYPE_HASHMAP },
        { "List", &TYPE_LIST },
        { "Option", &TYPE_OPTION },
        { "Queue", &TYPE_QUEUE },
        { "Rc", &TYPE_RC },
        { "RemoteFuture", &TYPE_REMOTE_FUTURE },
        { "Set", &TYPE_SET },
        { "Slice", &TYPE_SLICE },
        { "Weak", &TYPE_WEAK },
    };
    Type *builtin = semantic_type_resolution_metadata_builtin_singleton(name);
    const size_t count = sizeof(shells) / sizeof(shells[0]);
    const TypeNameSlot *match;

    if (builtin != NULL)
        return builtin;
    if (name == NULL)
        return NULL;
    match = (const TypeNameSlot *)bsearch(
        &name, shells, count, sizeof(shells[0]),
        metadata_type_name_slot_compare);
    return match != NULL ? *match->slot : NULL;
}
