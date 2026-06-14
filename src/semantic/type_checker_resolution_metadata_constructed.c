#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "diag_codes.h"
#include "type_checker_internal.h"
#include "type_checker_collection_policy.h"
#include "type_checker_resolution_metadata_internal.h"

typedef enum StableSlotShellKind {
    STABLE_SLOT_SHELL_SLOT,
    STABLE_SLOT_SHELL_SECURE_SLOT,
    STABLE_SLOT_SHELL_READ_VIEW,
    STABLE_SLOT_SHELL_WRITE_VIEW,
    STABLE_SLOT_SHELL_MOVE_TOKEN,
} StableSlotShellKind;

typedef struct StableSlotShellSpec {
    const char *name;
    StableSlotShellKind kind;
} StableSlotShellSpec;

static int
stable_slot_shell_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const StableSlotShellSpec *spec = (const StableSlotShellSpec *)entry;

    return strcmp(name, spec->name);
}

static const StableSlotShellSpec *
stable_slot_shell_spec(const char *name)
{
    static const StableSlotShellSpec specs[] = {
        { "MoveToken", STABLE_SLOT_SHELL_MOVE_TOKEN },
        { "ReadView", STABLE_SLOT_SHELL_READ_VIEW },
        { "SecureSlot", STABLE_SLOT_SHELL_SECURE_SLOT },
        { "Slot", STABLE_SLOT_SHELL_SLOT },
        { "WriteView", STABLE_SLOT_SHELL_WRITE_VIEW },
    };
    const size_t count = sizeof(specs) / sizeof(specs[0]);
    const StableSlotShellSpec *match;

    if (name == NULL)
        return NULL;
    match = (const StableSlotShellSpec *)bsearch(
        &name, specs, count, sizeof(specs[0]), stable_slot_shell_compare);
    return match;
}

static Type *
stable_slot_shell_create(const StableSlotShellSpec *spec, Type *inner)
{
    if (spec == NULL || inner == NULL)
        return NULL;

    switch (spec->kind) {
    case STABLE_SLOT_SHELL_SECURE_SLOT:
        return type_create_slot(inner, true);
    case STABLE_SLOT_SHELL_READ_VIEW:
        return type_create_read_view(inner);
    case STABLE_SLOT_SHELL_WRITE_VIEW:
        return type_create_write_view(inner);
    case STABLE_SLOT_SHELL_MOVE_TOKEN:
        return type_create_slot_access(inner, false, SLOT_ACCESS_MOVE_TOKEN);
    case STABLE_SLOT_SHELL_SLOT:
    default:
        return type_create_slot(inner, false);
    }
}

static bool
stable_constructed_type_node_is_builtin_constructed(const ASTNode *type_node)
{
    const char *name;
    size_t argc;

    if (type_node == NULL)
        return false;
    if (type_node->type == AST_CHANNEL_TYPE
        || type_node->type == AST_FUTURE_TYPE
        || type_node->type == AST_EVENT_HANDLER_TYPE) {
        return true;
    }
    if (type_node->type != AST_TYPE)
        return false;
    if (ast_type_tuple_element_count(type_node) > 0) {
        return true;
    }
    name = ast_type_name(type_node);
    if (name == NULL || ast_type_generic_args(type_node) == NULL)
        return false;
    argc = ast_generic_param_count(ast_type_generic_args(type_node));
    if (semantic_type_resolution_metadata_stable_constructed_shell(name, argc)
        != NULL) {
        return true;
    }
    return argc == 1
        && semantic_type_resolution_metadata_stable_slot_like_shell(name);
}

