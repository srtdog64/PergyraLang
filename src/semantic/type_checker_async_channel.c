#include <string.h>

#include "diag_codes.h"
#include "type_checker_internal.h"
#include "type_checker_channel_transport_internal.h"
#include "type_checker_ownership_internal.h"

static const char *
spawn_direct_callee_name(ASTNode *spawned)
{
    ASTNode *callee;

    if (spawned == NULL || spawned->type != AST_CALL)
        return NULL;
    callee = ast_call_callee(spawned);
    if (callee == NULL || callee->type != AST_IDENTIFIER)
        return NULL;
    return ast_identifier_name(callee);
}

static FuncParam *
spawn_callable_param_at(ASTNode *decl, size_t index)
{
    if (decl == NULL || decl->type != AST_FUNC_DECL)
        return NULL;
    if (decl->is_async_decl) {
        return ast_async_func_param(decl, index);
    }
    return ast_func_param(decl, index);
}

static ASTNode *
spawn_find_callable_decl(ASTNode *program, const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *stmt = ast_program_statement(program, i);
        if (stmt == NULL || stmt->type != AST_FUNC_DECL)
            continue;
        if (stmt->is_async_decl) {
            const char *async_name = ast_async_func_name(stmt);
            if (async_name != NULL && strcmp(async_name, name) == 0)
                return stmt;
            continue;
        }
        const char *stmt_name = ast_declaration_name(stmt);
        if (stmt_name != NULL && strcmp(stmt_name, name) == 0)
            return stmt;
    }
    return NULL;
}

static bool
semantic_channel_type_is_token(const Type *type);

static Type *
async_channel_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

static bool
semantic_type_ref_names_token(ASTNode *type_ref)
{
    if (type_ref == NULL)
        return false;
    if (type_ref->type == AST_TYPE)
        return ast_type_name(type_ref) != NULL
            && strcmp(ast_type_name(type_ref), "Token") == 0;
    if (type_ref->type == AST_IDENTIFIER)
        return ast_identifier_name(type_ref) != NULL
            && strcmp(ast_identifier_name(type_ref), "Token") == 0;
    return false;
}

static bool
semantic_validate_spawn_token_boundary(ASTNode *expr, SemanticContext *ctx)
{
    ASTNode *spawned;
    const char *callee_name;
    ASTNode *decl;
    bool rejected = false;

    if (expr == NULL || ctx == NULL)
        return false;

    spawned = ast_spawn_function(expr);
    callee_name = spawn_direct_callee_name(spawned);
    if (callee_name == NULL)
        return false;

    decl = spawn_find_callable_decl(ctx->program_root, callee_name);
    if (decl == NULL || decl->type != AST_FUNC_DECL)
        return false;

    for (size_t i = 0; i < ast_call_arg_count(spawned); i++) {
        ASTNode *arg = ast_call_argument(spawned, i);
        FuncParam *param = spawn_callable_param_at(decl, i);
        Type *param_type = NULL;
        bool param_is_token;

        if (arg == NULL || param == NULL)
            continue;

        param_is_token = semantic_type_ref_names_token(param->type);
        if (!param_is_token) {
            param_type = type_check_func_resolve_param_type(param, ctx);
            param_is_token = semantic_channel_type_is_token(param_type);
        }
        if (!param_is_token)
            continue;

        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_REMOTE_FUTURE_MISUSE,
            PGY_CAUSE_REMOTE_FUTURE_DIRECT_ACCESS,
            PGY_FIX_MOVE_INTO_ASYNC_FUNCTION,
            arg,
            "Authority-bearing Token parameter cannot cross spawn boundary.\n"
            "Reason:\n"
            "- spawn may move execution to another task before authority provenance is observed\n"
            "- beta authority tokens are local to the issuing authorized flow\n"
            "- unsupported token transport would make authority rejection non-queryable\n"
            "Fix:\n"
            "- keep Token<T> use in the synchronous authorized flow\n"
            "- or pass a plain projection/value result into '%s' instead",
            callee_name);
        rejected = true;
    }
    return rejected;
}

