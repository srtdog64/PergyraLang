#include <stdlib.h>
#include <string.h>

#include "diag_codes.h"
#include "type_checker_collection_policy.h"
#include "type_checker_internal.h"

typedef struct StableShellSpec {
    const char *name;
    Type **constructor;
    size_t min_args;
    size_t max_args;
    bool slot_like;
} StableShellSpec;

static int
metadata_stable_shell_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const StableShellSpec *spec = (const StableShellSpec *)entry;

    return strcmp(name, spec->name);
}

static const StableShellSpec *
metadata_stable_shell_spec(const char *name)
{
    static const StableShellSpec specs[] = {
        { "Array", &TYPE_ARRAY, 1, 1, false },
        { "Box", &TYPE_BOX, 1, 1, false },
        { "Channel", &TYPE_CHANNEL, 1, 1, false },
        { "DeviceSlot", &TYPE_DEVICE_SLOT, 1, 1, false },
        { "Future", &TYPE_FUTURE, 1, 1, false },
        { "HashMap", &TYPE_HASHMAP, 2, 2, false },
        { "List", &TYPE_LIST, 1, 1, false },
        { "MoveToken", NULL, 1, 1, true },
        { "Option", &TYPE_OPTION, 1, 1, false },
        { "Queue", &TYPE_QUEUE, 1, 1, false },
        { "Rc", &TYPE_RC, 1, 1, false },
        { "ReadView", NULL, 1, 1, true },
        { "RemoteFuture", &TYPE_REMOTE_FUTURE, 1, 1, false },
        { "Result", &TYPE_RESULT, 1, 2, false },
        { "SecureSlot", NULL, 1, 1, true },
        { "Set", &TYPE_SET, 1, 1, false },
        { "Slice", &TYPE_SLICE, 1, 1, false },
        { "Slot", NULL, 1, 1, true },
        { "Token", &TYPE_TOKEN, 1, 1, false },
        { "Weak", &TYPE_WEAK, 1, 1, false },
        { "WriteView", NULL, 1, 1, true },
    };
    const size_t count = sizeof(specs) / sizeof(specs[0]);
    const StableShellSpec *match;

    if (name == NULL)
        return NULL;
    match = (const StableShellSpec *)bsearch(
        &name, specs, count, sizeof(specs[0]),
        metadata_stable_shell_compare);
    return match;
}

bool
semantic_type_resolution_metadata_type_ref_has_no_generic_args(
    const ASTNode *type_node)
{
    return type_node != NULL
        && type_node->type == AST_TYPE
        && ast_generic_param_count(ast_type_generic_args(type_node)) == 0;
}

Type *
semantic_type_resolution_lookup_metadata_name_or_alias_or_unknown(
    SemanticContext *ctx,
    const char *name,
    ASTNode *site)
{
    Type *resolved;

    if (name == NULL || name[0] == '\0')
        return TYPE_UNKNOWN;

    resolved = semantic_type_resolution_lookup_metadata_name_or_alias(ctx,
                                                                      name);
    if (resolved != NULL)
        return resolved;

    semantic_error_with_hints(ctx, PGY_CODE_SEM_UNKNOWN_TYPE,
        PGY_CAUSE_TYPE_UNKNOWN, PGY_FIX_IMPORT_OR_DECLARE_TYPE, site,
        "Unknown type '%s'", name);
    return TYPE_UNKNOWN;
}

bool
semantic_type_resolution_metadata_stable_builtin_shell_arity(
    const char *name,
    size_t *out_min,
    size_t *out_max)
{
    size_t min = 1;
    size_t max = 1;
    const StableShellSpec *spec = metadata_stable_shell_spec(name);

    if (spec == NULL)
        return false;

    min = spec->min_args;
    max = spec->max_args;

    if (out_min != NULL)
        *out_min = min;
    if (out_max != NULL)
        *out_max = max;
    return true;
}

bool
semantic_type_resolution_metadata_stable_slot_like_shell(const char *name)
{
    const StableShellSpec *spec = metadata_stable_shell_spec(name);
    return spec != NULL && spec->slot_like;
}

Type *
semantic_type_resolution_metadata_stable_constructed_shell(const char *name,
                                                           size_t argc)
{
    const StableShellSpec *spec = metadata_stable_shell_spec(name);

    if (spec == NULL || spec->constructor == NULL)
        return NULL;
    if (argc < spec->min_args || argc > spec->max_args)
        return NULL;
    return *spec->constructor;
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
    name = ast_type_name(type_node);
    if (!semantic_type_resolution_metadata_stable_builtin_shell_arity(
            name, &min_args, &max_args))
        return false;
    if (semantic_type_resolution_metadata_name_is_shadowed_class(ctx, name))
        return false;

    provided = ast_generic_param_count(ast_type_generic_args(type_node));
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
    ASTNode *constraint = ast_generic_param_constraint(gp);
    const char *name = ast_generic_param_name(gp);
    if (constraint != NULL)
        return semantic_type_resolution_lookup_metadata_type_ref(ctx,
                                                                 constraint);
    return semantic_type_resolution_lookup_metadata_name_or_alias(ctx, name);
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
    name = ast_type_name(type_node);
    if (!semantic_type_resolution_metadata_stable_builtin_shell_arity(
            name, &min_args, &max_args))
        return false;
    if (semantic_type_resolution_metadata_name_is_shadowed_class(ctx, name))
        return false;
    args_node = ast_type_generic_args(type_node);
    argc = ast_generic_param_count(args_node);
    if (argc < min_args || argc > max_args || argc > 2)
        return false;

    for (size_t i = 0; i < argc; i++) {
        args[i] = metadata_resolve_generic_arg_for_diagnostic(
            ctx, ast_generic_param_at(args_node, i));
        if (args[i] == TYPE_UNKNOWN)
            return true;
        if (args[i] == NULL)
            return false;
    }

    if (strcmp(name, "HashMap") == 0
        && !type_checker_hashmap_key_supported(args[0])) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
            PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            type_node,
            "HashMap currently supports only %s keys",
            type_checker_hashmap_key_policy_text());
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
    name = ast_type_name(type_node);
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
