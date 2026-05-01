#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_builtins_internal.h"
#include "diag_codes.h"

static Type *
slotops_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

Type *
type_check_claim_slot(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 0) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "ClaimSlot takes no arguments");
        return TYPE_UNKNOWN;
    }

    return TYPE_UNKNOWN;
}

Type *
type_check_move_token(ASTNode *call, SemanticContext *ctx)
{
    if (!check_call_arity(call, 1, "Move", ctx))
        return TYPE_UNKNOWN;

    ASTNode *slot_arg = call->data.call.arguments[0];
    if (slot_arg == NULL || slot_arg->type != AST_IDENTIFIER) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call,
            "Move requires a named owning Slot<T>/SecureSlot<T> binding");
        return TYPE_UNKNOWN;
    }

    Symbol *sym = scope_lookup(ctx->scope, slot_arg->data.identifier.name);
    Type *slot_type = sym != NULL ? sym->type : TYPE_UNKNOWN;
    if (!type_is_owned_slot_handle(slot_type)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, slot_arg,
            "Move requires owning Slot<T>/SecureSlot<T>, got '%s'",
            slot_type != NULL ? slot_type->name : "<null>");
        return TYPE_UNKNOWN;
    }
    if (sym == NULL || sym->kind != SYMBOL_SLOT) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, slot_arg,
            "Move requires an owning slot binding");
        return TYPE_UNKNOWN;
    }
    if (sym->slot_info.state == SLOT_STATE_RELEASED) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_MOVE_FROM_RELEASED, PGY_CAUSE_MOVE_FROM_RELEASED, PGY_FIX_RECLAIM_OR_TRACE_EARLIER_MOVE, slot_arg,
            "Cannot move released slot '%s'",
            sym->name);
        return TYPE_UNKNOWN;
    }
    const char *active_view_name = NULL;
    const char *active_view_kind = NULL;
    if (semantic_find_active_slot_view_for_source(ctx->scope, sym->name,
            &active_view_name, &active_view_kind, NULL)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_PIN_PARALLEL_CONFLICT,
            PGY_CAUSE_PIN_PARALLEL_CONFLICT,
            PGY_FIX_SERIALIZE_PIN_ACCESS,
            slot_arg,
            "Cannot move slot '%s' while %s '%s' is live.\n"
            "Reason:\n"
            "- pinned views are scoped capability leases over the source slot\n"
            "- moving the owner while a view is live would invalidate cleanup and aliasing order\n"
            "Fix:\n"
            "- end the pin/view scope before Move(%s)\n"
            "- or move ownership before acquiring '%s'",
            sym->name,
            active_view_kind != NULL ? active_view_kind : "view",
            active_view_name != NULL ? active_view_name : "<view>",
            sym->name,
            active_view_name != NULL ? active_view_name : "<view>");
        return TYPE_UNKNOWN;
    }

    scope_release_slot(ctx->scope, sym->name);
    return type_create_slot_access(slot_type->data.slot.inner_type,
        slot_type->data.slot.is_secure, SLOT_ACCESS_MOVE_TOKEN);
}

