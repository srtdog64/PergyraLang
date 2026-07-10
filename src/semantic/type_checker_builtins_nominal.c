#include <string.h>

#include "diag_codes.h"
#include "type_checker_internal.h"
#include "type_checker_builtins_internal.h"
#include "runtime/pgy_runtime_capability.h"

Type *
type_check_builtin_call(ASTNode *call, BuiltinKind kind, SemanticContext *ctx)
{
    bool intent_obs_handled = false;
    Type *intent_obs_type = type_check_intent_observability_builtin(
        call, ctx, &intent_obs_handled);
    if (intent_obs_handled)
        return intent_obs_type;

    bool nominal_owner_handled = false;
    Type *nominal_owner_type = type_check_nominal_ownership_builtin(
        call, kind, ctx, &nominal_owner_handled);
    if (nominal_owner_handled)
        return nominal_owner_type;

    switch (kind) {
    case BUILTIN_CLAIM_SLOT:
        return type_check_claim_slot(call, ctx);
    case BUILTIN_CLAIM_SECURE_SLOT:
        semantic_record_effect(ctx, EFFECT_SECURE);
        return type_check_claim_slot(call, ctx);
    case BUILTIN_CLAIM_DEVICE_SLOT:
        return type_check_claim_device_slot(call, ctx);
    case BUILTIN_VIEW_READ:
        return type_check_view_read(call, ctx);
    case BUILTIN_VIEW_WRITE:
        return type_check_view_write(call, ctx);
    case BUILTIN_MOVE:
        return type_check_move_token(call, ctx);
    case BUILTIN_WRITE:
        type_check_write_slot(call, ctx);
        return TYPE_VOID;
    case BUILTIN_READ:
        return type_check_read_slot(call, ctx);
    case BUILTIN_RELEASE:
        type_check_release_slot(call, ctx);
        return TYPE_VOID;
    case BUILTIN_SLOT_RAW_POINTER:
        for (size_t i = 0; i < ast_call_arg_count(call); i++)
            type_check_expression(ast_call_argument(call, i), ctx);
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_RAW_ESCAPE_UNSTABLE,
            PGY_CAUSE_RAW_ESCAPE_UNSTABLE,
            PGY_FIX_USE_PIN_OR_WAIT_FOR_RAW_ESCAPE_CONTRACT,
            call,
            "SlotRawPointer is not beta-stable. Reason: unsafe { } is only a lexical escape marker today, not a scoped unsafe(raw) capability or system-tier raw pointer contract. Fix: use typed Pin/Lease views for hot-path slot access, or wait for the scoped raw escape contract with ABI lowering and diagnostics.");
        return TYPE_UNKNOWN;
    case BUILTIN_DEVICE_WRITE:
    case BUILTIN_DEVICE_READ:
    case BUILTIN_RELEASE_DEVICE_SLOT:
    case BUILTIN_SUBMIT_DEVICE_READ:
        return type_check_stdlib_call(call, ast_identifier_name(ast_call_callee(call)), ctx);
    case BUILTIN_ARGS: {
        Type *args[1] = { TYPE_STRING };
        if (!check_call_arity(call, 0, "Args", ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_NONDETERMINISTIC);
        semantic_record_capability(ctx, PGY_CAP_ENV);
        return type_create_constructed(TYPE_ARRAY, args, 1);
    }
    case BUILTIN_LOG:
        for (size_t i = 0; i < ast_call_arg_count(call); i++)
            type_check_expression(ast_call_argument(call, i), ctx);
        return TYPE_VOID;
    case BUILTIN_LOG_RAW:
        if (!check_call_arity(call, 1, "LogRaw", ctx))
            return TYPE_VOID;
        require_assignable(type_check_expression(ast_call_argument(call, 0), ctx),
                          TYPE_STRING, ast_call_argument(call, 0), ctx);
        return TYPE_VOID;
    case BUILTIN_LOG_BANNER:
        if (!check_call_arity(call, 1, "LogBanner", ctx))
            return TYPE_VOID;
        require_assignable(type_check_expression(ast_call_argument(call, 0), ctx),
                          TYPE_STRING, ast_call_argument(call, 0), ctx);
        return TYPE_VOID;
    case BUILTIN_LOG_BLOCK:
        if (!check_call_arity(call, 1, "LogBlock", ctx))
            return TYPE_VOID;
        require_assignable(type_check_expression(ast_call_argument(call, 0), ctx),
                          TYPE_STRING, ast_call_argument(call, 0), ctx);
        return TYPE_VOID;
    case BUILTIN_CLONE:
        return type_check_stdlib_call(call, "Clone", ctx);
    case BUILTIN_TO_OBJECT:
        return type_check_to_object(call, ctx);
        case BUILTIN_TO_TOBJECT:
            return type_check_to_tobject(call, ctx);
    case BUILTIN_HAS_PROJECTION:
        return type_check_has_projection(call, ctx);
    case BUILTIN_HAS_LAYER:
        return type_check_has_layer(call, ctx);
    case BUILTIN_HAS_STATE:
        return type_check_has_state(call, ctx);
    case BUILTIN_HAS_ZONE:
        return type_check_has_zone(call, ctx);
    case BUILTIN_HAS_ZONE_PROJECTION:
        return type_check_has_zone_projection_builtin(call, ctx);
    case BUILTIN_HAS_ZONE_LAYER:
        return type_check_has_zone_layer_builtin(call, ctx);
    case BUILTIN_HAS_ZONE_STATE:
        return type_check_has_zone_state_builtin(call, ctx);
    case BUILTIN_PARALLEL:
        return TYPE_VOID;
    case BUILTIN_DIR_WALK:
        if (check_call_arity(call, 1, "DirWalk", ctx)) {
            require_assignable(type_check_expression(ast_call_argument(call, 0), ctx),
                TYPE_STRING, ast_call_argument(call, 0), ctx);
        }
        {
            Type *args[1] = { TYPE_STRING };
            semantic_record_effect(ctx, EFFECT_IO);
            semantic_record_effect(ctx, EFFECT_NONDETERMINISTIC);
            semantic_record_capability(ctx, PGY_CAP_IO_READ);
            return type_create_constructed(TYPE_ARRAY, args, 1);
        }
    case BUILTIN_FILE_EXISTS:
        if (check_call_arity(call, 1, "FileExists", ctx)) {
            require_assignable(type_check_expression(ast_call_argument(call, 0), ctx),
                TYPE_STRING, ast_call_argument(call, 0), ctx);
        }
        semantic_record_effect(ctx, EFFECT_IO);
        return TYPE_BOOL;
    case BUILTIN_FILE_OPEN:
        if (check_call_arity(call, 2, "FileOpen", ctx)) {
            require_assignable(type_check_expression(ast_call_argument(call, 0), ctx),
                TYPE_STRING, ast_call_argument(call, 0), ctx);
            require_assignable(type_check_expression(ast_call_argument(call, 1), ctx),
                TYPE_STRING, ast_call_argument(call, 1), ctx);
        }
        semantic_record_effect(ctx, EFFECT_IO);
        return TYPE_INT;
    case BUILTIN_FILE_READ:
        if (check_call_arity(call, 1, "FileRead", ctx)) {
            require_assignable(type_check_expression(ast_call_argument(call, 0), ctx),
                TYPE_INT, ast_call_argument(call, 0), ctx);
        }
        semantic_record_effect(ctx, EFFECT_IO);
        return TYPE_STRING;
    case BUILTIN_FILE_WRITE:
        if (check_call_arity(call, 2, "FileWrite", ctx)) {
            require_assignable(type_check_expression(ast_call_argument(call, 0), ctx),
                TYPE_INT, ast_call_argument(call, 0), ctx);
            require_assignable(type_check_expression(ast_call_argument(call, 1), ctx),
                TYPE_STRING, ast_call_argument(call, 1), ctx);
        }
        semantic_record_effect(ctx, EFFECT_IO);
        return TYPE_VOID;
    case BUILTIN_FILE_CLOSE:
        if (check_call_arity(call, 1, "FileClose", ctx)) {
            require_assignable(type_check_expression(ast_call_argument(call, 0), ctx),
                TYPE_INT, ast_call_argument(call, 0), ctx);
        }
        semantic_record_effect(ctx, EFFECT_IO);
        return TYPE_VOID;
    case BUILTIN_READ_FILE:
        if (check_call_arity(call, 1, "ReadFile", ctx)) {
            require_assignable(type_check_expression(ast_call_argument(call, 0), ctx),
                TYPE_STRING, ast_call_argument(call, 0), ctx);
        }
        semantic_record_effect(ctx, EFFECT_IO);
        semantic_record_capability(ctx, PGY_CAP_IO_READ);
        return TYPE_STRING;
    case BUILTIN_READ_STDIN:
        if (check_call_arity(call, 1, "ReadStdin", ctx)) {
            require_assignable(type_check_expression(ast_call_argument(call, 0), ctx),
                TYPE_INT, ast_call_argument(call, 0), ctx);
        }
        semantic_record_effect(ctx, EFFECT_IO);
        semantic_record_effect(ctx, EFFECT_NONDETERMINISTIC);
        semantic_record_capability(ctx, PGY_CAP_IO_READ);
        return TYPE_STRING;
    case BUILTIN_WRITE_FILE:
        if (check_call_arity(call, 2, "WriteFile", ctx)) {
            require_assignable(type_check_expression(ast_call_argument(call, 0), ctx),
                TYPE_STRING, ast_call_argument(call, 0), ctx);
            require_assignable(type_check_expression(ast_call_argument(call, 1), ctx),
                TYPE_STRING, ast_call_argument(call, 1), ctx);
        }
        semantic_record_effect(ctx, EFFECT_IO);
        semantic_record_capability(ctx, PGY_CAP_IO_WRITE);
        return TYPE_VOID;
    case BUILTIN_INPUT:
        semantic_record_effect(ctx, EFFECT_NONDETERMINISTIC);
        semantic_record_capability(ctx, PGY_CAP_IO_READ);
        if (ast_call_arg_count(call) > 1) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call,
                "'Input' expects at most 1 argument, got %llu",
                (unsigned long long) ast_call_arg_count(call));
            return TYPE_STRING;
        }
        if (ast_call_arg_count(call) == 1) {
            require_assignable(type_check_expression(ast_call_argument(call, 0), ctx),
                TYPE_STRING, ast_call_argument(call, 0), ctx);
        }
        return TYPE_STRING;
    case BUILTIN_PRINT:
        return type_check_stdlib_call(call, "Print", ctx);
    case BUILTIN_READ_LINE:
        return type_check_stdlib_call(call, "ReadLine", ctx);
    case BUILTIN_NOW:
        return type_check_stdlib_call(call, "Now", ctx);
    case BUILTIN_SLEEP:
        return type_check_stdlib_call(call, "Sleep", ctx);
    default:
        return TYPE_UNKNOWN;
    }
}
