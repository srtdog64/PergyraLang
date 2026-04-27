#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "diag_codes.h"
#include "type_checker_internal.h"

static void
semantic_type_resolution_free_owned_type(Type *type)
{
    if (type == NULL)
        return;
    free(type->name);
    if (type->kind == TYPE_KIND_CONSTRUCTED)
        free(type->data.constructed.args);
    if (type->kind == TYPE_KIND_FUNCTION)
        free(type->data.function.param_types);
    if (type->kind == TYPE_KIND_TUPLE)
        free(type->data.tuple.elements);
    free(type);
}

static Type *
metadata_builtin_singleton(const char *name);

static bool
metadata_type_ref_has_no_generic_args(const ASTNode *type_node)
{
    return type_node != NULL
        && type_node->type == AST_TYPE
        && (type_node->data.type.generic_args == NULL
            || type_node->data.type.generic_args->count == 0);
}

static bool
metadata_alias_stack_contains(const char **stack, size_t stack_count, const char *name)
{
    if (stack == NULL || name == NULL)
        return false;
    for (size_t i = 0; i < stack_count; i++) {
        if (stack[i] != NULL && strcmp(stack[i], name) == 0)
            return true;
    }
    return false;
}

static void
metadata_format_alias_cycle(const char **stack,
                            size_t stack_count,
                            const char *closing_name,
                            char *out,
                            size_t out_size)
{
    size_t start = 0;
    size_t used = 0;
    bool emitted = false;

    if (out == NULL || out_size == 0)
        return;
    out[0] = '\0';
    if (closing_name == NULL)
        closing_name = "<alias>";

    for (size_t i = 0; i < stack_count; i++) {
        if (stack[i] != NULL && strcmp(stack[i], closing_name) == 0) {
            start = i;
            break;
        }
    }

    for (size_t i = start; i < stack_count; i++) {
        int written;
        const char *label = stack[i] != NULL ? stack[i] : "<alias>";
        written = snprintf(out + used,
                           out_size - used,
                           "%s%s",
                           emitted ? " -> " : "",
                           label);
        if (written < 0)
            return;
        if ((size_t)written >= out_size - used) {
            out[out_size - 1] = '\0';
            return;
        }
        used += (size_t)written;
        emitted = true;
    }

    if (used < out_size) {
        int written = snprintf(out + used,
                               out_size - used,
                               "%s%s",
                               emitted ? " -> " : "",
                               closing_name);
        if (written < 0)
            return;
        if ((size_t)written >= out_size - used)
            out[out_size - 1] = '\0';
    }
}

static Type *
metadata_type_ref_with_alias_stack(SemanticContext *ctx,
                                   ASTNode *type_node,
                                   const char **alias_stack,
                                   size_t alias_stack_count);

static Type *
metadata_type_from_name_with_alias_stack(SemanticContext *ctx,
                                         const char *name,
                                         const char **alias_stack,
                                         size_t alias_stack_count);