bool
type_check_write_slot(ASTNode *call, SemanticContext *ctx)
{
    size_t arg_count = call->data.call.arg_count;

    if (arg_count < 2) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call,
            "Write requires at least 2 arguments: Write(slot, value)");
        return false;
    }

    ASTNode *slot_arg = call->data.call.arguments[0];
    if (slot_arg != NULL && slot_arg->type == AST_IDENTIFIER) {
        Symbol *target_sym = scope_lookup(ctx->scope,
            slot_arg->data.identifier.name);
        if (target_sym != NULL && target_sym->kind == SYMBOL_SLOT) {
            const char *active_view_name = NULL;
            const char *active_view_kind = NULL;
            if (semantic_find_active_slot_view_for_source(ctx->scope,
                    target_sym->name, &active_view_name, &active_view_kind,
                    NULL)) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_PIN_PARALLEL_CONFLICT,
                    PGY_CAUSE_PIN_PARALLEL_CONFLICT,
                    PGY_FIX_SERIALIZE_PIN_ACCESS,
                    slot_arg,
                    "Cannot write slot '%s' while %s '%s' is live.\n"
                    "Reason:\n"
                    "- pinned views are scoped capability leases over the source slot\n"
                    "- owner writes during a live view would bypass the view's aliasing contract\n"
                    "Fix:\n"
                    "- write through the active view when it is a WriteView<T>\n"
                    "- or end the pin/view scope before Write(%s, ...)",
                    target_sym->name,
                    active_view_kind != NULL ? active_view_kind : "view",
                    active_view_name != NULL ? active_view_name : "<view>",
                    target_sym->name);
                return false;
            }
        }
    }
    Type *slot_type = slotops_normalize_type(type_check_expression(slot_arg, ctx));

    if (type_is_constructed_named(slot_type, "RemoteFuture")) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_REMOTE_FUTURE_MISUSE, PGY_CAUSE_REMOTE_FUTURE_DIRECT_ACCESS, PGY_FIX_AWAIT_FUTURE, slot_arg,
            "RemoteFuture does not support Write(); remote resources are "
            "read-only via 'await'. Use Channel to send data to remote World");
        return false;
    }
    if (slot_type->kind != TYPE_KIND_SLOT) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, slot_arg,
            "First argument to Write must be a Slot, got '%s'",
            slot_type->name != NULL ? slot_type->name : "<unknown>");
        return false;
    }
    if (type_is_read_view(slot_type)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_VIEW_KIND_MISMATCH, PGY_CAUSE_VIEW_KIND_OP_MISMATCH, PGY_FIX_ACQUIRE_MATCHING_VIEW_OR_USE_SLOT, slot_arg,
            "Cannot write through ReadView<T>; create a WriteView(slot) or keep the owning Slot<T>");
        return false;
    }
    if (type_is_move_token(slot_type)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_MOVE_TOKEN_MISUSE, PGY_CAUSE_MOVE_TOKEN_DIRECT_ACCESS, PGY_FIX_MATERIALIZE_TOKEN_TO_SLOT, slot_arg,
            "Cannot write through MoveToken<T>");
        return false;
    }

    if (slot_type->data.slot.is_secure)
        semantic_record_effect(ctx, EFFECT_SECURE);

    if (ctx->in_parallel && slot_type->data.slot.is_secure) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PARALLEL_SECURE_FORBIDDEN, PGY_CAUSE_PARALLEL_SECURE_IN_TASK, PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL, slot_arg,
            "Parallel context does not permit SecureSlot access yet; serialize authority-bearing slot reads/writes/releases outside the parallel block");
        return false;
    }

    if (slot_arg->type == AST_IDENTIFIER) {
        Symbol *sym = scope_lookup(ctx->scope, slot_arg->data.identifier.name);
        if (sym != NULL && sym->kind == SYMBOL_SLOT) {
            if (sym->slot_info.state == SLOT_STATE_RELEASED) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_SLOT_RELEASED,
                    PGY_CAUSE_SLOT_LIFECYCLE_WRITE_AFTER_RELEASE,
                    PGY_FIX_RECLAIM_BEFORE_USE,
                    slot_arg,
                    "Cannot write to released slot '%s'",
                    sym->name);
                return false;
            }

            if (sym->slot_info.is_secure) {
                if (arg_count < 3) {
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                        PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                        PGY_FIX_MATCH_BUILTIN_SIGNATURE,
                        call,
                        "Write to SecureSlot '%s' requires a token argument",
                        sym->name);
                    return false;
                }

                ASTNode *token_arg = call->data.call.arguments[2];
                if (!builtin_validate_secure_token_arg(token_arg, sym, slot_type, ctx))
                    return false;
            } else if (arg_count > 2) {
                semantic_warning(ctx, call,
                    "Write to plain Slot '%s' ignores extra token argument",
                    sym->name);
            }
        } else if (sym != NULL && type_is_write_view(sym->type)
                   && sym->slot_info.paired_slot_name != NULL) {
            Symbol *owner = scope_lookup(ctx->scope, sym->slot_info.paired_slot_name);
            if (owner != NULL && owner->slot_info.state == SLOT_STATE_RELEASED) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_SLOT_RELEASED, PGY_CAUSE_SLOT_VIEW_WRITE_THROUGH_RELEASED_OWNER, PGY_FIX_RECLAIM_SOURCE_OR_DROP_VIEW, slot_arg,
                    "Cannot write through WriteView '%s' because source slot '%s' was released",
                    sym->name, owner->name);
                return false;
            }
            if (owner != NULL && owner->slot_info.is_secure)
                semantic_record_effect(ctx, EFFECT_SECURE);
        }
    }

    ASTNode *value_arg = call->data.call.arguments[1];
    Type *value_type = slotops_normalize_type(type_check_expression(value_arg, ctx));
    Type *inner_type = slotops_normalize_type(slot_type->data.slot.inner_type);

    if (!type_is_assignable(value_type, inner_type)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_SLOT_WRITE_VALUE_TYPE_MISMATCH,
            PGY_FIX_ALIGN_VALUE_TO_SLOT_INNER,
            value_arg,
            "Cannot write '%s' to %s (expected '%s')",
            value_type->name != NULL ? value_type->name : "<unknown>",
            slot_type->name != NULL ? slot_type->name : "<unknown>",
            inner_type->name != NULL ? inner_type->name : "<unknown>");
        return false;
    }

    return true;
}