static void
semantic_validate_spawn_ref_boundary(ASTNode *expr,
                                     SemanticContext *ctx,
                                     Type *inner)
{
    ASTNode *spawned;
    const char *callee_name;
    ASTNode *decl;

    (void)inner;
    if (expr == NULL || ctx == NULL)
        return;

    spawned = ast_spawn_function(expr);
    callee_name = spawn_direct_callee_name(spawned);
    if (callee_name == NULL)
        return;

    decl = spawn_find_callable_decl(ctx->program_root, callee_name);
    if (decl == NULL || decl->type != AST_FUNC_DECL)
        return;

    for (size_t i = 0; i < ast_call_arg_count(spawned); i++) {
        ASTNode *arg = ast_call_argument(spawned, i);
        FuncParam *param = spawn_callable_param_at(decl, i);
        Type *param_type;
        OwnershipTypeClass ownership_class;
        const char *arg_label;

        if (arg == NULL || param == NULL || param->mode != PARAM_MODE_REF)
            continue;

        param_type = type_check_func_resolve_param_type(param, ctx);
        ownership_class = semantic_classify_ownership_type(param_type, ctx);
        if (ownership_class == OWNERSHIP_TYPE_COPY_ONLY)
            continue;

        arg_label = "<argument>";
        if (arg->type == AST_IDENTIFIER && ast_identifier_name(arg) != NULL)
            arg_label = ast_identifier_name(arg);

        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE,
            PGY_FIX_CHANGE_REF_TO_OWN_OR_STOP_ESCAPE,
            arg,
            "Borrowed ref %s '%s' cannot cross spawn boundary.\n"
            "Reason:\n"
            "- spawn may continue after the current synchronous call frame advances\n"
            "- ref is non-owning, so the spawned task would not own the %s provenance\n"
            "- beta-stable task boundaries allow copy-only refs, or explicit ownership transfer\n"
            "Fix:\n"
            "- change parameter %llu of '%s' to 'own' and move the value into spawn\n"
            "- or pass a copy/projection/value snapshot instead of a borrowed boundary value",
            semantic_ownership_value_label(ownership_class),
            arg_label,
            semantic_ownership_provenance_label(ownership_class),
            (unsigned long long)(i + 1),
            callee_name);
    }
}

static bool
semantic_reject_anonymous_async_spawn(ASTNode *expr, SemanticContext *ctx)
{
    ASTNode *spawned;

    if (expr == NULL || ctx == NULL)
        return false;
    spawned = ast_spawn_function(expr);
    if (spawned == NULL
        || spawned->type != AST_FUNC_DECL
        || !spawned->is_async_decl)
        return false;

    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_REMOTE_FUTURE_MISUSE,
        PGY_CAUSE_REMOTE_FUTURE_DIRECT_ACCESS,
        PGY_FIX_MOVE_INTO_ASYNC_FUNCTION,
        spawned,
        "Anonymous async spawn bodies are beta-out-of-scope.\n"
        "Reason:\n"
        "- parser accepts 'spawn async () { ... }', but beta semantics only "
        "close named async/function calls at the spawn boundary\n"
        "- anonymous spawned bodies need closure capture and lifetime analysis "
        "before they can be trusted\n"
        "Fix:\n"
        "- move the body into a named async function\n"
        "- call it with 'spawn Worker(args...)' so parameter ownership and "
        "effects are checked explicitly");
    return true;
}

Type *
type_check_spawn_expr(ASTNode *expr, SemanticContext *ctx)
{
    Type *args[1];
    Type *inner;

    semantic_record_body_summary(ctx, BODY_SUMMARY_SPAWNS_TASK);
    semantic_record_effect(ctx, EFFECT_REMOTE);
    if (semantic_reject_active_slot_view_boundary(expr, ctx,
            "spawn suspension boundary",
            "spawn may run after the current synchronous frame advances",
            "move spawn")) {
        Type *unknown_args[1] = { TYPE_UNKNOWN };
        return type_create_constructed(TYPE_FUTURE, unknown_args, 1);
    }
    if (semantic_reject_anonymous_async_spawn(expr, ctx)) {
        Type *unknown_args[1] = { TYPE_UNKNOWN };
        return type_create_constructed(TYPE_FUTURE, unknown_args, 1);
    }
    if (semantic_validate_spawn_token_boundary(expr, ctx)) {
        args[0] = TYPE_UNKNOWN;
        return type_create_constructed(TYPE_FUTURE, args, 1);
    }
    /* Type-check the spawned function/expression */
    inner = async_channel_normalize_type(
        type_check_expression(ast_spawn_function(expr), ctx));
    semantic_validate_spawn_ref_boundary(expr, ctx, inner);
    args[0] = inner;
    return type_create_constructed(TYPE_FUTURE, args, 1);
}

