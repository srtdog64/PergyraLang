#include <string.h>

#include "diag_codes.h"
#include "type_checker_internal.h"

bool
semantic_type_resolution_metadata_type_ref_has_no_generic_args(
    const ASTNode *type_node)
{
    return type_node != NULL
        && type_node->type == AST_TYPE
        && (type_node->data.type.generic_args == NULL
            || type_node->data.type.generic_args->count == 0);
}

bool
semantic_type_resolution_metadata_stable_builtin_shell_arity(
    const char *name,
    size_t *out_min,
    size_t *out_max)
{
    size_t min = 1;
    size_t max = 1;

    if (name == NULL)
        return false;

    if (strcmp(name, "HashMap") == 0) {
        min = 2;
        max = 2;
    } else if (strcmp(name, "Result") == 0) {
        min = 1;
        max = 2;
    } else if (!(strcmp(name, "Array") == 0
            || strcmp(name, "Slice") == 0
            || strcmp(name, "List") == 0
            || strcmp(name, "Queue") == 0
            || strcmp(name, "Set") == 0
            || strcmp(name, "Box") == 0
            || strcmp(name, "Rc") == 0
            || strcmp(name, "Weak") == 0
            || strcmp(name, "Channel") == 0
            || strcmp(name, "Future") == 0
            || strcmp(name, "RemoteFuture") == 0
            || strcmp(name, "Token") == 0
            || strcmp(name, "DeviceSlot") == 0
            || strcmp(name, "Option") == 0
            || strcmp(name, "Slot") == 0
            || strcmp(name, "SecureSlot") == 0
            || strcmp(name, "ReadView") == 0
            || strcmp(name, "WriteView") == 0
            || strcmp(name, "MoveToken") == 0)) {
        return false;
    }

    if (out_min != NULL)
        *out_min = min;
    if (out_max != NULL)
        *out_max = max;
    return true;
}

bool
semantic_type_resolution_metadata_stable_slot_like_shell(const char *name)
{
    return name != NULL
        && (strcmp(name, "Slot") == 0
            || strcmp(name, "SecureSlot") == 0
            || strcmp(name, "ReadView") == 0
            || strcmp(name, "WriteView") == 0
            || strcmp(name, "MoveToken") == 0);
}

Type *
semantic_type_resolution_metadata_stable_constructed_shell(const char *name,
                                                           size_t argc)
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

bool
semantic_type_resolution_metadata_name_is_shadowed_class(SemanticContext *ctx,
                                                         const char *name)
{
    Symbol *sym;

    if (ctx == NULL || name == NULL)
        return false;
    sym = scope_lookup(ctx->scope, name);
    return sym != NULL && sym->kind == SYMBOL_CLASS;
}

bool
semantic_type_resolution_reject_invalid_stable_shell_arity(
    SemanticContext *ctx,
    ASTNode *type_node)
{
    const char *name;
    size_t min_args;
    size_t max_args;
    size_t provided;
    bool slot_like;

    if (ctx == NULL || type_node == NULL || type_node->type != AST_TYPE)
        return false;
    name = type_node->data.type.name;
    if (!semantic_type_resolution_metadata_stable_builtin_shell_arity(
            name, &min_args, &max_args))
        return false;
    if (semantic_type_resolution_metadata_name_is_shadowed_class(ctx, name))
        return false;

    provided = type_node->data.type.generic_args != NULL
        ? type_node->data.type.generic_args->count
        : 0;
    if (provided >= min_args && provided <= max_args)
        return false;

    slot_like = semantic_type_resolution_metadata_stable_slot_like_shell(name);
    if (slot_like) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_CLASS_CONTRACT_INVALID,
            PGY_CAUSE_CLASS_CONTRACT,
            PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN,
            type_node,
            max_args == 1
                ? "%s requires exactly one type argument"
                : "%s requires one or two type arguments",
            name);
        return true;
    }
    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_INFER_GENERIC,
        PGY_CAUSE_GENERIC_ARGS_INVALID,
        PGY_FIX_ALIGN_GENERIC_ARG_LIST,
        type_node,
        max_args == 1
            ? "%s requires exactly one type argument"
            : "%s requires one or two type arguments",
        name);
    return true;
}

static Type *
metadata_resolve_generic_arg_for_diagnostic(SemanticContext *ctx,
                                            GenericParam *gp)
{
    if (ctx == NULL || gp == NULL)
        return NULL;
    if (gp->constraint != NULL)
        return semantic_type_resolution_lookup_type_ref_or_materialize(
            ctx, gp->constraint);
    return semantic_type_resolution_lookup_metadata_name_or_alias(ctx, gp->name);
}

bool
semantic_type_resolution_reject_invalid_stable_constructed_type(
    SemanticContext *ctx,
    ASTNode *type_node)
{
    GenericParams *args_node;
    Type *args[2];
    const char *name;
    size_t min_args;
    size_t max_args;
    size_t argc;

    if (ctx == NULL || type_node == NULL || type_node->type != AST_TYPE)
        return false;
    name = type_node->data.type.name;
    if (!semantic_type_resolution_metadata_stable_builtin_shell_arity(
            name, &min_args, &max_args))
        return false;
    if (semantic_type_resolution_metadata_name_is_shadowed_class(ctx, name))
        return false;
    args_node = type_node->data.type.generic_args;
    argc = args_node != NULL ? args_node->count : 0;
    if (argc < min_args || argc > max_args || argc > 2)
        return false;

    for (size_t i = 0; i < argc; i++) {
        args[i] = metadata_resolve_generic_arg_for_diagnostic(
            ctx, args_node->params[i]);
        if (args[i] == TYPE_UNKNOWN)
            return true;
        if (args[i] == NULL)
            return false;
    }

    if (strcmp(name, "HashMap") == 0
        && !type_equals(args[0], TYPE_STRING)
        && !type_equals(args[0], TYPE_INT)
        && !type_equals(args[0], TYPE_LONG)
        && !type_equals(args[0], TYPE_BOOL)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
            PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            type_node,
            "HashMap currently supports only String, Int, Long, or Bool keys");
        return true;
    }

    return false;
}

bool
semantic_type_resolution_reject_unknown_bare_named_type(SemanticContext *ctx,
                                                        ASTNode *type_node)
{
    const char *name;

    if (ctx == NULL || type_node == NULL || type_node->type != AST_TYPE)
        return false;
    if (!semantic_type_resolution_metadata_type_ref_has_no_generic_args(
            type_node))
        return false;
    name = type_node->data.type.name;
    if (name == NULL)
        return false;
    if (semantic_type_resolution_metadata_builtin_singleton(name) != NULL)
        return false;
    if (scope_lookup(ctx->scope, name) != NULL)
        return false;
    if (ctx->program_root != NULL
        && find_type_alias_decl(ctx->program_root, name) != NULL) {
        return false;
    }

    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_UNKNOWN_TYPE,
        PGY_CAUSE_TYPE_UNKNOWN,
        PGY_FIX_IMPORT_OR_DECLARE_TYPE,
        type_node,
        "Unknown type '%s'",
        name);
    return true;
}
