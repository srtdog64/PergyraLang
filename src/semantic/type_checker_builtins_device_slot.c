#include <string.h>

#include "diag_codes.h"
#include "type_checker_internal.h"
#include "type_checker_builtins_slotops.h"

Type *
type_check_claim_device_slot(ASTNode *call, SemanticContext *ctx)
{
    if (!check_call_arity(call, 0, "ClaimDeviceSlot", ctx))
        return TYPE_UNKNOWN;
    semantic_record_effect(ctx, EFFECT_REMOTE);
    return wrap_constructed(TYPE_DEVICE_SLOT, TYPE_UNKNOWN);
}

Type *
type_check_device_handle_arg(ASTNode *expr, SemanticContext *ctx,
                             const char *builtin_name,
                             bool allow_released)
{
    Type *slot_type;
    Symbol *sym = NULL;

    if (expr == NULL)
        return TYPE_UNKNOWN;

    slot_type = type_check_expression(expr, ctx);
    if (!type_is_constructed_named(slot_type, "DeviceSlot")) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_BUILTIN_SLOT_TYPE_REQUIRED, PGY_FIX_PASS_DEVICE_SLOT,
            expr,
            "%s requires DeviceSlot<T>, got '%s'",
            builtin_name, slot_type->name);
        return TYPE_UNKNOWN;
    }

    if (expr->type == AST_IDENTIFIER) {
        const char *slot_name = ast_identifier_name(expr);
        sym = scope_lookup(ctx->scope, slot_name);
        if (!allow_released
            && sym != NULL
            && sym->slot_info.state == SLOT_STATE_RELEASED) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_SLOT_RELEASED,
                PGY_CAUSE_DEVICE_SLOT_USE_AFTER_RELEASE,
                PGY_FIX_RECLAIM_BEFORE_USE, expr,
                "Cannot use released DeviceSlot '%s' in %s",
                slot_name, builtin_name);
            return TYPE_UNKNOWN;
        }
    }

    if (ctx->in_parallel) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PARALLEL_SECURE_FORBIDDEN,
            PGY_CAUSE_PARALLEL_SECURE_IN_TASK,
            PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL, expr,
            "Parallel context does not permit DeviceSlot operations yet; keep device access serialized outside the parallel block");
        return TYPE_UNKNOWN;
    }

    semantic_record_effect(ctx, EFFECT_REMOTE);
    return slot_type;
}
