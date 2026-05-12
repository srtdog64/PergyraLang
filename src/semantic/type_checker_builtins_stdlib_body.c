/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker ??stdlib builtin dispatch body.
 * Owns the stdlib builtin body that used to be hidden in the builtins
 * include chain.
 * Cross-TU helpers live in type_checker_builtins_internal.h.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../common/string_compat.h"
#include "type_checker_internal.h"
#include "type_checker_builtins_internal.h"
#include "diag_codes.h"

static Type *
stdlib_body_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

static Type *
type_check_builtin_print(ASTNode *expr, SemanticContext *ctx)
{
    if (!check_call_arity(expr, 1, "Print", ctx))
        return TYPE_UNKNOWN;
    require_assignable(stdlib_body_normalize_type(
            type_check_expression(expr->data.call.arguments[0], ctx)),
        TYPE_STRING, expr->data.call.arguments[0], ctx);
    return TYPE_VOID;
}

static Type *
type_check_builtin_sleep(ASTNode *expr, SemanticContext *ctx)
{
    if (!check_call_arity(expr, 1, "Sleep", ctx))
        return TYPE_UNKNOWN;
    require_assignable(stdlib_body_normalize_type(
            type_check_expression(expr->data.call.arguments[0], ctx)),
        TYPE_INT, expr->data.call.arguments[0], ctx);
    semantic_record_effect(ctx, EFFECT_NONDETERMINISTIC);
    return TYPE_VOID;
}

static Type *
type_check_builtin_device_read(ASTNode *expr, const char *name,
                               SemanticContext *ctx)
{
    if (!check_call_arity(expr, 1, name, ctx))
        return TYPE_UNKNOWN;
    return stdlib_body_normalize_type(type_get_constructed_arg(
        type_check_device_handle_arg(expr->data.call.arguments[0],
            ctx, name, false), 0));
}

static Type *
type_check_builtin_device_write(ASTNode *expr, const char *name,
                                SemanticContext *ctx)
{
    Type *slot_type;
    Type *inner;

    if (!check_call_arity(expr, 2, name, ctx))
        return TYPE_UNKNOWN;

    slot_type = type_check_device_handle_arg(
        expr->data.call.arguments[0], ctx, name, false);
    inner = stdlib_body_normalize_type(type_get_constructed_arg(slot_type, 0));
    require_assignable(stdlib_body_normalize_type(
            type_check_expression(expr->data.call.arguments[1], ctx)),
        inner, expr->data.call.arguments[1], ctx);
    return TYPE_VOID;
}

static Type *
type_check_builtin_release_device_slot(ASTNode *expr, const char *name,
                                       SemanticContext *ctx)
{
    ASTNode *slot_arg;
    Type *slot_type;

    if (!check_call_arity(expr, 1, name, ctx))
        return TYPE_UNKNOWN;

    semantic_record_body_summary(ctx, BODY_SUMMARY_DROPS_RESOURCE);
    slot_arg = expr->data.call.arguments[0];
    slot_type = type_check_device_handle_arg(slot_arg, ctx, name, true);
    if (slot_type == TYPE_UNKNOWN)
        return TYPE_UNKNOWN;
    if (slot_arg->type != AST_IDENTIFIER) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, slot_arg,
            "ReleaseDeviceSlot requires a DeviceSlot identifier");
        return TYPE_UNKNOWN;
    }
    {
        Symbol *sym = scope_lookup(ctx->scope, slot_arg->data.identifier.name);
        if (sym != NULL && sym->slot_info.state == SLOT_STATE_RELEASED) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, slot_arg,
                "DeviceSlot '%s' has already been released",
                slot_arg->data.identifier.name);
            return TYPE_UNKNOWN;
        }
    }
    scope_release_slot(ctx->scope, slot_arg->data.identifier.name);
    return TYPE_VOID;
}

static Type *
type_check_builtin_submit_device_read(ASTNode *expr, const char *name,
                                      SemanticContext *ctx)
{
    Type *slot_type;

    if (!check_call_arity(expr, 1, name, ctx))
        return TYPE_UNKNOWN;

    slot_type = type_check_device_handle_arg(
        expr->data.call.arguments[0], ctx, name, false);
    return wrap_constructed(TYPE_REMOTE_FUTURE,
        stdlib_body_normalize_type(type_get_constructed_arg(slot_type, 0)));
}