Type *
type_check_read_slot(ASTNode *call, SemanticContext *ctx)
{
    size_t arg_count = call->data.call.arg_count;

    if (arg_count < 1) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call,
            "Read requires at least 1 argument: Read(slot)");
        return TYPE_UNKNOWN;
    }

    ASTNode *slot_arg = call->data.call.arguments[0];
    Type *slot_type = slotops_normalize_type(type_check_expression(slot_arg, ctx));

    if (type_is_constructed_named(slot_type, "RemoteFuture")) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_REMOTE_FUTURE_MISUSE, PGY_CAUSE_REMOTE_FUTURE_DIRECT_ACCESS, PGY_FIX_AWAIT_FUTURE, slot_arg,
            "RemoteFuture does not support Read(); use 'await' to obtain "
            "Result<T>, then Unwrap() or '?' to extract the value");
        return TYPE_UNKNOWN;
    }
    if (slot_type->kind != TYPE_KIND_SLOT) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, slot_arg,
            "First argument to Read must be a Slot, got '%s'",
            slot_type->name != NULL ? slot_type->name : "<unknown>");
        return TYPE_UNKNOWN;
    }
    if (type_is_write_view(slot_type)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_VIEW_KIND_MISMATCH, PGY_CAUSE_VIEW_KIND_OP_MISMATCH, PGY_FIX_ACQUIRE_MATCHING_VIEW_OR_USE_SLOT, slot_arg,
            "Cannot read through WriteView<T>; create a ReadView(slot) or keep the owning Slot<T>");
        return TYPE_UNKNOWN;
    }
    if (type_is_move_token(slot_type)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_MOVE_TOKEN_MISUSE, PGY_CAUSE_MOVE_TOKEN_DIRECT_ACCESS, PGY_FIX_MATERIALIZE_TOKEN_TO_SLOT, slot_arg,
            "Cannot read through MoveToken<T>");
        return TYPE_UNKNOWN;
    }

    if (slot_type->data.slot.is_secure)
        semantic_record_effect(ctx, EFFECT_SECURE);

    if (ctx->in_parallel && slot_type->data.slot.is_secure) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PARALLEL_SECURE_FORBIDDEN, PGY_CAUSE_PARALLEL_SECURE_IN_TASK, PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL, slot_arg,
            "Parallel context does not permit SecureSlot access yet; serialize authority-bearing slot reads/writes/releases outside the parallel block");
        return TYPE_UNKNOWN;
    }

    if (slot_arg->type == AST_IDENTIFIER) {
        Symbol *sym = scope_lookup(ctx->scope, slot_arg->data.identifier.name);
        if (sym != NULL && sym->kind == SYMBOL_SLOT) {
            if (sym->slot_info.state == SLOT_STATE_RELEASED) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_SLOT_RELEASED,
                    PGY_CAUSE_SLOT_LIFECYCLE_READ_AFTER_RELEASE,
                    PGY_FIX_RECLAIM_BEFORE_USE,
                    slot_arg,
                    "Cannot read from released slot '%s'",
                    sym->name);
                return TYPE_UNKNOWN;
            }

            const char *active_view_name = NULL;
            bool active_is_write = false;
            if (semantic_find_active_slot_view_for_source(ctx->scope, sym->name,
                    &active_view_name, NULL, &active_is_write)
                && active_is_write) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_PIN_PARALLEL_CONFLICT,
                    PGY_CAUSE_PIN_PARALLEL_CONFLICT,
                    PGY_FIX_SERIALIZE_PIN_ACCESS,
                    slot_arg,
                    "Cannot read slot '%s' while WriteView '%s' is live.\n"
                    "Reason:\n"
                    "- WriteView<T> is the exclusive mutable view over the source slot\n"
                    "- owner reads during a live write view would bypass the view's aliasing contract\n"
                    "Fix:\n"
                    "- end the write view scope before Read(%s)\n"
                    "- or split the operation into a read-only view followed by a write view",
                    sym->name,
                    active_view_name != NULL ? active_view_name : "<view>",
                    sym->name);
                return TYPE_UNKNOWN;
            }

            if (sym->slot_info.is_secure) {
                if (arg_count < 2) {
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call,
                        "Read from SecureSlot '%s' requires a token argument",
                        sym->name);
                    return TYPE_UNKNOWN;
                }
                ASTNode *token_arg = call->data.call.arguments[1];
                if (!builtin_validate_secure_token_arg(token_arg, sym, slot_type, ctx))
                    return TYPE_UNKNOWN;
            }
        } else if (sym != NULL && type_is_read_view(sym->type)
                   && sym->slot_info.paired_slot_name != NULL) {
            Symbol *owner = scope_lookup(ctx->scope, sym->slot_info.paired_slot_name);
            if (owner != NULL && owner->slot_info.state == SLOT_STATE_RELEASED) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_SLOT_RELEASED, PGY_CAUSE_SLOT_VIEW_READ_THROUGH_RELEASED_OWNER, PGY_FIX_RECLAIM_SOURCE_OR_DROP_VIEW, slot_arg,
                    "Cannot read through ReadView '%s' because source slot '%s' was released",
                    sym->name, owner->name);
                return TYPE_UNKNOWN;
            }
            if (owner != NULL && owner->slot_info.is_secure)
                semantic_record_effect(ctx, EFFECT_SECURE);
        }
    }

    return slotops_normalize_type(slot_type->data.slot.inner_type);
}

