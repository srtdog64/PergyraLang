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
#include "type_checker_channel_transport_internal.h"
#include "diag_codes.h"

Type *
type_check_stdlib_call(ASTNode *expr, const char *name, SemanticContext *ctx)
{
    bool handled = false;
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

    /* FSM builtins */
    if (strcmp(name, "FsmNew") == 0) { return TYPE_UNKNOWN; }
    if (strcmp(name, "FsmAddState") == 0) {
        if (expr->data.call.arg_count >= 2) {
            type_check_expression(expr->data.call.arguments[0], ctx);
            type_check_expression(expr->data.call.arguments[1], ctx);
        }
        return TYPE_INT;
    }
    if (strcmp(name, "FsmTransition") == 0) {
        if (expr->data.call.arg_count >= 4) {
            for (size_t ai = 0; ai < 4; ai++)
                type_check_expression(expr->data.call.arguments[ai], ctx);
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "FsmStep") == 0) {
        if (expr->data.call.arg_count >= 2) {
            type_check_expression(expr->data.call.arguments[0], ctx);
            type_check_expression(expr->data.call.arguments[1], ctx);
        }
        return TYPE_BOOL;
    }
    if (strcmp(name, "FsmCurrent") == 0) {
        if (expr->data.call.arg_count >= 1)
            type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_INT;
    }
    if (strcmp(name, "FsmCurrentName") == 0) {
        if (expr->data.call.arg_count >= 1)
            type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_STRING;
    }
    /* Timer builtins */
    if (strcmp(name, "TimerNew") == 0) {
        if (expr->data.call.arg_count >= 1)
            type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_UNKNOWN;
    }
    if (strcmp(name, "TimerTick") == 0) {
        if (expr->data.call.arg_count >= 2) {
            type_check_expression(expr->data.call.arguments[0], ctx);
            type_check_expression(expr->data.call.arguments[1], ctx);
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "TimerRemaining") == 0) {
        if (expr->data.call.arg_count >= 1)
            type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_INT;
    }
    if (strcmp(name, "TimerDone") == 0 || strcmp(name, "CooldownReady") == 0) {
        if (expr->data.call.arg_count >= 1)
            type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_BOOL;
    }
    if (strcmp(name, "TimerReset") == 0 || strcmp(name, "CooldownTrigger") == 0) {
        if (expr->data.call.arg_count >= 1)
            type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_VOID;
    }
    if (strcmp(name, "CooldownNew") == 0) {
        if (expr->data.call.arg_count >= 1)
            type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_UNKNOWN;
    }
    if (strcmp(name, "CooldownTick") == 0) {
        if (expr->data.call.arg_count >= 2) {
            type_check_expression(expr->data.call.arguments[0], ctx);
            type_check_expression(expr->data.call.arguments[1], ctx);
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "ToString") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_STRING;
    }
    if (strcmp(name, "Print") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        return TYPE_VOID;
    }
    if (strcmp(name, "ReadLine") == 0) {
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_NONDETERMINISTIC);
        return TYPE_STRING;
    }
    if (strcmp(name, "Now") == 0) {
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_NONDETERMINISTIC);
        return TYPE_INT;
    }
    if (strcmp(name, "Sleep") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_INT, expr->data.call.arguments[0], ctx);
        semantic_record_effect(ctx, EFFECT_NONDETERMINISTIC);
        return TYPE_VOID;
    }

    if (strcmp(name, "ClaimQubit") == 0) {
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        /* Qubit starts in SUPERPOSITION state (uncollapsed). */
        return TYPE_QUBIT;
    }
    if (strcmp(name, "ClaimDeviceSlot") == 0) {
        return type_check_claim_device_slot(expr, ctx);
    }
    if (strcmp(name, "DeviceRead") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        return type_get_constructed_arg(
            type_check_device_handle_arg(expr->data.call.arguments[0], ctx, name, false), 0);
    }
    if (strcmp(name, "DeviceWrite") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        Type *slot_type = type_check_device_handle_arg(
            expr->data.call.arguments[0], ctx, name, false);
        Type *inner = type_get_constructed_arg(slot_type, 0);
        require_assignable(type_check_expression(expr->data.call.arguments[1], ctx),
            inner, expr->data.call.arguments[1], ctx);
        return TYPE_VOID;
    }
    if (strcmp(name, "ReleaseDeviceSlot") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_body_summary(ctx, BODY_SUMMARY_DROPS_RESOURCE);
        ASTNode *slot_arg = expr->data.call.arguments[0];
        Type *slot_type = type_check_device_handle_arg(slot_arg, ctx, name, true);
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
    if (strcmp(name, "SubmitDeviceRead") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        Type *slot_type = type_check_device_handle_arg(
            expr->data.call.arguments[0], ctx, name, false);
        return wrap_constructed(TYPE_REMOTE_FUTURE,
            type_get_constructed_arg(slot_type, 0));
    }

    /* ---- Clone: explicit copy of Slot ---- */
    if (strcmp(name, "Clone") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        Type *arg_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (arg_type == NULL)
            return TYPE_UNKNOWN;
        /* Clone returns the same type ??a fresh independent copy */
        return arg_type;
    }

    /* ---- Result builtins ---- */
    if (strcmp(name, "IsOk") == 0 || strcmp(name, "IsErr") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_BOOL;
    }
    if (strcmp(name, "Some") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        return wrap_constructed(TYPE_OPTION,
            type_check_expression(expr->data.call.arguments[0], ctx));
    }
    if (strcmp(name, "None") == 0) {
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        return wrap_constructed(TYPE_OPTION, TYPE_UNKNOWN);
    }
    if (strcmp(name, "IsSome") == 0 || strcmp(name, "IsNone") == 0) {
        Type *ot;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        ot = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(ot, "Option")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "%s requires Option<T>, got '%s'", name,
                ot != NULL ? ot->name : "<null>");
            return TYPE_UNKNOWN;
        }
        return TYPE_BOOL;
    }
    if (strcmp(name, "UnwrapOption") == 0) {
        Type *ot;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        ot = type_check_expression(expr->data.call.arguments[0], ctx);
        if (type_is_constructed_named(ot, "Option"))
            return type_get_constructed_arg(ot, 0);
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
            "UnwrapOption requires Option<T>, got '%s'",
            ot != NULL ? ot->name : "<null>");
        return TYPE_UNKNOWN;
    }
    if (strcmp(name, "Unwrap") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        Type *rt = type_check_expression(expr->data.call.arguments[0], ctx);
        if (type_is_constructed_named(rt, "Result"))
            return type_get_constructed_arg(rt, 0);
        return TYPE_UNKNOWN;
    }
    if (strcmp(name, "UnwrapOr") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        Type *rt = type_check_expression(expr->data.call.arguments[0], ctx);
        type_check_expression(expr->data.call.arguments[1], ctx);
        if (type_is_constructed_named(rt, "Result"))
            return type_get_constructed_arg(rt, 0);
        return TYPE_UNKNOWN;
    }

    /* ---- Channel builtins ---- */
    if (strcmp(name, "TryRecv") == 0) {
        return type_check_channel_recv_builtin(expr, name, false, ctx);
    }
    if (strcmp(name, "RecvTimeout") == 0) {
        return type_check_channel_recv_builtin(expr, name, true, ctx);
    }
    if (strcmp(name, "TrySend") == 0) {
        return type_check_channel_send_builtin(expr, name, false, false, ctx);
    }
    if (strcmp(name, "SendTimeout") == 0) {
        return type_check_channel_send_builtin(expr, name, true, false, ctx);
    }
    if (strcmp(name, "TrySendStatus") == 0) {
        return type_check_channel_send_builtin(expr, name, false, true, ctx);
    }
    if (strcmp(name, "SendTimeoutStatus") == 0) {
        return type_check_channel_send_builtin(expr, name, true, true, ctx);
    }
    if (strcmp(name, "ChannelLength") == 0
        || strcmp(name, "ChannelCapacity") == 0
        || strcmp(name, "ChannelSpace") == 0
        || strcmp(name, "ChannelFull") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_REMOTE);
        Type *ch_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(ch_type, "Channel")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "%s requires Channel<T>, got '%s'",
                name,
                ch_type != NULL ? ch_type->name : "<null>");
            return TYPE_UNKNOWN;
        }
        return strcmp(name, "ChannelFull") == 0 ? TYPE_BOOL : TYPE_INT;
    }
    if (strcmp(name, "ChannelClosed") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_REMOTE);
        Type *ch_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(ch_type, "Channel")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "ChannelClosed requires Channel<T>, got '%s'",
                ch_type != NULL ? ch_type->name : "<null>");
            return TYPE_UNKNOWN;
        }
        return TYPE_BOOL;
    }
    if (strcmp(name, "ChannelClose") == 0) {
        return type_check_channel_close_builtin(expr, ctx);
    }
    if (strcmp(name, "ChannelReady") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_REMOTE);
        Type *ch_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(ch_type, "Channel")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "ChannelReady requires Channel<T>, got '%s'",
                ch_type != NULL ? ch_type->name : "<null>");
            return TYPE_UNKNOWN;
        }
        return TYPE_BOOL;
    }
    if (strcmp(name, "Cancel") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_REMOTE);
        if (semantic_reject_active_slot_view_boundary(expr, ctx,
                "cancel cleanup boundary",
                "cancel may trigger task cleanup on another execution frontier",
                "move cancel")) {
            return TYPE_UNKNOWN;
        }
        Type *task_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_future_like(task_type)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "Cancel requires Future<T> or RemoteFuture<T>, got '%s'",
                task_type != NULL ? task_type->name : "<null>");
            return TYPE_UNKNOWN;
        }
        if (type_check_cancel_rejects_payload(expr->data.call.arguments[0],
                task_type, ctx)) {
            return TYPE_UNKNOWN;
        }
        return TYPE_BOOL;
    }
    if (strcmp(name, "IsCancelled") == 0) {
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_REMOTE);
        return TYPE_BOOL;
    }

    if (strcmp(name, "Measure") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_NONDETERMINISTIC | EFFECT_COLLAPSE);
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
    }
    if (strcmp(name, "Entangle") == 0) {
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
    }
    if (strcmp(name, "QubitState") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        return TYPE_INT;
    }
    if (strcmp(name, "IsCollapsed") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        return TYPE_BOOL;
    }
    if (strcmp(name, "ReleaseQubit") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_body_summary(ctx, BODY_SUMMARY_DROPS_RESOURCE);
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        consume_qubit_value(expr->data.call.arguments[0], ctx, "released");
        return TYPE_VOID;
    }
    if (strcmp(name, "H") == 0) {
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
    }
    if (strcmp(name, "IntoClassical") == 0) {
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
    }

    return NULL;
}
