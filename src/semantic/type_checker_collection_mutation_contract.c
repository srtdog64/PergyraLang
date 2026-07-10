#include "type_checker_internal.h"
#include "type_checker_builtins_internal.h"
#include "diag_codes.h"

static bool
type_is_mutable_collection_storage(const Type *type)
{
    return type_is_constructed_named(type, "Array")
        || type_is_constructed_named(type, "List")
        || type_is_constructed_named(type, "Set")
        || type_is_constructed_named(type, "Queue")
        || type_is_constructed_named(type, "HashMap");
}

bool
reject_non_inout_param_collection_mutator_receiver(ASTNode *receiver_expr,
                                                   const Type *receiver_type,
                                                   const char *mutator_name,
                                                   const char *container_kind,
                                                   SemanticContext *ctx)
{
    const char *receiver_name;
    Symbol *receiver_sym;

    if (ctx == NULL || receiver_expr == NULL
        || receiver_expr->type != AST_IDENTIFIER)
        return false;
    if (!type_is_mutable_collection_storage(receiver_type))
        return false;

    receiver_name = ast_identifier_name(receiver_expr);
    receiver_sym = receiver_name != NULL
        ? scope_lookup(ctx->scope, receiver_name)
        : NULL;
    if (receiver_sym == NULL || !receiver_sym->is_parameter
        || (receiver_sym->param_mode != PARAM_MODE_DEFAULT
            && receiver_sym->param_mode != PARAM_MODE_REF))
        return false;

    semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
        PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
        PGY_FIX_MATCH_BUILTIN_SIGNATURE, receiver_expr,
        "Collection mutator '%s' cannot target non-inout parameter '%s'.\n"
        "Reason:\n"
        "- caller-visible %s mutation must be explicit at the function boundary\n"
        "- default parameters are values and ref parameters are read-only borrows\n"
        "Fix:\n"
        "- spell the parameter as 'inout %s: %s' when caller mutation is intended\n"
        "- or mutate a local collection/sink and return the result",
        mutator_name != NULL ? mutator_name : "<collection mutator>",
        receiver_name != NULL ? receiver_name : "<receiver>",
        container_kind != NULL ? container_kind : "collection",
        receiver_name != NULL ? receiver_name : "<receiver>",
        type_name_or_unknown(receiver_type));
    return true;
}

bool
reject_parallel_collection_mutator(ASTNode *expr,
                                   const char *name,
                                   bool mutates_storage,
                                   SemanticContext *ctx)
{
    if (ctx == NULL || !ctx->in_parallel || !mutates_storage)
        return false;

    semantic_error_with_hints(ctx, PGY_CODE_SEM_PARALLEL_SLOT_CONFLICT,
        PGY_CAUSE_PARALLEL_RESOURCE_CONFLICT,
        PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL, expr,
        "Parallel context does not permit collection mutator '%s'.\n"
        "Reason:\n"
        "- growable collection storage may reallocate, rehash, or alias while another task observes it\n"
        "- generated C/LLVM workers cannot safely share mutable collection storage by raw pointer\n"
        "Fix:\n"
        "- mutate the collection before or after parallel\n"
        "- or send immutable values through a channel/result boundary",
        name != NULL ? name : "<collection builtin>");
    return true;
}
