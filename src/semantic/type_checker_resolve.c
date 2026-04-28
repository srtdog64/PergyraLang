#include <stdlib.h>
#include <string.h>

#include "type_checker_internal.h"
#include "diag_codes.h"

static Type *
resolve_generic_type_arg(GenericParam *gp, SemanticContext *ctx,
                         const ASTNode *site)
{
    if (gp == NULL)
        return TYPE_UNKNOWN;
    if (gp->constraint != NULL)
        return semantic_type_resolution_lookup_or_materialize(ctx,
                                                              gp->constraint);
    return resolve_named_type(gp->name, ctx, site);
}

/* Instrumentation counters for type-resolution audit. */
size_t g_resolve_type_node_calls = 0;
size_t g_resolve_type_node_unique_nodes = 0;
static void **g_resolve_type_node_seen = NULL;
static size_t g_resolve_type_node_seen_cap = 0;
size_t g_resolve_type_node_cache_hits = 0;
size_t g_resolve_type_node_cache_misses = 0;

static void
resolve_type_node_stats_record(ASTNode *node)
{
    const char *env = getenv("PGY_TYPE_RES_STATS");
    if (env == NULL || env[0] == '\0' || env[0] == '0')
        return;

    g_resolve_type_node_calls++;
    for (size_t i = 0; i < g_resolve_type_node_unique_nodes; i++) {
        if (g_resolve_type_node_seen[i] == node)
            return;
    }

    if (g_resolve_type_node_unique_nodes == g_resolve_type_node_seen_cap) {
        size_t new_cap = g_resolve_type_node_seen_cap == 0
            ? 64
            : g_resolve_type_node_seen_cap * 2;
        void **grown = realloc(g_resolve_type_node_seen,
                               new_cap * sizeof(void *));
        if (grown == NULL)
            return;
        g_resolve_type_node_seen = grown;
        g_resolve_type_node_seen_cap = new_cap;
    }
    g_resolve_type_node_seen[g_resolve_type_node_unique_nodes++] = node;
}

static Type *resolve_type_node_uncached(ASTNode *node, SemanticContext *ctx);

Type *
resolve_type_node(ASTNode *node, SemanticContext *ctx)
{
    Type *metadata_type;
    static int cache_disabled = -1;

    resolve_type_node_stats_record(node);
    if (node == NULL)
        return TYPE_VOID;

    metadata_type = semantic_type_resolution_lookup_resolved_type(ctx, node);
    if (metadata_type != NULL)
        return metadata_type;

    if (cache_disabled < 0) {
        const char *env = getenv("PGY_DISABLE_TYPE_CACHE");
        cache_disabled = (env != NULL && env[0] != '\0' && env[0] != '0') ? 1 : 0;
    }

    if (!cache_disabled && ctx != NULL) {
        for (size_t i = 0; i < ctx->resolve_type_cache.count; i++) {
            if (ctx->resolve_type_cache.keys[i] == (void *)node) {
                g_resolve_type_node_cache_hits++;
                return (Type *)ctx->resolve_type_cache.values[i];
            }
        }
    }

    g_resolve_type_node_cache_misses++;
    Type *result = resolve_type_node_uncached(node, ctx);

    bool is_cacheable_primitive =
        result == TYPE_INT || result == TYPE_LONG
        || result == TYPE_FLOAT || result == TYPE_DOUBLE
        || result == TYPE_BOOL || result == TYPE_STRING
        || result == TYPE_VOID;
    if (!cache_disabled && ctx != NULL && is_cacheable_primitive) {
        if (ctx->resolve_type_cache.count == ctx->resolve_type_cache.capacity) {
            size_t new_cap = ctx->resolve_type_cache.capacity == 0
                ? 128
                : ctx->resolve_type_cache.capacity * 2;
            void **new_keys = malloc(new_cap * sizeof(void *));
            void **new_values = malloc(new_cap * sizeof(void *));
            if (new_keys == NULL || new_values == NULL) {
                free(new_keys);
                free(new_values);
                return result;
            }
            if (ctx->resolve_type_cache.count > 0) {
                memcpy(new_keys, ctx->resolve_type_cache.keys,
                       ctx->resolve_type_cache.count * sizeof(void *));
                memcpy(new_values, ctx->resolve_type_cache.values,
                       ctx->resolve_type_cache.count * sizeof(void *));
            }
            free(ctx->resolve_type_cache.keys);
            free(ctx->resolve_type_cache.values);
            ctx->resolve_type_cache.keys = new_keys;
            ctx->resolve_type_cache.values = new_values;
            ctx->resolve_type_cache.capacity = new_cap;
        }
        ctx->resolve_type_cache.keys[ctx->resolve_type_cache.count] = (void *)node;
        ctx->resolve_type_cache.values[ctx->resolve_type_cache.count] = result;
        ctx->resolve_type_cache.count++;
    }

    return result;
}

