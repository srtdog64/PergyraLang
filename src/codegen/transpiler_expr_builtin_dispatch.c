#include "transpiler_expr_builtin_dispatch.h"

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_expr_core_builtins_emit.h"
#include "transpiler_expr_domain_query_builtin.h"
#include "transpiler_expr_io_builtin.h"
#include "transpiler_expr_projection_builtin.h"
#include "transpiler_intent_observability_builtin_emit.h"
#include "transpiler_slot_builtin_emit.h"
#include "../semantic/diag_codes.h"

char *
emit_call_builtin_dispatch(ASTNode *call,
                           BuiltinKind bk,
                           TranspilerCtx *ctx,
                           bool *handled)
{
    size_t argc = ast_call_arg_count(call);
    ASTNode *arg0 = ast_call_argument(call, 0);

    if (handled == NULL)
        return NULL;
    *handled = true;

    switch (bk) {
    case BUILTIN_CLAIM_SLOT:
    case BUILTIN_CLAIM_SECURE_SLOT:
        return emit_builtin_claim_slot(call, ctx);
    case BUILTIN_CLAIM_DEVICE_SLOT:
        return emit_builtin_claim_device_slot(call, ctx);
    case BUILTIN_VIEW_READ:
    case BUILTIN_VIEW_WRITE:
    case BUILTIN_MOVE:
        return emit_builtin_view(call, ctx);
    case BUILTIN_WRITE:
        return emit_builtin_write(call, ctx);
    case BUILTIN_READ:
        return emit_builtin_read(call, ctx);
    case BUILTIN_RELEASE:
        return emit_builtin_release(call, ctx);
    case BUILTIN_DEVICE_WRITE:
        return emit_builtin_device_write(call, ctx);
    case BUILTIN_DEVICE_READ:
        return emit_builtin_device_read(call, ctx);
    case BUILTIN_RELEASE_DEVICE_SLOT:
        return emit_builtin_release_device_slot(call, ctx);
    case BUILTIN_SUBMIT_DEVICE_READ:
        return emit_builtin_submit_device_read(call, ctx);
    case BUILTIN_LOG:
        return emit_builtin_log(call, ctx);
    case BUILTIN_LOG_RAW:
        return emit_builtin_log_raw(call, ctx);
    case BUILTIN_LOG_BANNER:
    case BUILTIN_LOG_BLOCK:
        return emit_builtin_log_banner(call, ctx);
    case BUILTIN_CLONE:
        if (argc >= 1 && arg0 != NULL)
            return emit_expression(arg0, ctx);
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ALIGN_ARG_TYPE,
            "C backend: Clone requires one value argument");
        return NULL;
    case BUILTIN_RC_NEW:
    case BUILTIN_RC_CLONE:
    case BUILTIN_RC_DROP:
    case BUILTIN_RC_DOWNGRADE:
    case BUILTIN_RC_GET:
    case BUILTIN_WEAK_UPGRADE:
    case BUILTIN_WEAK_DROP:
        return emit_builtin_rc(call, bk, ctx);
    case BUILTIN_BOX:
    case BUILTIN_BOX_GET:
    case BUILTIN_BOX_SET:
    case BUILTIN_BOX_DROP:
    case BUILTIN_BOX_IS_VALID:
    case BUILTIN_BOX_ARRAY:
        return emit_builtin_box(call, bk, ctx);
    case BUILTIN_TO_OBJECT:
    case BUILTIN_TO_TOBJECT:
        return emit_builtin_to_dto(call, ctx);
    case BUILTIN_HAS_PROJECTION:
    case BUILTIN_HAS_LAYER:
    case BUILTIN_HAS_STATE:
    case BUILTIN_HAS_ZONE:
    case BUILTIN_HAS_ZONE_PROJECTION:
    case BUILTIN_HAS_ZONE_LAYER:
    case BUILTIN_HAS_ZONE_STATE:
        return emit_builtin_domain_query(call, bk, ctx);
    case BUILTIN_ALLOCATOR_SYSTEM:
    case BUILTIN_ALLOCATOR_TRACING:
    case BUILTIN_ALLOCATOR_DEBUG:
    case BUILTIN_ALLOCATOR_DESTROY:
    case BUILTIN_ALLOCATOR_SCRATCH:
    case BUILTIN_ALLOCATOR_RESULT:
    case BUILTIN_ALLOCATOR_PERSISTENT:
    case BUILTIN_ALLOCATOR_POOL:
        return emit_builtin_allocator(call, bk, ctx);
    case BUILTIN_DIR_WALK:
    case BUILTIN_FILE_EXISTS:
    case BUILTIN_FILE_OPEN:
    case BUILTIN_FILE_READ:
    case BUILTIN_FILE_WRITE:
    case BUILTIN_FILE_CLOSE:
    case BUILTIN_READ_FILE:
    case BUILTIN_WRITE_FILE:
    case BUILTIN_INPUT:
    case BUILTIN_ARGS:
    case BUILTIN_PRINT:
    case BUILTIN_READ_LINE:
    case BUILTIN_NOW:
    case BUILTIN_SLEEP:
        return emit_builtin_io(call, bk, ctx);
    case BUILTIN_INTENT_LAST_TRACE:
    case BUILTIN_INTENT_LAST_FAILURE:
    case BUILTIN_INTENT_LAST_NAME:
    case BUILTIN_INTENT_LAST_HANDLE:
    case BUILTIN_INTENT_LAST_TRACE_ID:
    case BUILTIN_INTENT_LAST_STEP_COUNT:
    case BUILTIN_INTENT_LAST_FAILED:
    case BUILTIN_INTENT_HISTORY_COUNT:
    case BUILTIN_INTENT_HISTORY_STEP_NAME:
    case BUILTIN_INTENT_HISTORY_STEP_ZONE:
    case BUILTIN_INTENT_HISTORY_STEP_PHASE:
    case BUILTIN_INTENT_HISTORY_STEP_PARTICIPANT:
    case BUILTIN_INTENT_HISTORY_STEP_SLOT:
    case BUILTIN_INTENT_HISTORY_STEP_FROM_ZONE:
    case BUILTIN_INTENT_HISTORY_STEP_FROM_SLOT:
    case BUILTIN_INTENT_HISTORY_STEP_TO_ZONE:
    case BUILTIN_INTENT_HISTORY_STEP_TO_SLOT:
    case BUILTIN_INTENT_HISTORY_STEP_OK:
    case BUILTIN_INTENT_HISTORY_STEP_FAILURE:
    case BUILTIN_INTENT_ACTIVE_COUNT:
    case BUILTIN_INTENT_ACTIVE_NAME:
    case BUILTIN_INTENT_ACTIVE_HANDLE:
    case BUILTIN_INTENT_ACTIVE_PARENT_HANDLE:
    case BUILTIN_INTENT_ACTIVE_TRACE_ID:
    case BUILTIN_INTENT_ACTIVE_PRIORITY:
    case BUILTIN_INTENT_ACTIVE_SUBJECT_COUNT:
    case BUILTIN_INTENT_ACTIVE_STEP_COUNT:
    case BUILTIN_INTENT_ACTIVE_CONCURRENT:
    case BUILTIN_INTENT_ACTIVE_FAILED:
    case BUILTIN_INTENT_ACTIVE_FAILURE:
    case BUILTIN_INTENT_ACTIVE_TRACE:
    case BUILTIN_INTENT_ACTIVE_STEP_NAME:
    case BUILTIN_INTENT_ACTIVE_STEP_ZONE:
    case BUILTIN_INTENT_ACTIVE_STEP_PHASE:
    case BUILTIN_INTENT_ACTIVE_STEP_PARTICIPANT:
    case BUILTIN_INTENT_ACTIVE_STEP_SLOT:
    case BUILTIN_INTENT_ACTIVE_STEP_FROM_ZONE:
    case BUILTIN_INTENT_ACTIVE_STEP_FROM_SLOT:
    case BUILTIN_INTENT_ACTIVE_STEP_TO_ZONE:
    case BUILTIN_INTENT_ACTIVE_STEP_TO_SLOT:
    case BUILTIN_INTENT_ACTIVE_STEP_OK:
    case BUILTIN_INTENT_ACTIVE_STEP_FAILURE:
    case BUILTIN_INTENT_CURRENT_HANDLE:
    case BUILTIN_INTENT_RECENT_COUNT:
    case BUILTIN_INTENT_RECENT_HANDLE:
    case BUILTIN_INTENT_RECENT_TRACE_ID:
    case BUILTIN_INTENT_RECENT_NAME:
    case BUILTIN_INTENT_RECENT_TRACE:
    case BUILTIN_INTENT_RECENT_FAILURE:
    case BUILTIN_INTENT_RECENT_STEP_COUNT:
    case BUILTIN_INTENT_RECENT_FAILED:
        return emit_builtin_intent_observability(call, bk, ctx);
    case BUILTIN_PARALLEL:
        /* parallel { ... } is a statement, not an expression. */
        return pergyra_strdup("((void)0)");
    default:
        *handled = false;
        return NULL;
    }
}