static Type *
stable_constructed_resolve_arg(SemanticContext *ctx,
                               GenericParam *gp)
{
    Type *resolved;

    if (ctx == NULL || gp == NULL)
        return NULL;
    ASTNode *constraint = ast_generic_param_constraint(gp);
    if (constraint != NULL) {
        resolved = semantic_type_resolution_lookup_metadata_type_ref(ctx,
                                                                     constraint);
        if (resolved != NULL)
            return resolved;
        if (stable_constructed_type_node_is_builtin_constructed(constraint)) {
            semantic_type_resolution_try_record_stable_constructed_type(
                ctx, constraint);
            return semantic_type_resolution_lookup_metadata_type_ref(
                ctx, constraint);
        }
        return NULL;
    }
    resolved = semantic_type_resolution_lookup_metadata_name_or_alias(ctx,
                                                                      ast_generic_param_name(gp));
    return resolved != TYPE_UNKNOWN ? resolved : NULL;
}

static void
try_record_generic_class_constructed_type(SemanticContext *ctx,
                                          ASTNode *type_node)
{
    const char *name;
    Symbol *sym;
    ASTNode *class_decl;
    GenericParams *decl_params;
    ASTNode **effective_args;
    Type **resolved_args;
    Type *result;
    size_t argc = 0;
    size_t provided_count = 0;

    if (ctx == NULL || type_node == NULL || type_node->type != AST_TYPE)
        return;

    name = ast_type_name(type_node);
    if (name == NULL)
        return;
    sym = scope_lookup(ctx->scope, name);
    if (sym == NULL || sym->kind != SYMBOL_CLASS || sym->type == NULL
        || sym->type == TYPE_UNKNOWN)
        return;

    class_decl = semantic_find_class_decl_by_name(ctx, name);
    if (class_decl == NULL || class_decl->type != AST_CLASS_DECL)
        return;
    decl_params = ast_class_generic_params(class_decl);
    if (ast_generic_param_count(decl_params) == 0)
        return;

    provided_count = ast_type_generic_args(type_node) != NULL
        ? ast_generic_param_count(ast_type_generic_args(type_node))
        : 0;
    if (provided_count == 0) {
        for (size_t i = 0; i < ast_generic_param_count(decl_params); i++) {
            GenericParam *param = ast_generic_param_at(decl_params, i);
            if (param == NULL || ast_generic_param_default_type(param) == NULL)
                return;
        }
    }

    effective_args = collect_effective_generic_arg_nodes(
        decl_params,
        ast_type_generic_args(type_node),
        type_node,
        ctx,
        "class",
        name,
        &argc);
    if (effective_args == NULL)
        return;

    if ((argc > 0 ? argc : 1) > SIZE_MAX / sizeof(Type *)) {
        free(effective_args);
        return;
    }
    resolved_args = calloc(argc > 0 ? argc : 1, sizeof(Type *));
    if (resolved_args == NULL) {
        free(effective_args);
        return;
    }

    for (size_t i = 0; i < argc; i++) {
        resolved_args[i] = semantic_type_resolution_lookup_metadata_type_ref(
            ctx, effective_args[i]);
        if (resolved_args[i] == NULL) {
            free(resolved_args);
            free(effective_args);
            return;
        }
        /* Void cannot be a generic type argument: a field, slot, or return of
         * type Void has no value representation. Fail closed with a diagnostic
         * rather than recording a degenerate constructed type. */
        if (type_equals(resolved_args[i], TYPE_VOID)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE,
                effective_args[i] != NULL ? effective_args[i] : type_node,
                "Generic type argument %llu of '%s' resolved to Void; "
                "Void is not a valid generic type argument",
                (unsigned long long)(i + 1), name);
            free(resolved_args);
            free(effective_args);
            return;
        }
    }

    result = type_create_constructed(sym->type, resolved_args, argc);
    if (result != NULL) {
        semantic_type_resolution_record_owned_resolved_type(ctx,
                                                            type_node,
                                                            result);
        validate_class_where_clause_specialization_ast(class_decl,
                                                       type_node,
                                                       type_node,
                                                       ctx);
    }
    free(resolved_args);
    free(effective_args);
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
            ? ast_channel_type_element_type(type_node)
            : ast_future_type_value_type(type_node);
        Type *inner = semantic_type_resolution_lookup_metadata_type_ref(
            ctx, inner_node);
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
        size_t param_count = ast_event_handler_param_count(type_node);
        Type **param_types = NULL;
        Type *return_type = TYPE_VOID;
        Type *shell;

        if (param_count > SIZE_MAX / sizeof(Type *))
            return;
        param_types = param_count > 0
            ? calloc(param_count, sizeof(Type *))
            : NULL;
        if (param_count > 0 && param_types == NULL)
            return;
        for (size_t i = 0; i < param_count; i++) {
            param_types[i] = semantic_type_resolution_lookup_metadata_type_ref(
                ctx, ast_event_handler_param_type(type_node, i));
            if (param_types[i] == NULL) {
                free(param_types);
                return;
            }
        }
        ASTNode *return_type_node = ast_event_handler_return_type(type_node);
        if (return_type_node != NULL) {
            return_type = semantic_type_resolution_lookup_metadata_type_ref(
                ctx, return_type_node);
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

    args_node = ast_type_generic_args(type_node);
    if ((args_node == NULL || ast_generic_param_count(args_node) == 0)
        && ast_type_name(type_node) != NULL) {
        try_record_generic_class_constructed_type(ctx, type_node);
        if (semantic_type_resolution_lookup_resolved_type(ctx, type_node) != NULL)
            return;
    }

    if (ast_type_tuple_element_count(type_node) > 0) {
        size_t element_count = ast_type_tuple_element_count(type_node);
        Type **elements;
        Type *shell;

        if (element_count > SIZE_MAX / sizeof(Type *))
            return;
        elements = calloc(element_count, sizeof(Type *));
        if (elements == NULL)
            return;
        for (size_t i = 0; i < element_count; i++) {
            elements[i] = semantic_type_resolution_lookup_metadata_type_ref(
                ctx, ast_type_tuple_element(type_node, i));
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

    if (semantic_type_resolution_metadata_stable_slot_like_shell(
            ast_type_name(type_node))) {
        Type *inner;
        const StableSlotShellSpec *slot_spec =
            stable_slot_shell_spec(ast_type_name(type_node));
        Type *slot_type;

        args_node = ast_type_generic_args(type_node);
        if (slot_spec == NULL)
            return;
        if (args_node == NULL || ast_generic_param_count(args_node) != 1)
            return;
        if (ast_generic_param_at(args_node, 0) == NULL)
            return;
        inner = stable_constructed_resolve_arg(ctx,
                                               ast_generic_param_at(args_node, 0));
        if (inner == NULL)
            return;

        slot_type = stable_slot_shell_create(slot_spec, inner);
        if (slot_type != NULL)
            semantic_type_resolution_record_owned_resolved_type(ctx, type_node, slot_type);
        return;
    }

    if (args_node == NULL
        || ast_generic_param_count(args_node) == 0
        || ast_generic_param_count(args_node) > 2)
        return;

    constructor = semantic_type_resolution_metadata_stable_constructed_shell(
        ast_type_name(type_node),
        ast_generic_param_count(args_node));
    if (constructor == NULL) {
        try_record_generic_class_constructed_type(ctx, type_node);
        return;
    }

    for (size_t i = 0; i < ast_generic_param_count(args_node); i++) {
        args[i] = stable_constructed_resolve_arg(
            ctx, ast_generic_param_at(args_node, i));
        if (args[i] == NULL)
            return;
    }

    if (semantic_type_resolution_reject_unsupported_stable_constructed_args(
            ctx,
            type_node,
            ast_type_name(type_node),
            args,
            ast_generic_param_count(args_node))) {
        return;
    }

    if (constructor == TYPE_HASHMAP
        && !type_checker_hashmap_key_supported(args[0])) {
        return;
    }

    result = type_create_constructed(constructor,
                                     args,
                                     ast_generic_param_count(args_node));
    if (result == NULL)
        return;
    semantic_type_resolution_record_owned_resolved_type(ctx, type_node, result);
}