static Type *
resolve_constructed_builtin_type(ASTNode *node, SemanticContext *ctx,
                                 const char *name)
{
    size_t expected_min = strcmp(name, "Result") == 0 ? 1 : 1;
    size_t expected_max = strcmp(name, "Result") == 0 ? 2 : 1;
    size_t provided = node->data.type.generic_args != NULL
        ? node->data.type.generic_args->count
        : 0;

    if (node->data.type.generic_args == NULL
        || provided < expected_min
        || provided > expected_max) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_GENERIC,
            PGY_CAUSE_GENERIC_ARGS_INVALID, PGY_FIX_ALIGN_GENERIC_ARG_LIST,
            node,
            strcmp(name, "Result") == 0
                ? "Result requires one or two type arguments"
                : "%s requires exactly one type argument",
            name);
        return TYPE_UNKNOWN;
    }

    Type *inner = resolve_generic_type_arg(
        node->data.type.generic_args->params[0], ctx, node);
    Type *constructor = TYPE_UNKNOWN;
    if (strcmp(name, "Array") == 0) constructor = TYPE_ARRAY;
    else if (strcmp(name, "Slice") == 0) constructor = TYPE_SLICE;
    else if (strcmp(name, "List") == 0) constructor = TYPE_LIST;
    else if (strcmp(name, "Queue") == 0) constructor = TYPE_QUEUE;
    else if (strcmp(name, "Box") == 0) constructor = TYPE_BOX;
    else if (strcmp(name, "Rc") == 0) constructor = TYPE_RC;
    else if (strcmp(name, "Weak") == 0) constructor = TYPE_WEAK;
    else if (strcmp(name, "Channel") == 0) constructor = TYPE_CHANNEL;
    else if (strcmp(name, "Future") == 0) constructor = TYPE_FUTURE;
    else if (strcmp(name, "RemoteFuture") == 0) constructor = TYPE_REMOTE_FUTURE;
    else if (strcmp(name, "Token") == 0) constructor = TYPE_TOKEN;
    else if (strcmp(name, "DeviceSlot") == 0) constructor = TYPE_DEVICE_SLOT;
    else if (strcmp(name, "Result") == 0) constructor = TYPE_RESULT;
    else if (strcmp(name, "Option") == 0) constructor = TYPE_OPTION;

    if (strcmp(name, "Result") == 0 && provided == 2) {
        Type *err = resolve_generic_type_arg(
            node->data.type.generic_args->params[1], ctx, node);
        Type *args[2] = { inner, err };
        return type_create_constructed(constructor, args, 2);
    }

    Type *args[1] = { inner };
    return type_create_constructed(constructor, args, 1);
}

