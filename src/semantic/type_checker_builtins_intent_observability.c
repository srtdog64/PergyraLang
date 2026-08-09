/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Intent observability builtin typing consumes the canonical ABI row.
 */

#include "type_checker_internal.h"
#include "type_checker_builtins_internal.h"

#include "../common/intent_observability_abi.h"

static Type *
intent_observability_return_type(PgyIntentObservabilityReturnKind kind)
{
    switch (kind) {
    case PGY_INTENT_OBSERVABILITY_RETURN_INT:
        return TYPE_INT;
    case PGY_INTENT_OBSERVABILITY_RETURN_BOOL:
        return TYPE_BOOL;
    case PGY_INTENT_OBSERVABILITY_RETURN_STRING:
        return TYPE_STRING;
    }
    return TYPE_UNKNOWN;
}

static Type *
intent_observability_argument_type(
    PgyIntentObservabilityArgumentKind kind)
{
    switch (kind) {
    case PGY_INTENT_OBSERVABILITY_ARGUMENT_INT:
        return TYPE_INT;
    case PGY_INTENT_OBSERVABILITY_ARGUMENT_INVALID:
        break;
    }
    return TYPE_UNKNOWN;
}

Type *
type_check_intent_observability_builtin(ASTNode *call,
                                        SemanticContext *ctx,
                                        bool *handled_out)
{
    ASTNode *callee = ast_call_callee(call);
    const char *source_name = ast_identifier_name(callee);
    const PgyIntentObservabilityAbiRow *row =
        pgy_intent_observability_abi_row_by_source(source_name);
    size_t argument_count;

    if (handled_out != NULL)
        *handled_out = row != NULL;
    if (row == NULL)
        return TYPE_UNKNOWN;

    argument_count = pgy_intent_observability_argument_count(row);
    check_call_arity(call, argument_count, row->source_name, ctx);
    for (size_t i = 0;
         call != NULL && i < argument_count && i < ast_call_arg_count(call);
        i++) {
        ASTNode *arg = ast_call_argument(call, i);
        Type *expected = intent_observability_argument_type(
            pgy_intent_observability_argument_kind_at(row, i));
        require_assignable(type_check_expression(arg, ctx), expected, arg, ctx);
    }
    return intent_observability_return_type(row->return_kind);
}
