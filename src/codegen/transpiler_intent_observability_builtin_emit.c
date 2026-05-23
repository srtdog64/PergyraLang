/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend intent observability builtin lowering.
 */

#include "transpiler_intent_observability_builtin_emit.h"

#include <stdlib.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "transpiler_format.h"

static const char *
intent_observability_zero_export(BuiltinKind bk)
{
    switch (bk) {
    case BUILTIN_INTENT_LAST_TRACE:
        return "pgy_intent_last_trace_export";
    case BUILTIN_INTENT_LAST_FAILURE:
        return "pgy_intent_last_failure_export";
    case BUILTIN_INTENT_LAST_NAME:
        return "pgy_intent_last_name_export";
    case BUILTIN_INTENT_LAST_HANDLE:
        return "pgy_intent_last_handle_export";
    case BUILTIN_INTENT_LAST_TRACE_ID:
        return "pgy_intent_last_trace_id_export";
    case BUILTIN_INTENT_LAST_STEP_COUNT:
        return "pgy_intent_last_step_count_export";
    case BUILTIN_INTENT_LAST_FAILED:
        return "pgy_intent_last_failed_export";
    case BUILTIN_INTENT_HISTORY_COUNT:
        return "pgy_intent_history_count_export";
    case BUILTIN_INTENT_ACTIVE_COUNT:
        return "pgy_intent_active_count_export";
    case BUILTIN_INTENT_CURRENT_HANDLE:
        return "pgy_intent_current_handle_export";
    case BUILTIN_INTENT_RECENT_COUNT:
        return "pgy_intent_recent_count_export";
    default:
        return NULL;
    }
}

static const char *
intent_observability_one_arg_export(BuiltinKind bk)
{
    switch (bk) {
    case BUILTIN_INTENT_HISTORY_STEP_NAME:
        return "pgy_intent_history_step_name_export";
    case BUILTIN_INTENT_HISTORY_STEP_ZONE:
        return "pgy_intent_history_step_zone_export";
    case BUILTIN_INTENT_HISTORY_STEP_PHASE:
        return "pgy_intent_history_step_phase_export";
    case BUILTIN_INTENT_HISTORY_STEP_PARTICIPANT:
        return "pgy_intent_history_step_participant_export";
    case BUILTIN_INTENT_HISTORY_STEP_SLOT:
        return "pgy_intent_history_step_slot_export";
    case BUILTIN_INTENT_HISTORY_STEP_FROM_ZONE:
        return "pgy_intent_history_step_from_zone_export";
    case BUILTIN_INTENT_HISTORY_STEP_FROM_SLOT:
        return "pgy_intent_history_step_from_slot_export";
    case BUILTIN_INTENT_HISTORY_STEP_TO_ZONE:
        return "pgy_intent_history_step_to_zone_export";
    case BUILTIN_INTENT_HISTORY_STEP_TO_SLOT:
        return "pgy_intent_history_step_to_slot_export";
    case BUILTIN_INTENT_HISTORY_STEP_OK:
        return "pgy_intent_history_step_ok_export";
    case BUILTIN_INTENT_HISTORY_STEP_FAILURE:
        return "pgy_intent_history_step_failure_export";
    case BUILTIN_INTENT_ACTIVE_NAME:
        return "pgy_intent_active_name_export";
    case BUILTIN_INTENT_ACTIVE_HANDLE:
        return "pgy_intent_active_handle_export";
    case BUILTIN_INTENT_ACTIVE_PARENT_HANDLE:
        return "pgy_intent_active_parent_handle_export";
    case BUILTIN_INTENT_ACTIVE_TRACE_ID:
        return "pgy_intent_active_trace_id_export";
    case BUILTIN_INTENT_ACTIVE_PRIORITY:
        return "pgy_intent_active_priority_export";
    case BUILTIN_INTENT_ACTIVE_SUBJECT_COUNT:
        return "pgy_intent_active_subject_count_export";
    case BUILTIN_INTENT_ACTIVE_STEP_COUNT:
        return "pgy_intent_active_step_count_export";
    case BUILTIN_INTENT_ACTIVE_CONCURRENT:
        return "pgy_intent_active_concurrent_export";
    case BUILTIN_INTENT_ACTIVE_FAILED:
        return "pgy_intent_active_failed_export";
    case BUILTIN_INTENT_ACTIVE_FAILURE:
        return "pgy_intent_active_failure_export";
    case BUILTIN_INTENT_ACTIVE_TRACE:
        return "pgy_intent_active_trace_export";
    case BUILTIN_INTENT_RECENT_HANDLE:
        return "pgy_intent_recent_handle_export";
    case BUILTIN_INTENT_RECENT_TRACE_ID:
        return "pgy_intent_recent_trace_id_export";
    case BUILTIN_INTENT_RECENT_NAME:
        return "pgy_intent_recent_name_export";
    case BUILTIN_INTENT_RECENT_TRACE:
        return "pgy_intent_recent_trace_export";
    case BUILTIN_INTENT_RECENT_FAILURE:
        return "pgy_intent_recent_failure_export";
    case BUILTIN_INTENT_RECENT_STEP_COUNT:
        return "pgy_intent_recent_step_count_export";
    case BUILTIN_INTENT_RECENT_FAILED:
        return "pgy_intent_recent_failed_export";
    default:
        return NULL;
    }
}