static Type *
metadata_alias_decl_target_type(SemanticContext *ctx,
                                ASTNode *alias_decl,
                                const char **alias_stack,
                                size_t alias_stack_count)
{
    const char *alias_name;
    const char *next_stack[33];

    if (ctx == NULL || alias_decl == NULL || alias_decl->type != AST_TYPE_ALIAS)
        return NULL;
    if (alias_decl->data.type_alias.target_type == NULL)
        return NULL;

    alias_name = alias_decl->data.type_alias.name;
    if (alias_name == NULL)
        return NULL;
    if (alias_stack_count >= 32) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_DEPENDENCY_CYCLE,
            PGY_CAUSE_TYPE_RESOLUTION_CYCLE,
            PGY_FIX_BREAK_CYCLE_VIA_INDIRECTION,
            alias_decl,
            "Type alias resolution exceeded maximum depth near '%s'.\n"
            "Contract source:\n"
            "- alias chain provenance: %s\n"
            "Reason:\n"
            "- alias metadata materialization exceeded the beta maximum alias depth\n"
            "- this usually means a dependency cycle escaped the graph prepass\n"
            "Fix:\n"
            "- break the alias chain with a concrete type declaration\n"
            "- or split the recursive shape behind an explicit indirection",
            alias_name,
            alias_name);
        return TYPE_UNKNOWN;
    }
    if (metadata_alias_stack_contains(alias_stack, alias_stack_count, alias_name)) {
        char cycle_text[512];
        metadata_format_alias_cycle(alias_stack,
                                    alias_stack_count,
                                    alias_name,
                                    cycle_text,
                                    sizeof(cycle_text));
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_DEPENDENCY_CYCLE,
            PGY_CAUSE_TYPE_RESOLUTION_CYCLE,
            PGY_FIX_BREAK_CYCLE_VIA_INDIRECTION,
            alias_decl,
            "Type resolution dependency cycle detected around alias '%s'.\n"
            "Contract source:\n"
            "- alias chain provenance: %s\n"
            "Reason:\n"
            "- alias metadata materialization re-entered '%s' before the current alias chain completed\n"
            "- cycle path: %s\n"
            "Fix:\n"
            "- break the alias loop so one alias resolves to a concrete type first\n"
            "- or replace one side of the cycle with a non-alias type declaration",
            alias_name,
            cycle_text[0] != '\0' ? cycle_text : alias_name,
            alias_name,
            cycle_text[0] != '\0' ? cycle_text : alias_name);
        return TYPE_UNKNOWN;
    }

    for (size_t i = 0; i < alias_stack_count; i++)
        next_stack[i] = alias_stack[i];
    next_stack[alias_stack_count] = alias_name;

    return metadata_type_ref_with_alias_stack(ctx,
                                             alias_decl->data.type_alias.target_type,
                                             next_stack,
                                             alias_stack_count + 1);
}

static Type *
metadata_type_from_name_with_alias_stack(SemanticContext *ctx,
                                         const char *name,
                                         const char **alias_stack,
                                         size_t alias_stack_count)
{
    Type *resolved;
    ASTNode *alias_decl;
    Symbol *sym;

    if (ctx == NULL || name == NULL)
        return NULL;

    resolved = metadata_builtin_singleton(name);
    if (resolved != NULL)
        return resolved;

    alias_decl = ctx->program_root != NULL
        ? find_type_alias_decl(ctx->program_root, name)
        : NULL;
    if (alias_decl != NULL)
        return metadata_alias_decl_target_type(ctx,
                                               alias_decl,
                                               alias_stack,
                                               alias_stack_count);

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
    }
    return sym->type;
}

static Type *
metadata_record_alias_type_ref(SemanticContext *ctx,
                               ASTNode *type_node,
                               ASTNode *alias_decl,
                               Type *resolved)
{
    const char *name;
    Symbol *sym;

    if (ctx == NULL || type_node == NULL || alias_decl == NULL
        || resolved == NULL || resolved == TYPE_UNKNOWN) {
        return NULL;
    }
    if (type_node->type != AST_TYPE || type_node->data.type.name == NULL)
        return NULL;

    name = type_node->data.type.name;
    semantic_type_resolution_record_named_dependency(
        ctx, type_node, name, TYPE_RES_NODE_ALIAS, alias_decl, name,
        "metadata type-alias lookup");
    semantic_type_resolution_record_resolved_type(ctx, type_node, resolved);

    sym = scope_lookup(ctx->scope, name);
    if (sym != NULL && (sym->type == NULL || sym->type == TYPE_UNKNOWN))
        sym->type = resolved;

    return resolved;
}