static Type *
type_check_builtin_clone(ASTNode *expr, SemanticContext *ctx)
{
    Type *arg_type;

    if (!check_call_arity(expr, 1, "Clone", ctx))
        return TYPE_UNKNOWN;

    arg_type = stdlib_body_normalize_type(
        type_check_expression(expr->data.call.arguments[0], ctx));
    return arg_type;
}

typedef enum StdlibBodyBuiltin {
    STDLIB_BODY_NOT_BUILTIN = 0,
    STDLIB_BODY_CANCEL,
    STDLIB_BODY_CHANNEL_CLOSE,
    STDLIB_BODY_CLAIM_QUBIT,
    STDLIB_BODY_ENTANGLE,
    STDLIB_BODY_H,
    STDLIB_BODY_INTO_CLASSICAL,
    STDLIB_BODY_IS_CANCELLED,
    STDLIB_BODY_IS_COLLAPSED,
    STDLIB_BODY_MEASURE,
    STDLIB_BODY_QUBIT_STATE,
    STDLIB_BODY_RELEASE_QUBIT,
    STDLIB_BODY_TO_STRING
} StdlibBodyBuiltin;

typedef struct StdlibBodyBuiltinSpec {
    const char *name;
    StdlibBodyBuiltin kind;
} StdlibBodyBuiltinSpec;

static int
stdlib_body_builtin_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const StdlibBodyBuiltinSpec *spec = (const StdlibBodyBuiltinSpec *)entry;

    return strcmp(name, spec->name);
}

static StdlibBodyBuiltin
stdlib_body_builtin_lookup(const char *name)
{
    static const StdlibBodyBuiltinSpec specs[] = {
        { "Cancel", STDLIB_BODY_CANCEL },
        { "ChannelClose", STDLIB_BODY_CHANNEL_CLOSE },
        { "ClaimQubit", STDLIB_BODY_CLAIM_QUBIT },
        { "Entangle", STDLIB_BODY_ENTANGLE },
        { "H", STDLIB_BODY_H },
        { "IntoClassical", STDLIB_BODY_INTO_CLASSICAL },
        { "IsCancelled", STDLIB_BODY_IS_CANCELLED },
        { "IsCollapsed", STDLIB_BODY_IS_COLLAPSED },
        { "Measure", STDLIB_BODY_MEASURE },
        { "QubitState", STDLIB_BODY_QUBIT_STATE },
        { "ReleaseQubit", STDLIB_BODY_RELEASE_QUBIT },
        { "ToString", STDLIB_BODY_TO_STRING },
    };
    const StdlibBodyBuiltinSpec *match;

    if (name == NULL)
        return STDLIB_BODY_NOT_BUILTIN;

    match = (const StdlibBodyBuiltinSpec *)bsearch(
        &name, specs, sizeof(specs) / sizeof(specs[0]),
        sizeof(specs[0]), stdlib_body_builtin_compare);
    return match != NULL ? match->kind : STDLIB_BODY_NOT_BUILTIN;
}

static Type *
type_check_resolved_stdlib_call(ASTNode *expr, const char *name,
                                SemanticContext *ctx, bool *handled_out)
{
    BuiltinKind kind;

    if (handled_out != NULL)
        *handled_out = true;

    kind = builtin_resolve(name);
    switch (kind) {
    case BUILTIN_PRINT:
        return type_check_builtin_print(expr, ctx);
    case BUILTIN_READ_LINE:
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_NONDETERMINISTIC);
        return TYPE_STRING;
    case BUILTIN_NOW:
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_NONDETERMINISTIC);
        return TYPE_INT;
    case BUILTIN_SLEEP:
        return type_check_builtin_sleep(expr, ctx);
    case BUILTIN_CLAIM_DEVICE_SLOT:
        return type_check_claim_device_slot(expr, ctx);
    case BUILTIN_DEVICE_READ:
        return type_check_builtin_device_read(expr, name, ctx);
    case BUILTIN_DEVICE_WRITE:
        return type_check_builtin_device_write(expr, name, ctx);
    case BUILTIN_RELEASE_DEVICE_SLOT:
        return type_check_builtin_release_device_slot(expr, name, ctx);
    case BUILTIN_SUBMIT_DEVICE_READ:
        return type_check_builtin_submit_device_read(expr, name, ctx);
    case BUILTIN_CLONE:
        return type_check_builtin_clone(expr, ctx);
    default:
        if (handled_out != NULL)
            *handled_out = false;
        return TYPE_UNKNOWN;
    }
}