bool
type_check_release_slot(ASTNode *call, SemanticContext *ctx)
{
    semantic_record_body_summary(ctx, BODY_SUMMARY_DROPS_RESOURCE);

    if (call->data.call.arg_count < 1) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            call,
            "Release requires at least 1 argument: Release(slot)");
        return false;
    }

    ASTNode *slot_arg = call->data.call.arguments[0];
    if (slot_arg == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            call,
            "Release requires a slot identifier argument");
        return false;
    }

    /* RemoteFuture has no Release ??it is consumed by await */
    if (slot_arg->type == AST_IDENTIFIER) {
        Symbol *rsym = scope_lookup(ctx->scope, slot_arg->data.identifier.name);
        if (rsym != NULL && rsym->type != NULL
            && type_is_constructed_named(rsym->type, "RemoteFuture")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_REMOTE_FUTURE_MISUSE, PGY_CAUSE_REMOTE_FUTURE_DIRECT_ACCESS, PGY_FIX_AWAIT_FUTURE, slot_arg,
                "RemoteFuture does not support Release(); it is consumed by "
                "'await' and returns Result<T>");
            return false;
        }
    }

    if (slot_arg->type != AST_IDENTIFIER) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, slot_arg,
            "Argument to Release must be a slot identifier");
        return false;
    }

    const char *slot_name = slot_arg->data.identifier.name;
    Symbol *sym = scope_lookup(ctx->scope, slot_name);

    if (sym == NULL || sym->kind != SYMBOL_SLOT) {
        if (sym != NULL && sym->type != NULL && sym->type->kind == TYPE_KIND_SLOT) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, slot_arg,
                "Only owning Slot<T>/SecureSlot<T> values can be released; views are non-owning and move tokens are transfer-only");
        } else {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_BUILTIN_SLOT_TYPE_REQUIRED, PGY_FIX_PASS_OWNING_SLOT,
                slot_arg,
                "'%s' is not a slot", slot_name);
        }
        return false;
    }

    if (sym->slot_info.is_secure)
        semantic_record_effect(ctx, EFFECT_SECURE);

    if (ctx->in_parallel && sym->slot_info.is_secure) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PARALLEL_SECURE_FORBIDDEN, PGY_CAUSE_PARALLEL_SECURE_IN_TASK, PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL, slot_arg,
            "Parallel context does not permit SecureSlot access yet; serialize authority-bearing slot reads/writes/releases outside the parallel block");
        return false;
    }

    if (sym->slot_info.state == SLOT_STATE_RELEASED) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_SLOT_DOUBLE_RELEASE, PGY_CAUSE_RELEASE_DOUBLE, PGY_FIX_REMOVE_REDUNDANT_RELEASE, slot_arg,
            "Slot '%s' has already been released", slot_name);
        return false;
    }

    const char *active_view_name = NULL;
    const char *active_view_kind = NULL;
    if (semantic_find_active_slot_view_for_source(ctx->scope, slot_name,
            &active_view_name, &active_view_kind, NULL)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_PIN_PARALLEL_CONFLICT,
            PGY_CAUSE_PIN_PARALLEL_CONFLICT,
            PGY_FIX_SERIALIZE_PIN_ACCESS,
            slot_arg,
            "Cannot release slot '%s' while %s '%s' is live.\n"
            "Reason:\n"
            "- pinned views are scoped capability leases over the source slot\n"
            "- releasing the owner while a view is live would invalidate cleanup and aliasing order\n"
            "Fix:\n"
            "- end the pin/view scope before Release(%s)\n"
            "- or move Release(%s) after the block that owns '%s'",
            slot_name,
            active_view_kind != NULL ? active_view_kind : "view",
            active_view_name != NULL ? active_view_name : "<view>",
            slot_name,
            slot_name,
            active_view_name != NULL ? active_view_name : "<view>");
        return false;
    }

    if (sym->slot_info.is_secure && call->data.call.arg_count < 2) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            call,
            "Release of SecureSlot '%s' requires a token argument",
            slot_name);
        return false;
    }
    if (sym->slot_info.is_secure
        && !builtin_validate_secure_token_arg(call->data.call.arguments[1], sym, sym->type, ctx)) {
        return false;
    }

    scope_release_slot(ctx->scope, slot_name);
    return true;
}