static const char *
intent_observability_two_arg_export(BuiltinKind bk)
{
    switch (bk) {
    case BUILTIN_INTENT_ACTIVE_STEP_NAME:
        return "pgy_intent_active_step_name_export";
    case BUILTIN_INTENT_ACTIVE_STEP_ZONE:
        return "pgy_intent_active_step_zone_export";
    case BUILTIN_INTENT_ACTIVE_STEP_PHASE:
        return "pgy_intent_active_step_phase_export";
    case BUILTIN_INTENT_ACTIVE_STEP_PARTICIPANT:
        return "pgy_intent_active_step_participant_export";
    case BUILTIN_INTENT_ACTIVE_STEP_SLOT:
        return "pgy_intent_active_step_slot_export";
    case BUILTIN_INTENT_ACTIVE_STEP_FROM_ZONE:
        return "pgy_intent_active_step_from_zone_export";
    case BUILTIN_INTENT_ACTIVE_STEP_FROM_SLOT:
        return "pgy_intent_active_step_from_slot_export";
    case BUILTIN_INTENT_ACTIVE_STEP_TO_ZONE:
        return "pgy_intent_active_step_to_zone_export";
    case BUILTIN_INTENT_ACTIVE_STEP_TO_SLOT:
        return "pgy_intent_active_step_to_slot_export";
    case BUILTIN_INTENT_ACTIVE_STEP_OK:
        return "pgy_intent_active_step_ok_export";
    case BUILTIN_INTENT_ACTIVE_STEP_FAILURE:
        return "pgy_intent_active_step_failure_export";
    default:
        return NULL;
    }
}

static bool
intent_observability_require_arg_count(ASTNode *call, TranspilerCtx *ctx,
                                       const char *export_name,
                                       size_t required)
{
    size_t actual = ast_call_arg_count(call);

    if (actual == required)
        return true;
    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "C backend: %s requires exactly %zu argument%s",
        export_name != NULL ? export_name : "intent observability builtin",
        required, required == 1 ? "" : "s");
    return false;
}

char *
emit_builtin_intent_observability(ASTNode *call, BuiltinKind bk,
                                  TranspilerCtx *ctx)
{
    const char *zero_export = intent_observability_zero_export(bk);
    const char *one_export = NULL;
    const char *two_export = NULL;

    if (ctx != NULL)
        ctx->uses_intent_observability = true;

    if (zero_export != NULL)
        return strdup_fmt("%s()", zero_export);

    one_export = intent_observability_one_arg_export(bk);
    if (one_export != NULL) {
        if (!intent_observability_require_arg_count(call, ctx, one_export, 1))
            return pergyra_strdup("0");
        char *index = emit_expression(ast_call_argument(call, 0), ctx);
        char *result = strdup_fmt("%s(%s)", one_export, index);
        free(index);
        return result;
    }

    two_export = intent_observability_two_arg_export(bk);
    if (two_export != NULL) {
        if (!intent_observability_require_arg_count(call, ctx, two_export, 2))
            return pergyra_strdup("0");
        char *intent_index = emit_expression(ast_call_argument(call, 0), ctx);
        char *step_index = emit_expression(ast_call_argument(call, 1), ctx);
        char *result = strdup_fmt("%s(%s, %s)", two_export, intent_index,
                                  step_index);
        free(intent_index);
        free(step_index);
        return result;
    }

    return NULL;
}