static Type *
metadata_type_ref_with_alias_stack(SemanticContext *ctx,
                                   ASTNode *type_node,
                                   const char **alias_stack,
                                   size_t alias_stack_count)
{
    Type *resolved;

    if (ctx == NULL || type_node == NULL)
        return NULL;

    resolved = semantic_type_resolution_lookup_resolved_type(ctx, type_node);
    if (resolved != NULL)
        return resolved;

    if (type_node->type == AST_TYPE) {
        const char *name = type_node->data.type.name;
        if (name != NULL && metadata_type_ref_has_no_generic_args(type_node)) {
            ASTNode *alias_decl;

            resolved = metadata_builtin_singleton(name);
            if (resolved != NULL) {
                semantic_type_resolution_record_resolved_type(ctx,
                                                              type_node,
                                                              resolved);
                return resolved;
            }

            alias_decl = ctx->program_root != NULL
                ? find_type_alias_decl(ctx->program_root, name)
                : NULL;
            if (alias_decl != NULL) {
                resolved = metadata_alias_decl_target_type(ctx,
                                                           alias_decl,
                                                           alias_stack,
                                                           alias_stack_count);
                if (resolved == TYPE_UNKNOWN)
                    return TYPE_UNKNOWN;
                return metadata_record_alias_type_ref(ctx,
                                                      type_node,
                                                      alias_decl,
                                                      resolved);
            }
        }
    }

    semantic_type_resolution_try_record_stable_constructed_type(ctx, type_node);
    return semantic_type_resolution_lookup_resolved_type(ctx, type_node);
}

static Type *
metadata_alias_type(SemanticContext *ctx, ASTNode *type_node)
{
    const char *name;
    ASTNode *alias_decl;
    Type *resolved = NULL;
    const char *alias_stack[33];

    if (ctx == NULL || type_node == NULL || type_node->type != AST_TYPE)
        return NULL;
    if (type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0)
        return NULL;

    name = type_node->data.type.name;
    if (name == NULL || ctx->program_root == NULL)
        return NULL;

    alias_decl = find_type_alias_decl(ctx->program_root, name);
    if (alias_decl == NULL || alias_decl->data.type_alias.target_type == NULL)
        return NULL;

    resolved = metadata_alias_decl_target_type(ctx, alias_decl, alias_stack, 0);
    if (resolved == TYPE_UNKNOWN)
        return TYPE_UNKNOWN;
    if (resolved == NULL)
        return NULL;

    return metadata_record_alias_type_ref(ctx, type_node, alias_decl, resolved);
}

static bool
metadata_name_is_bare_generic_builtin_shell(const char *name)
{
    if (name == NULL)
        return false;
    return strcmp(name, "Array") == 0
        || strcmp(name, "Slice") == 0
        || strcmp(name, "List") == 0
        || strcmp(name, "Queue") == 0
        || strcmp(name, "HashMap") == 0
        || strcmp(name, "Set") == 0
        || strcmp(name, "Box") == 0
        || strcmp(name, "Rc") == 0
        || strcmp(name, "Weak") == 0
        || strcmp(name, "RemoteFuture") == 0
        || strcmp(name, "DeviceSlot") == 0
        || strcmp(name, "Option") == 0;
}

