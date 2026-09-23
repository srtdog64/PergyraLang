/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Copy contract for single-owner builtin runtime handles (Rc, Weak, Box,
 * Allocator, TextBuilder), and the let-binding contract for Channel.
 *
 * A handle value is the one owner of its runtime storage. A let, assignment,
 * return or generic argument that copies an existing binding would give that
 * storage a second name, and the first release would leave the other one
 * dangling. A by-value parameter borrows the caller's handle for the call, so
 * the callee may read it but not release it. The same rules run in the
 * self-host semantic (ast_single_owner_handle_verdict_owner.pgy).
 */

#include <string.h>

#include "diag_codes.h"
#include "type_checker_ownership_let_internal.h"
#include "type_checker_ownership_support_internal.h"

static bool
single_owner_handle_value_is_place(const ASTNode *value)
{
    return value != NULL
        && (value->type == AST_IDENTIFIER
            || value->type == AST_MEMBER_ACCESS
            || value->type == AST_ARRAY_ACCESS);
}

static const char *
single_owner_handle_fresh_source(const Type *type)
{
    Type *constructor = type_constructed_constructor(type);

    if (constructor != NULL && constructor == TYPE_RC)
        return "RcClone(handle), which counts a second strong owner";
    if (constructor != NULL && constructor == TYPE_WEAK)
        return "RcDowngrade(rc), which counts a second weak owner";
    if (constructor != NULL && constructor == TYPE_BOX)
        return "a new Box(...)";
    if (type_is_builtin_owner_handle(type))
        return "TextBuilderNew(capacity)";
    return "an Allocator constructor such as AllocatorResult()";
}

bool
semantic_reject_single_owner_handle_place_copy(ASTNode *site,
                                               ASTNode *value,
                                               SemanticContext *ctx,
                                               const Type *type,
                                               const char *consumer)
{
    /* A TextBuilder local is never rebound, whatever the new value is. */
    if (ctx == NULL || !type_is_single_owner_runtime_handle(type)
        || (!type_is_builtin_owner_handle(type)
            && !single_owner_handle_value_is_place(value)))
        return false;
    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_ANCHORED_HANDLE_COPY,
        PGY_CAUSE_MOVABLE_HANDLE_COPY_ATTEMPT,
        PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
        site,
        "'%s' is a single-owner runtime handle and cannot be copied into %s.\n"
        "Reason:\n"
        "- the copy would name the same runtime storage as its source\n"
        "- releasing either binding would leave the other one dangling\n"
        "Fix:\n"
        "- keep using the existing binding directly\n"
        "- or take a new handle from %s",
        type_name_or_unknown(type),
        consumer != NULL ? consumer : "another binding",
        single_owner_handle_fresh_source(type));
    return true;
}

bool
semantic_reject_single_owner_handle_return(ASTNode *site,
                                           ASTNode *value,
                                           SemanticContext *ctx,
                                           const Type *type)
{
    Symbol *symbol;

    if (ctx == NULL || !type_is_single_owner_runtime_handle(type)
        || !single_owner_handle_value_is_place(value))
        return false;
    /* A handle held by a local of this function moves out through return:
     * the local ends with the function, so no second name survives. */
    if (value->type == AST_IDENTIFIER) {
        symbol = scope_lookup(ctx->scope, ast_identifier_name(value));
        if (symbol != NULL && !symbol->is_parameter && !symbol->is_host_field)
            return false;
    }
    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_ANCHORED_HANDLE_COPY,
        PGY_CAUSE_MOVABLE_HANDLE_COPY_ATTEMPT,
        PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
        site,
        "'%s' cannot be returned from a parameter, field, or element.\n"
        "Reason:\n"
        "- the caller still holds that handle, so the returned copy would name the same runtime storage\n"
        "- only a handle held by a local of this function moves out through return\n"
        "Fix:\n"
        "- return a handle created in this function\n"
        "- or take a new handle from %s",
        type_name_or_unknown(type),
        single_owner_handle_fresh_source(type));
    return true;
}

