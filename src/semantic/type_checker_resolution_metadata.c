#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "diag_codes.h"
#include "type_checker_internal.h"

Type *
semantic_type_resolution_metadata_builtin_singleton(const char *name);

static Type *
metadata_type_from_name_with_alias_stack(SemanticContext *ctx,
                                         const char *name,
                                         const char **alias_stack,
                                         size_t alias_stack_count);

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

    resolved = semantic_type_resolution_metadata_builtin_singleton(name);
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
        if (name != NULL
            && semantic_type_resolution_metadata_type_ref_has_no_generic_args(
                type_node)) {
            ASTNode *alias_decl;

            resolved = semantic_type_resolution_metadata_builtin_singleton(name);
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

    /* Strict beta keeps this as a single central escape hatch only. Current
     * DAG smoke requires this path to be dormant (materializer_fallbacks == 0);
     * resolver-inventory smoke rejects any owner-local fallback reintroduction.
     */
    semantic_type_resolution_record_materializer_fallback(ctx, type_node);
    return resolve_type_node(type_node, ctx);
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
semantic_type_resolution_lookup_metadata_name_or_alias(SemanticContext *ctx,
                                                       const char *name)
{
    const char *alias_stack[33];

    return metadata_type_from_name_with_alias_stack(ctx, name, alias_stack, 0);
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