static void
metadata_record_named_materializer_fallback(SemanticContext *ctx,
                                            ASTNode *type_node)
{
    const char *name;
    Symbol *sym;

    if (ctx == NULL || type_node == NULL || type_node->type != AST_TYPE)
        return;

    name = type_node->data.type.name;
    if (metadata_name_is_bare_generic_builtin_shell(name)) {
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

static void
metadata_record_materializer_fallback(SemanticContext *ctx, ASTNode *type_node)
{
    if (ctx == NULL)
        return;

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
        Type *alias_type = metadata_alias_type(ctx, type_node);
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
        resolved = metadata_builtin_singleton(type_node->data.type.name);
    if (resolved != NULL) {
        semantic_type_resolution_record_resolved_type(ctx, type_node, resolved);
        return resolved;
    }

    resolved = metadata_scope_named_type(ctx, type_node);
    if (resolved != NULL)
        return resolved;

    semantic_type_resolution_try_record_stable_constructed_type(ctx,
                                                                type_node);
    resolved = semantic_type_resolution_lookup_resolved_type(ctx, type_node);
    if (resolved != NULL)
        return resolved;

    metadata_record_materializer_fallback(ctx, type_node);
    return resolve_type_node(type_node, ctx);
}

static Type *
metadata_lookup_type_ref(SemanticContext *ctx, ASTNode *type_node)
{
    Type *resolved;

    if (type_node == NULL)
        return NULL;

    resolved = semantic_type_resolution_lookup_resolved_type(ctx, type_node);
    if (resolved != NULL)
        return resolved;

    if (type_node->type == AST_TYPE)
        resolved = metadata_builtin_singleton(type_node->data.type.name);
    if (resolved != NULL) {
        semantic_type_resolution_record_resolved_type(ctx, type_node, resolved);
        return resolved;
    }

    resolved = metadata_scope_named_type(ctx, type_node);
    if (resolved != NULL)
        return resolved;

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
    if (argc == 1 && strcmp(name, "Array") == 0)
        return TYPE_ARRAY;
    if (argc == 1 && strcmp(name, "Slice") == 0)
        return TYPE_SLICE;
    if (argc == 1 && strcmp(name, "List") == 0)
        return TYPE_LIST;
    if (argc == 1 && strcmp(name, "Queue") == 0)
        return TYPE_QUEUE;
    if (argc == 1 && strcmp(name, "Set") == 0)
        return TYPE_SET;
    if (argc == 1 && strcmp(name, "Box") == 0)
        return TYPE_BOX;
    if (argc == 1 && strcmp(name, "Rc") == 0)
        return TYPE_RC;
    if (argc == 1 && strcmp(name, "Weak") == 0)
        return TYPE_WEAK;
    if (argc == 1 && strcmp(name, "Channel") == 0)
        return TYPE_CHANNEL;
    if (argc == 1 && strcmp(name, "Future") == 0)
        return TYPE_FUTURE;
    if (argc == 1 && strcmp(name, "RemoteFuture") == 0)
        return TYPE_REMOTE_FUTURE;
    if (argc == 1 && strcmp(name, "Token") == 0)
        return TYPE_TOKEN;
    if (argc == 1 && strcmp(name, "DeviceSlot") == 0)
        return TYPE_DEVICE_SLOT;
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
    if (strcmp(name, "Allocator") == 0)
        return TYPE_ALLOCATOR;
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

    if (ctx == NULL || type_node == NULL)
        return;

    if (type_node->type == AST_CHANNEL_TYPE || type_node->type == AST_FUTURE_TYPE) {
        ASTNode *inner_node = type_node->type == AST_CHANNEL_TYPE
            ? type_node->data.channel_type.element_type
            : type_node->data.future_type.value_type;
        Type *inner = metadata_lookup_type_ref(ctx, inner_node);
        Type *shell;
        if (inner == NULL)
            return;
        args[0] = inner;
        shell = type_create_constructed(
            type_node->type == AST_CHANNEL_TYPE ? TYPE_CHANNEL : TYPE_FUTURE,
            args,
            1);
        if (shell != NULL)
            semantic_type_resolution_record_owned_resolved_type(ctx, type_node, shell);
        return;
    }

    if (type_node->type == AST_EVENT_HANDLER_TYPE) {
        size_t param_count = type_node->data.event_handler_type.param_count;
        Type **param_types = param_count > 0
            ? calloc(param_count, sizeof(Type *))
            : NULL;
        Type *return_type = TYPE_VOID;
        Type *shell;

        if (param_count > 0 && param_types == NULL)
            return;
        for (size_t i = 0; i < param_count; i++) {
            param_types[i] = metadata_lookup_type_ref(
                ctx, type_node->data.event_handler_type.param_types[i]);
            if (param_types[i] == NULL) {
                free(param_types);
                return;
            }
        }
        if (type_node->data.event_handler_type.return_type != NULL) {
            return_type = metadata_lookup_type_ref(
                ctx, type_node->data.event_handler_type.return_type);
            if (return_type == NULL) {
                free(param_types);
                return;
            }
        }
        shell = type_create_function(param_types, param_count, return_type);
        free(param_types);
        if (shell != NULL)
            semantic_type_resolution_record_owned_resolved_type(ctx, type_node, shell);
        return;
    }

    if (type_node->type != AST_TYPE)
        return;

    if (type_node->data.type.tuple_elements != NULL
        && type_node->data.type.tuple_element_count > 0) {
        size_t element_count = type_node->data.type.tuple_element_count;
        Type **elements = calloc(element_count, sizeof(Type *));
        Type *shell;

        if (elements == NULL)
            return;
        for (size_t i = 0; i < element_count; i++) {
            elements[i] = metadata_lookup_type_ref(
                ctx, type_node->data.type.tuple_elements[i]);
            if (elements[i] == NULL) {
                free(elements);
                return;
            }
        }
        shell = type_create_tuple(elements, element_count);
        free(elements);
        if (shell != NULL)
            semantic_type_resolution_record_owned_resolved_type(ctx, type_node, shell);
        return;
    }

    if (type_node->data.type.name != NULL
        && (strcmp(type_node->data.type.name, "Slot") == 0
            || strcmp(type_node->data.type.name, "SecureSlot") == 0
            || strcmp(type_node->data.type.name, "ReadView") == 0
            || strcmp(type_node->data.type.name, "WriteView") == 0
            || strcmp(type_node->data.type.name, "MoveToken") == 0)) {
        Type *inner;
        Type *slot_type = NULL;

        args_node = type_node->data.type.generic_args;
        if (args_node == NULL || args_node->count != 1)
            return;
        if (args_node->params[0] == NULL)
            return;
        inner = metadata_lookup_type_ref(ctx, args_node->params[0]->constraint);
        if (inner == NULL) {
            const char *alias_stack[33];
            inner = metadata_type_from_name_with_alias_stack(
                ctx, args_node->params[0]->name, alias_stack, 0);
        }
        if (inner == NULL)
            return;

        if (strcmp(type_node->data.type.name, "SecureSlot") == 0)
            slot_type = type_create_slot(inner, true);
        else if (strcmp(type_node->data.type.name, "ReadView") == 0)
            slot_type = type_create_read_view(inner);
        else if (strcmp(type_node->data.type.name, "WriteView") == 0)
            slot_type = type_create_write_view(inner);
        else if (strcmp(type_node->data.type.name, "MoveToken") == 0)
            slot_type = type_create_slot_access(inner, false, SLOT_ACCESS_MOVE_TOKEN);
        else
            slot_type = type_create_slot(inner, false);

        if (slot_type != NULL)
            semantic_type_resolution_record_owned_resolved_type(ctx, type_node, slot_type);
        return;
    }

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
        args[i] = metadata_lookup_type_ref(ctx, arg_type);
        if (args[i] == NULL && gp != NULL) {
            const char *alias_stack[33];
            args[i] = metadata_type_from_name_with_alias_stack(
                ctx, gp->name, alias_stack, 0);
        }
        if (args[i] == NULL)
            return;
    }

    if (constructor == TYPE_HASHMAP
        && !type_equals(args[0], TYPE_STRING)
        && !type_equals(args[0], TYPE_INT)
        && !type_equals(args[0], TYPE_LONG)
        && !type_equals(args[0], TYPE_BOOL)) {
        return;
    }

    result = type_create_constructed(constructor, args, args_node->count);
    if (result == NULL)
        return;
    semantic_type_resolution_record_owned_resolved_type(ctx, type_node, result);
}