Type *
type_check_stdlib_call(ASTNode *expr, const char *name, SemanticContext *ctx)
{
    bool handled = false;
    StdlibBodyBuiltin body_builtin = STDLIB_BODY_NOT_BUILTIN;
    Type *resolved_type = type_check_resolved_stdlib_call(
        expr, name, ctx, &handled);
    if (handled)
        return resolved_type;

    Type *scalar_type = type_check_stdlib_scalar_call(expr, name, ctx, &handled);
    if (handled)
        return scalar_type;

    Type *map_type = type_check_stdlib_map_call(expr, name, ctx, &handled);
    if (handled)
        return map_type;

    Type *collection_type = type_check_stdlib_collection_call(
        expr, name, ctx, &handled);
    if (handled)
        return collection_type;

    {
        Type *state_tool_type = type_check_state_tool_builtin(
            expr, name, ctx, &handled);
        if (handled)
            return state_tool_type;
    }

    body_builtin = stdlib_body_builtin_lookup(name);
    switch (body_builtin) {
    case STDLIB_BODY_TO_STRING:
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_STRING;

    case STDLIB_BODY_CLAIM_QUBIT:
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        /* Qubit starts in SUPERPOSITION state (uncollapsed). */
        return TYPE_QUBIT;

    case STDLIB_BODY_NOT_BUILTIN:
    default:
        break;
    }

    Type *variant_type = type_check_stdlib_variant_builtin_call(
        expr, name, ctx, &handled);
    if (handled)
        return variant_type;

    /* ---- Channel builtins ---- */
    Type *transport_type = type_check_stdlib_channel_transport_call(
        expr, name, ctx, &handled);
    if (handled)
        return transport_type;
    {
        Type *channel_state_type = type_check_channel_state_builtin(
            expr, name, ctx, &handled);
        if (handled)
            return channel_state_type;
    }

    switch (body_builtin) {
    case STDLIB_BODY_CHANNEL_CLOSE:
        return type_check_channel_close_builtin(expr, ctx);

    case STDLIB_BODY_CANCEL: {
        Type *task_type;

        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_REMOTE);
        if (semantic_reject_active_slot_view_boundary(expr, ctx,
                "cancel cleanup boundary",
                "cancel may trigger task cleanup on another execution frontier",
                "move cancel")) {
            return TYPE_UNKNOWN;
        }
        task_type = stdlib_body_normalize_type(
            type_check_expression(expr->data.call.arguments[0], ctx));
        if (!type_is_future_like(task_type)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "Cancel requires Future<T> or RemoteFuture<T>, got '%s'",
                type_name_or_unknown(task_type));
            return TYPE_UNKNOWN;
        }
        if (type_check_cancel_rejects_payload(expr->data.call.arguments[0],
                task_type, ctx)) {
            return TYPE_UNKNOWN;
        }
        return TYPE_BOOL;
    }

    case STDLIB_BODY_IS_CANCELLED:
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_REMOTE);
        return TYPE_BOOL;

    case STDLIB_BODY_MEASURE:
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_COLLAPSE);
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        /* State validation: CLASSICAL qubits cannot be measured */
        {
            QubitSemanticState qs = get_qubit_semantic_state(
                expr->data.call.arguments[0], ctx);
            if (qs == QUBIT_STATE_CLASSICAL)
                semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr,
                    "Cannot Measure() a qubit in CLASSICAL state "
                    "(already converted via IntoClassical)");
        }
        set_qubit_semantic_state(expr->data.call.arguments[0], ctx,
                                 QUBIT_STATE_COLLAPSED);
        /* Propagate collapse to all qubits in the same entanglement pool */
        {
            int32_t pool = get_qubit_entangle_pool(
                expr->data.call.arguments[0], ctx);
            if (pool >= 0)
                propagate_collapse_to_pool(ctx, pool);
        }
        return TYPE_INT;

    case STDLIB_BODY_ENTANGLE:
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        require_assignable(type_check_qubit_use(expr->data.call.arguments[1], ctx),
            TYPE_QUBIT, expr->data.call.arguments[1], ctx);
        /* State validation: only SUPERPOSITION/NONE qubits can be entangled */
        {
            QubitSemanticState sa = get_qubit_semantic_state(
                expr->data.call.arguments[0], ctx);
            QubitSemanticState sb = get_qubit_semantic_state(
                expr->data.call.arguments[1], ctx);
            if (sa == QUBIT_STATE_COLLAPSED || sa == QUBIT_STATE_CLASSICAL)
                semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr,
                    "Cannot Entangle() a qubit in %s state",
                    qubit_state_name(sa));
            if (sb == QUBIT_STATE_COLLAPSED || sb == QUBIT_STATE_CLASSICAL)
                semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr,
                    "Cannot Entangle() a qubit in %s state",
                    qubit_state_name(sb));
        }
        set_qubit_semantic_state(expr->data.call.arguments[0], ctx,
                                 QUBIT_STATE_ENTANGLED);
        set_qubit_semantic_state(expr->data.call.arguments[1], ctx,
                                 QUBIT_STATE_ENTANGLED);
        /* Compile-time entanglement pool: allocate / merge */
        {
            int32_t pa = get_qubit_entangle_pool(
                expr->data.call.arguments[0], ctx);
            int32_t pb = get_qubit_entangle_pool(
                expr->data.call.arguments[1], ctx);
            if (pa >= 0 && pb >= 0) {
                if (pa != pb)
                    merge_entangle_pools(ctx, pa, pb);
            } else if (pa >= 0) {
                set_qubit_entangle_pool(expr->data.call.arguments[1], ctx, pa);
            } else if (pb >= 0) {
                set_qubit_entangle_pool(expr->data.call.arguments[0], ctx, pb);
            } else {
                int32_t new_pool = alloc_entangle_pool(ctx);
                set_qubit_entangle_pool(expr->data.call.arguments[0], ctx,
                                        new_pool);
                set_qubit_entangle_pool(expr->data.call.arguments[1], ctx,
                                        new_pool);
            }
        }
        return TYPE_VOID;

    case STDLIB_BODY_QUBIT_STATE:
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        return TYPE_INT;

    case STDLIB_BODY_IS_COLLAPSED:
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        return TYPE_BOOL;

    case STDLIB_BODY_RELEASE_QUBIT:
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_body_summary(ctx, BODY_SUMMARY_DROPS_RESOURCE);
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        consume_qubit_value(expr->data.call.arguments[0], ctx, "released");
        return TYPE_VOID;

    case STDLIB_BODY_H:
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        /* State validation: CLASSICAL qubits cannot receive gate operations */
        {
            QubitSemanticState qs = get_qubit_semantic_state(
                expr->data.call.arguments[0], ctx);
            if (qs == QUBIT_STATE_CLASSICAL)
                semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr,
                    "Cannot apply H() to a qubit in CLASSICAL state "
                    "(already converted via IntoClassical)");
        }
        set_qubit_semantic_state(expr->data.call.arguments[0], ctx,
                                 QUBIT_STATE_SUPERPOSITION);
        return TYPE_VOID;

    case STDLIB_BODY_INTO_CLASSICAL:
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        /* State validation: only COLLAPSED qubits can be converted.
         * Unmeasured/unknown states (NONE) must be rejected. */
        {
            QubitSemanticState qs = get_qubit_semantic_state(
                expr->data.call.arguments[0], ctx);
            if (qs != QUBIT_STATE_COLLAPSED)
                semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr,
                    "IntoClassical() requires a COLLAPSED qubit (after Measure) "
                    "got %s", qubit_state_name(qs));
        }
        set_qubit_semantic_state(expr->data.call.arguments[0], ctx,
                                 QUBIT_STATE_CLASSICAL);
        consume_qubit_value(expr->data.call.arguments[0], ctx,
                            "converted to classical");
        return TYPE_BOOL;

    case STDLIB_BODY_TO_STRING:
    case STDLIB_BODY_CLAIM_QUBIT:
    case STDLIB_BODY_NOT_BUILTIN:
    default:
        break;
    }

    return NULL;
}
