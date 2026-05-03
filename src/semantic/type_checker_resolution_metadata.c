#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_resolution_metadata_internal.h"

typedef struct TypeNameSlot {
    const char *name;
    Type **slot;
} TypeNameSlot;

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

static size_t
metadata_key_hash(const void *key)
{
    uintptr_t value = (uintptr_t)key;

    value >>= 3;
    value ^= value >> 16;
    value *= (uintptr_t)0x7feb352dU;
    value ^= value >> 15;
    return (size_t)value;
}

static bool
metadata_index_insert(SemanticContext *ctx, void *key, size_t entry_index)
{
    size_t mask;
    size_t slot;

    if (ctx == NULL || key == NULL
        || ctx->type_resolution_metadata.index_capacity == 0) {
        return false;
    }

    mask = ctx->type_resolution_metadata.index_capacity - 1;
    slot = metadata_key_hash(key) & mask;
    for (size_t probe = 0;
         probe < ctx->type_resolution_metadata.index_capacity;
         probe++) {
        void *existing = ctx->type_resolution_metadata.index_keys[slot];
        if (existing == NULL || existing == key) {
            ctx->type_resolution_metadata.index_keys[slot] = key;
            ctx->type_resolution_metadata.index_entries[slot] = entry_index + 1;
            return true;
        }
        slot = (slot + 1) & mask;
    }
    return false;
}

static bool
metadata_rebuild_index(SemanticContext *ctx, size_t capacity)
{
    void **keys;
    size_t *entries;

    if (ctx == NULL || capacity == 0)
        return false;

    keys = calloc(capacity, sizeof(void *));
    entries = calloc(capacity, sizeof(size_t));
    if (keys == NULL || entries == NULL) {
        free(keys);
        free(entries);
        return false;
    }

    free(ctx->type_resolution_metadata.index_keys);
    free(ctx->type_resolution_metadata.index_entries);
    ctx->type_resolution_metadata.index_keys = keys;
    ctx->type_resolution_metadata.index_entries = entries;
    ctx->type_resolution_metadata.index_capacity = capacity;

    for (size_t i = 0; i < ctx->type_resolution_metadata.count; i++) {
        if (!metadata_index_insert(ctx,
                                   ctx->type_resolution_metadata.keys[i],
                                   i)) {
            return false;
        }
    }
    return true;
}

static bool
metadata_ensure_index_capacity(SemanticContext *ctx, size_t next_count)
{
    size_t new_capacity;

    if (ctx == NULL)
        return false;
    if (ctx->type_resolution_metadata.index_capacity != 0
        && next_count * 2 < ctx->type_resolution_metadata.index_capacity) {
        return true;
    }

    new_capacity = ctx->type_resolution_metadata.index_capacity == 0
        ? 256
        : ctx->type_resolution_metadata.index_capacity * 2;
    while (next_count * 2 >= new_capacity) {
        if (new_capacity > SIZE_MAX / 2)
            return false;
        new_capacity *= 2;
    }
    return metadata_rebuild_index(ctx, new_capacity);
}

static bool
metadata_lookup_entry_index(SemanticContext *ctx, ASTNode *type_node,
                            size_t *out_index)
{
    size_t mask;
    size_t slot;
    void *key = (void *)type_node;

    if (out_index != NULL)
        *out_index = 0;
    if (ctx == NULL || type_node == NULL)
        return false;
    if (ctx->type_resolution_metadata.count == 0)
        return false;
    if (ctx->type_resolution_metadata.index_capacity == 0
        && !metadata_ensure_index_capacity(ctx,
                                           ctx->type_resolution_metadata.count)) {
        return false;
    }

    mask = ctx->type_resolution_metadata.index_capacity - 1;
    slot = metadata_key_hash(key) & mask;
    for (size_t probe = 0;
         probe < ctx->type_resolution_metadata.index_capacity;
         probe++) {
        void *existing = ctx->type_resolution_metadata.index_keys[slot];
        if (existing == NULL)
            return false;
        if (existing == key) {
            size_t entry = ctx->type_resolution_metadata.index_entries[slot];
            if (entry == 0)
                return false;
            if (out_index != NULL)
                *out_index = entry - 1;
            return true;
        }
        slot = (slot + 1) & mask;
    }
    return false;
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

    if (!metadata_ensure_index_capacity(
            ctx, ctx->type_resolution_metadata.count + 1)) {
        return;
    }
    ctx->type_resolution_metadata.keys[ctx->type_resolution_metadata.count] =
        (void *)type_node;
    ctx->type_resolution_metadata.values[ctx->type_resolution_metadata.count] =
        resolved_type;
    ctx->type_resolution_metadata.owned[ctx->type_resolution_metadata.count] =
        owned;
    if (!metadata_index_insert(ctx,
                               (void *)type_node,
                               ctx->type_resolution_metadata.count)) {
        return;
    }
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
semantic_type_resolution_lookup_annotation_or_unknown(SemanticContext *ctx,
                                                      ASTNode *type_node)
{
    Type *resolved;

    if (ctx == NULL || type_node == NULL)
        return TYPE_UNKNOWN;

    resolved = semantic_type_resolution_lookup_annotation_nullable(ctx,
                                                                   type_node);
    return resolved != NULL ? resolved : TYPE_UNKNOWN;
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
    semantic_type_resolution_record_metadata_dead_end_diagnostic(ctx, type_node);
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

    semantic_type_resolution_try_record_stable_constructed_type(ctx, type_node);
    resolved = semantic_type_resolution_lookup_resolved_type(ctx, type_node);
    if (resolved != NULL)
        return resolved;

    return NULL;
}

Type *
semantic_type_resolution_lookup_type_ref_or_materialize(SemanticContext *ctx,
                                                        ASTNode *type_node)
{
    Type *resolved = semantic_type_resolution_lookup_metadata_type_ref(ctx,
                                                                       type_node);
    return resolved != NULL
        ? resolved
        : semantic_type_resolution_lookup_or_materialize(ctx, type_node);
}

Type *
semantic_type_resolution_metadata_builtin_singleton(const char *name)
{
    static const TypeNameSlot builtins[] = {
        { "Allocator", &TYPE_ALLOCATOR },
        { "Bool", &TYPE_BOOL },
        { "Double", &TYPE_DOUBLE },
        { "Float", &TYPE_FLOAT },
        { "Int", &TYPE_INT },
        { "Long", &TYPE_LONG },
        { "QubitSlot", &TYPE_QUBIT },
        { "String", &TYPE_STRING },
        { "Void", &TYPE_VOID },
    };
    const size_t count = sizeof(builtins) / sizeof(builtins[0]);

    for (size_t i = 0; name != NULL && i < count; i++) {
        if (strcmp(name, builtins[i].name) == 0)
            return *builtins[i].slot;
    }
    return NULL;
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

    if (builtin != NULL)
        return builtin;
    for (size_t i = 0; name != NULL && i < count; i++) {
        if (strcmp(name, shells[i].name) == 0)
            return *shells[i].slot;
    }
    return NULL;
}