static Type *
resolve_slot_family_type(ASTNode *node, SemanticContext *ctx, const char *name)
{
    if (node->data.type.generic_args == NULL
        || node->data.type.generic_args->count != 1) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_CLASS_CONTRACT_INVALID,
            PGY_CAUSE_CLASS_CONTRACT,
            PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN, node,
            "%s requires exactly one type argument", name);
        return TYPE_UNKNOWN;
    }

    Type *inner = resolve_generic_type_arg(
        node->data.type.generic_args->params[0], ctx, node);
    if (strcmp(name, "SecureSlot") == 0)
        return type_create_slot(inner, true);
    if (strcmp(name, "ReadView") == 0)
        return type_create_read_view(inner);
    if (strcmp(name, "WriteView") == 0)
        return type_create_write_view(inner);
    if (strcmp(name, "MoveToken") == 0)
        return type_create_slot_access(inner, false, SLOT_ACCESS_MOVE_TOKEN);
    return type_create_slot(inner, false);
}

static Type *
resolve_hashmap_type(ASTNode *node, SemanticContext *ctx)
{
    if (node->data.type.generic_args == NULL
        || node->data.type.generic_args->count != 2) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_GENERIC,
            PGY_CAUSE_GENERIC_ARGS_INVALID, PGY_FIX_ALIGN_GENERIC_ARG_LIST,
            node, "HashMap requires exactly two type arguments");
        return TYPE_UNKNOWN;
    }

    Type *key = resolve_generic_type_arg(
        node->data.type.generic_args->params[0], ctx, node);
    Type *value = resolve_generic_type_arg(
        node->data.type.generic_args->params[1], ctx, node);
    if (!type_equals(key, TYPE_STRING)
        && !type_equals(key, TYPE_INT)
        && !type_equals(key, TYPE_LONG)
        && !type_equals(key, TYPE_BOOL)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
            PGY_FIX_MATCH_BUILTIN_SIGNATURE, node,
            "HashMap currently supports only String, Int, Long, or Bool keys");
        return TYPE_UNKNOWN;
    }

    Type *args[2] = { key, value };
    return type_create_constructed(TYPE_HASHMAP, args, 2);
}

static Type *
resolve_user_generic_class_type(ASTNode *node, SemanticContext *ctx,
                                const char *name)
{
    Symbol *sym = scope_lookup(ctx->scope, name);
    if (sym == NULL || sym->kind != SYMBOL_CLASS || sym->type == NULL
        || ctx == NULL || ctx->program_root == NULL) {
        return NULL;
    }

    ASTNode *class_decl = find_type_decl_by_name(ctx->program_root, name);
    if (class_decl == NULL
        || class_decl->type != AST_CLASS_DECL
        || class_decl->data.class_decl.generic_params == NULL
        || class_decl->data.class_decl.generic_params->count == 0) {
        return NULL;
    }

    size_t argc = 0;
    ASTNode **effective_args = collect_effective_generic_arg_nodes(
        class_decl->data.class_decl.generic_params,
        node->data.type.generic_args,
        node, ctx, "class", name, &argc);
    if (effective_args == NULL)
        return TYPE_UNKNOWN;

    Type **args = calloc(argc > 0 ? argc : 1, sizeof(Type *));
    if (args == NULL) {
        free(effective_args);
        return TYPE_UNKNOWN;
    }

    for (size_t i = 0; i < argc; i++)
        args[i] = resolve_type_node(effective_args[i], ctx);
    free(effective_args);

    Type *result = type_create_constructed(sym->type, args, argc);
    validate_class_where_clause_specialization_ast(class_decl, node, node, ctx);
    free(args);
    return result;
}