static bool
semantic_channel_type_is_token(const Type *type)
{
    return type_is_constructed_named(type, "Token");
}

Type *
type_check_channel_send(ASTNode *expr, SemanticContext *ctx)
{
    ASTNode *channel = ast_channel_send_channel(expr);
    ASTNode *value = ast_channel_send_value(expr);

    semantic_record_body_summary(ctx, BODY_SUMMARY_SENDS_CHANNEL);
    semantic_record_effect(ctx, EFFECT_REMOTE);
    if (semantic_reject_active_slot_view_boundary(expr, ctx,
            "channel handoff boundary",
            "channel send may hand the value to another execution frontier",
            "move the channel send")) {
        return TYPE_VOID;
    }
    /* Check channel and value types */
    Type *channel_type = async_channel_normalize_type(
        type_check_expression(channel, ctx));
    Type *value_type = async_channel_normalize_type(
        type_check_expression(value, ctx));
    if (!type_constructed_is(channel_type, TYPE_CHANNEL, 1)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_CHANNEL_TRANSPORT_INVALID,
            PGY_CAUSE_CHANNEL_TRANSPORT_RULE_VIOLATION,
            PGY_FIX_ALIGN_CHANNEL_ELEMENT_TYPE,
            channel,
            "Channel send requires Channel<T>, got '%s'",
            type_name_or_unknown(channel_type));
        return TYPE_VOID;
    }

    Type *element_type = async_channel_normalize_type(
        type_get_constructed_arg(channel_type, 0));
    OwnershipTypeClass element_ownership =
        semantic_classify_ownership_type(element_type, ctx);
    OwnershipTypeClass value_ownership =
        semantic_classify_ownership_type(value_type, ctx);

    if (semantic_channel_type_is_token(element_type)
        || semantic_channel_type_is_token(value_type)) {
        semantic_report_channel_transport_policy(
            value, ctx,
            "Channels cannot transport Token values yet",
            "token transfer would cross the channel boundary without a closed authority contract\n"
            "- authority-bearing token state must remain local for now",
            "keep the token local to the authorized flow\n"
            "- or send a plain projection/value instead");
        return TYPE_VOID;
    }

    if (element_ownership == OWNERSHIP_TYPE_SUBJECT_IDENTITY
        || value_ownership == OWNERSHIP_TYPE_SUBJECT_IDENTITY) {
        if (semantic_validate_channel_transport_ownership(
                value, value_type, ctx,
                "Channel send",
                OWNERSHIP_TYPE_SUBJECT_IDENTITY,
                element_ownership, value_ownership,
                "subject",
                type_name_or_unknown(element_type),
                type_name_or_unknown(value_type),
                "subject",
                "bind the subject first in a local variable")) {
            return TYPE_VOID;
        }
        consume_qubit_value(value, ctx, "sent through channel");
        return TYPE_VOID;
    }

    if (element_ownership == OWNERSHIP_TYPE_MOVE_ONLY
        || value_ownership == OWNERSHIP_TYPE_MOVE_ONLY) {
        if (element_ownership != OWNERSHIP_TYPE_MOVE_ONLY
            || value_ownership != OWNERSHIP_TYPE_MOVE_ONLY) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_CHANNEL_TRANSPORT_INVALID,
                PGY_CAUSE_CHANNEL_TRANSPORT_RULE_VIOLATION,
                PGY_FIX_ALIGN_CHANNEL_ELEMENT_TYPE,
                value,
                "Channel send slot-handle (movable) mismatch: expected '%s', got '%s'.\n"
                "Reason:\n"
                "- channel element type and sent value must agree on the slot handle (movable) contract\n"
                "- ownership transfer cannot be derived when the boundary expects '%s' but received '%s'\n"
                "Fix:\n"
                "- send a value of type '%s'\n"
                "- or change the channel element type to match '%s'",
                resource_handle_display_name(element_type),
                resource_handle_display_name(value_type),
                resource_handle_display_name(element_type),
                resource_handle_display_name(value_type),
                resource_handle_display_name(element_type),
                resource_handle_display_name(value_type));
            return TYPE_VOID;
        }
        if (semantic_check_channel_send_borrowed_transfer(
                value, ctx,
                "slot handle (movable)",
                "slot-handle (movable) provenance",
                "copied/value/projection result",
                "bind the value first by storing the slot handle (movable) in a local variable")) {
            return TYPE_VOID;
        }
        consume_qubit_value(value, ctx, "sent through channel");
        return TYPE_VOID;
    }

    if (element_ownership == OWNERSHIP_TYPE_ANCHORED_HANDLE
        || value_ownership == OWNERSHIP_TYPE_ANCHORED_HANDLE) {
        if (semantic_validate_channel_transport_ownership(
                value, value_type, ctx,
                "Channel send",
                OWNERSHIP_TYPE_ANCHORED_HANDLE,
                element_ownership, value_ownership,
                "slot handle (anchored)",
                resource_handle_display_name(element_type),
                resource_handle_display_name(value_type),
                "slot handle (anchored)",
                "bind the slot handle (anchored) first in a local variable")) {
            return TYPE_VOID;
        }
        return TYPE_VOID;
    }

    if (element_ownership == OWNERSHIP_TYPE_BORROW_TRACKED
        || value_ownership == OWNERSHIP_TYPE_BORROW_TRACKED) {
        if (semantic_validate_channel_transport_ownership(
                value, value_type, ctx,
                "Channel send",
                OWNERSHIP_TYPE_BORROW_TRACKED,
                element_ownership, value_ownership,
                "boundary value",
                type_name_or_unknown(element_type),
                type_name_or_unknown(value_type),
                "boundary value",
                "bind the value first in a local variable")) {
            return TYPE_VOID;
        }
        consume_qubit_value(value, ctx, "sent through channel");
        return TYPE_VOID;
    }

    require_assignable(value_type, element_type, value, ctx);
    return TYPE_VOID;
}

