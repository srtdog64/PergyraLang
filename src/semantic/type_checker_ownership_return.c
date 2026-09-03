/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Return-statement ownership boundary checks.
 */

#include "type_checker_internal.h"
#include "type_checker_ownership_consumers_internal.h"
#include "diag_codes.h"

static Type *
ownership_return_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

static Type *
ownership_return_apply_context(ASTNode *value, Type *value_type,
                               Type *expected_type)
{
    if (value == NULL || expected_type == NULL || expected_type == TYPE_UNKNOWN)
        return value_type;
    if (value->type == AST_ARRAY_LITERAL
        && ast_array_literal_count(value) == 0
        && type_is_constructed_named(expected_type, "Array")) {
        return expected_type;
    }
    return value_type;
}

/* Accumulate one return's type into the inferred return for a function with no
 * `-> Type`. The first record sets the type; later records must unify or a loud
 * conflict is raised (mirrors the "loud, never silent" stance elsewhere). */
static void
ownership_return_record_inferred(SemanticContext *ctx, Type *ret_type,
                                 ASTNode *node)
{
    Type *rt = ownership_return_normalize_type(ret_type);

    if (ctx->inferred_return == NULL) {
        ctx->inferred_return = rt;
        return;
    }
    if (ctx->inferred_return_conflict)
        return;
    if (rt == TYPE_UNKNOWN || ctx->inferred_return == TYPE_UNKNOWN)
        return;
    if (!type_equals(ctx->inferred_return, rt)) {
        ctx->inferred_return_conflict = true;
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_INFER_REQUIRED,
            PGY_CAUSE_INFER_NO_SOURCE,
            PGY_FIX_ALIGN_OPERAND_TYPE,
            node,
            "Return types disagree across paths and cannot be inferred ('%s' vs '%s').\n"
            "Reason:\n"
            "- a function without an explicit '-> Type' infers its return from the body\n"
            "- two reachable returns produce incompatible types\n"
            "Fix:\n"
            "- add an explicit '-> Type' annotation\n"
            "- or make all returns produce the same type",
            type_name_or_unknown(ctx->inferred_return),
            type_name_or_unknown(rt));
    }
}

bool
type_check_return_stmt(ASTNode *node, SemanticContext *ctx)
{
    Type *ret_type = TYPE_VOID;
    ASTNode *value = ast_return_value(node);

    semantic_require_no_live_text_builder(ctx->scope, node, ctx, "return");

    semantic_record_body_summary(ctx, BODY_SUMMARY_MAY_RETURN);

    if (value != NULL && value->type == AST_LAMBDA_EXPR
        && ctx->current_return != NULL
        && ctx->current_return->kind == TYPE_KIND_FUNCTION) {
        Type *saved_expected_lambda = ctx->expected_lambda_type;
        ctx->expected_lambda_type = ctx->current_return;
        ret_type = ownership_return_normalize_type(
            type_check_expression(value, ctx));
        ctx->expected_lambda_type = saved_expected_lambda;
    } else if (value != NULL) {
        ret_type = ownership_return_normalize_type(
            type_check_expression(value, ctx));
    }
    ret_type = ownership_return_apply_context(value, ret_type,
                                               ctx->current_return);

    if (value != NULL && ctx->current_return != NULL
        && type_equals(ctx->current_return, TYPE_VOID)
        && type_equals(ret_type, TYPE_VOID)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_ASSIGNABILITY_CHECK,
            PGY_FIX_ALIGN_OPERAND_TYPE,
            value,
            "Void function return must not carry a Void expression value.\n"
            "Reason:\n"
            "- Void calls are statement-only side effects, not return values\n"
            "- lowering 'return <Void expression>' requires a backend placeholder\n"
            "Fix:\n"
            "- emit the side-effecting call as a statement before 'return'\n"
            "- or use bare 'return' in a Void function");
        ret_type = TYPE_UNKNOWN;
    }

    if (ctx->inferring_return)
        ownership_return_record_inferred(ctx, ret_type, node);
    else if (ctx->current_return != NULL)
        require_assignable(ret_type, ctx->current_return, node, ctx);

    if (value != NULL) {
        if (semantic_reject_active_slot_owner_escape(
                value, ctx, "return", "return")) {
            return false;
        }
        if (type_is_read_view(ret_type) || type_is_write_view(ret_type)) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_PIN_ESCAPE,
                PGY_CAUSE_PIN_ESCAPE,
                PGY_FIX_CHANGE_REF_TO_OWN_OR_STOP_ESCAPE,
                value,
                "Pinned view cannot escape through return.\n"
                "Reason:\n"
                "- %s is a lexical capability lease over an owning slot\n"
                "- returning it would let the view outlive cleanup and CFG frontier checks\n"
                "Fix:\n"
                "- return a copied value or projection instead\n"
                "- or keep all ReadView/WriteView use inside the current scope",
                type_is_write_view(ret_type) ? "WriteView<T>" : "ReadView<T>");
            return false;
        }
        semantic_validate_borrowed_escape(
            node, value, ctx, ret_type, NULL,
            OWNERSHIP_CONSUMER_RETURN, NULL, NULL, NULL,
            false, NULL, NULL);
    }

    if (value != NULL
        && type_is_qubit(ret_type)
        && value->type == AST_IDENTIFIER) {
        consume_qubit_value(value, ctx, "returned");
    }

    /* A return exits every lexical scope up to the nearest function.  No
     * live completion handle may be discarded by that control-flow edge. */
    semantic_future_require_function_retired(
        ctx->scope, node, ctx, "return");

    return !ctx->has_error;
}