static Type *
resolve_type_node_uncached(ASTNode *node, SemanticContext *ctx)
{
    if (node == NULL)
        return TYPE_VOID;

    if (node->type == AST_CHANNEL_TYPE) {
        Type *inner = resolve_type_node(node->data.channel_type.element_type, ctx);
        Type *args[1] = { inner };
        return type_create_constructed(TYPE_CHANNEL, args, 1);
    }

    if (node->type == AST_FUTURE_TYPE) {
        Type *inner = resolve_type_node(node->data.future_type.value_type, ctx);
        Type *args[1] = { inner };
        return type_create_constructed(TYPE_FUTURE, args, 1);
    }

    if (node->type == AST_EVENT_HANDLER_TYPE) {
        size_t param_count = node->data.event_handler_type.param_count;
        Type **param_types = calloc(param_count > 0 ? param_count : 1,
                                    sizeof(Type *));
        Type *return_type = TYPE_VOID;
        Type *result;

        if (param_types == NULL)
            return TYPE_UNKNOWN;

        for (size_t i = 0; i < param_count; i++) {
            param_types[i] = resolve_type_node(
                node->data.event_handler_type.param_types[i], ctx);
        }
        if (node->data.event_handler_type.return_type != NULL) {
            return_type = resolve_type_node(
                node->data.event_handler_type.return_type, ctx);
        }

        result = type_create_function(param_types, param_count, return_type);
        free(param_types);
        return result != NULL ? result : TYPE_UNKNOWN;
    }

    if (node->type != AST_TYPE)
        return TYPE_UNKNOWN;

    if (node->data.type.tuple_elements != NULL
        && node->data.type.tuple_element_count > 0) {
        size_t n = node->data.type.tuple_element_count;
        Type **elems = calloc(n, sizeof(Type *));
        if (elems == NULL)
            return TYPE_UNKNOWN;
        for (size_t i = 0; i < n; i++)
            elems[i] = resolve_type_node(node->data.type.tuple_elements[i], ctx);
        Type *result = type_create_tuple(elems, n);
        free(elems);
        return result != NULL ? result : TYPE_UNKNOWN;
    }

    const char *name = node->data.type.name;
    if (strcmp(name, "Slot") == 0 || strcmp(name, "SecureSlot") == 0
        || strcmp(name, "ReadView") == 0 || strcmp(name, "WriteView") == 0
        || strcmp(name, "MoveToken") == 0) {
        return resolve_slot_family_type(node, ctx, name);
    }

    if (strcmp(name, "HashMap") == 0)
        return resolve_hashmap_type(node, ctx);

    if (strcmp(name, "Set") == 0
        && node->data.type.generic_args != NULL
        && node->data.type.generic_args->count == 1) {
        Type *inner = resolve_generic_type_arg(
            node->data.type.generic_args->params[0], ctx, node);
        Type *args[1] = { inner };
        return type_create_constructed(TYPE_SET, args, 1);
    }

    Symbol *builtin_shadow = scope_lookup(ctx->scope, name);
    bool allow_builtin_constructed =
        builtin_shadow == NULL || builtin_shadow->kind != SYMBOL_CLASS;

    if (allow_builtin_constructed
        && (strcmp(name, "Array") == 0 || strcmp(name, "Slice") == 0
            || strcmp(name, "List") == 0 || strcmp(name, "Queue") == 0
            || strcmp(name, "Box") == 0 || strcmp(name, "Rc") == 0
            || strcmp(name, "Weak") == 0 || strcmp(name, "Channel") == 0
            || strcmp(name, "Future") == 0 || strcmp(name, "RemoteFuture") == 0
            || strcmp(name, "Token") == 0
            || strcmp(name, "DeviceSlot") == 0 || strcmp(name, "Result") == 0
            || strcmp(name, "Option") == 0)) {
        return resolve_constructed_builtin_type(node, ctx, name);
    }

    Type *generic_class = resolve_user_generic_class_type(node, ctx, name);
    if (generic_class != NULL)
        return generic_class;

    return resolve_named_type(name, ctx, node);
}

bool
require_assignable(Type *from, Type *to, const ASTNode *site,
                   SemanticContext *ctx)
{
    if (type_is_assignable(from, to))
        return true;

    if (to->kind == TYPE_KIND_SLOT && to->data.slot.inner_type != NULL
        && type_is_assignable(from, to->data.slot.inner_type)) {
        return true;
    }

    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_TYPE_MISMATCH,
        PGY_CAUSE_ASSIGNABILITY_CHECK,
        PGY_FIX_ANNOTATE_OR_CONVERT,
        site,
        "Type mismatch: cannot assign '%s' to '%s'",
        from->name, to->name);
    return false;
}

Type *
wrap_constructed(Type *constructor, Type *inner)
{
    Type *args[1] = { inner };
    return type_create_constructed(constructor, args, 1);
}