void
semantic_reject_single_owner_handle_parameter_release(ASTNode *call,
                                                      BuiltinKind kind,
                                                      SemanticContext *ctx)
{
    const char *operation;
    const char *root_name;
    ASTNode *arg;
    Symbol *root;

    switch (kind) {
    case BUILTIN_RC_DROP: operation = "RcDrop"; break;
    case BUILTIN_WEAK_DROP: operation = "WeakDrop"; break;
    case BUILTIN_BOX_DROP: operation = "BoxDrop"; break;
    case BUILTIN_ALLOCATOR_DESTROY: operation = "AllocatorDestroy"; break;
    default: return;
    }
    if (ctx == NULL || ast_call_arg_count(call) != 1)
        return;
    arg = ast_call_argument(call, 0);
    root_name = semantic_addressable_boundary_root_name(arg);
    root = root_name != NULL ? scope_lookup(ctx->scope, root_name) : NULL;
    if (root == NULL || !(root->is_parameter || root->is_host_field))
        return;
    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
        PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
        PGY_FIX_MATCH_BUILTIN_SIGNATURE,
        arg,
        "%s requires a handle held by a local of the current function; '%s' is %s.\n"
        "Reason:\n"
        "- the caller still holds that handle and releases it itself\n"
        "- releasing it here would leave the caller's binding dangling\n"
        "- an 'own' transfer of a handle is not yet proven on both front ends\n"
        "Fix:\n"
        "- release the handle in the function whose local created it",
        operation, root_name,
        root->is_parameter ? "a parameter" : "a field of the receiver");
}

bool
ownership_let_validate_builtin_owner_binding(ASTNode *node,
                                             SemanticContext *ctx,
                                             Type *decl_type,
                                             ASTNode *init)
{
    ASTNode *callee;
    const char *callee_name = NULL;

    if (!type_is_single_owner_runtime_handle(decl_type))
        return true;
    if (!type_is_builtin_owner_handle(decl_type))
        return !semantic_reject_single_owner_handle_place_copy(
            node, init, ctx, decl_type, "a new binding");

    callee = init != NULL && init->type == AST_CALL
        ? ast_call_callee(init) : NULL;
    if (callee != NULL && callee->type == AST_IDENTIFIER)
        callee_name = ast_identifier_name(callee);

    if (!ast_let_is_mutable(node)
        && callee_name != NULL
        && strcmp(callee_name, "TextBuilderNew") == 0) {
        return true;
    }

    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_ANCHORED_HANDLE_COPY,
        PGY_CAUSE_MOVABLE_HANDLE_COPY_ATTEMPT,
        PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
        node,
        "TextBuilder is a single-owner local and cannot be copied, rebound, or declared mutable.\n"
        "Reason:\n"
        "- the current bounded owner rung admits only a fresh TextBuilderNew(...) initializer\n"
        "- generic move, field, container, and return transfer are not yet proven\n"
        "Fix:\n"
        "- initialize an immutable local directly from TextBuilderNew(capacity)\n"
        "- finish or drop that same local in its declaration scope");
    return false;
}

/*
 * Channel<T> descriptors are by-value structs with interior cursors, so a
 * second let-binding to an existing channel silently copies the descriptor
 * and the copies drift (duplicate/split deliveries even in serial code) --
 * the same class as the rejected Channel parameters and aggregate fields
 * (docs/189 C12, board WO-RT-6). Fail closed: a Channel local may only be
 * born from a fresh constructor call, immutably. If the representation
 * ruling later promotes Channel to a heap handle, this rule relaxes to
 * legal aliasing; until then rejection is the only sound surface.
 */
bool
ownership_let_validate_channel_binding(ASTNode *node,
                                       SemanticContext *ctx,
                                       Type *decl_type,
                                       ASTNode *init)
{
    ASTNode *callee;
    const char *callee_name = NULL;

    if (!type_is_constructed_named(decl_type, "Channel"))
        return true;

    callee = init != NULL && init->type == AST_CALL
        ? ast_call_callee(init) : NULL;
    if (callee != NULL && callee->type == AST_IDENTIFIER)
        callee_name = ast_identifier_name(callee);

    if (!ast_let_is_mutable(node)
        && callee_name != NULL
        && strcmp(callee_name, "Channel") == 0) {
        return true;
    }

    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_ANCHORED_HANDLE_COPY,
        PGY_CAUSE_MOVABLE_HANDLE_COPY_ATTEMPT,
        PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
        node,
        "Channel is identity-bearing and cannot be re-bound, copied, or declared mutable.\n"
        "Reason:\n"
        "- binding an existing channel to another name copies its descriptor (buffer pointer + cursors), so the copies drift and deliveries silently split\n"
        "Fix:\n"
        "- initialize an immutable local directly from Channel(capacity)\n"
        "- use that one channel variable at every send/recv site");
    return false;
}