Type *
type_check_channel_recv(ASTNode *expr, SemanticContext *ctx)
{
    ASTNode *channel = ast_channel_recv_channel(expr);

    semantic_record_effect(ctx, EFFECT_REMOTE);
    if (semantic_reject_active_slot_view_boundary(expr, ctx,
            "channel handoff boundary",
            "channel receive may observe work from another execution frontier",
            "move the channel receive")) {
        return TYPE_UNKNOWN;
    }
    Type *channel_type = async_channel_normalize_type(
        type_check_expression(channel, ctx));
    if (!type_constructed_is(channel_type, TYPE_CHANNEL, 1)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_CHANNEL_TRANSPORT_INVALID,
            PGY_CAUSE_CHANNEL_TRANSPORT_RULE_VIOLATION,
            PGY_FIX_ALIGN_CHANNEL_ELEMENT_TYPE,
            channel,
            "Channel recv requires Channel<T>, got '%s'",
            type_name_or_unknown(channel_type));
        return TYPE_UNKNOWN;
    }

    Type *element_type = async_channel_normalize_type(
        type_get_constructed_arg(channel_type, 0));
    if (semantic_channel_type_is_token(element_type)) {
        semantic_report_channel_transport_policy(
            channel, ctx,
            "Channels cannot yield Token values yet",
            "receive would materialize token state outside the closed authority flow\n"
            "- authority-bearing token state remains local-only at channel boundaries under the current authority contract",
            "receive a plain value instead\n"
            "- or keep the token local");
        return TYPE_UNKNOWN;
    }

    return element_type;
}
