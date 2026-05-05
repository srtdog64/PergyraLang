/* LLVM call emitter split into sub-1000 LOC include chunks.
 * Keep this shim for the existing llvm_expr include order. */
static LLVMTypeRef
llvm_collection_required_value_type(LLVMGenCtx *ctx, ASTNode *node,
                                    const char *collection_kind,
                                    const char *var_name,
                                    const char *type_name,
                                    LLVMValueRef *out)
{
    if (type_name == NULL || type_name[0] == '\0') {
        if (ctx != NULL && !ctx->has_error) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM %s operation requires concrete element/value type metadata for '%s'",
                collection_kind != NULL ? collection_kind : "collection",
                var_name != NULL ? var_name : "<collection>");
        }
        if (out != NULL && ctx != NULL)
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
        return NULL;
    }
    return pergyra_type_to_llvm(ctx, type_name);
}

static LLVMFuncEntry *
llvm_required_collection_function(LLVMGenCtx *ctx,
                                  ASTNode *node,
                                  const char *callee_name,
                                  const char *function_name)
{
    LLVMFuncEntry *fn = function_name != NULL
        ? llvm_lookup_function(ctx, function_name)
        : NULL;

    if (fn == NULL && ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM collection operation '%s' requires registered runtime function '%s'",
            callee_name != NULL ? callee_name : "collection operation",
            function_name != NULL ? function_name : "<missing>");
    }
    return fn;
}

#include "llvm_expr_constructor_calls.h"
#include "llvm_expr_array_calls.h"
#include "llvm_expr_collection_base_calls.h"
#include "llvm_expr_domain_query_calls.h"
#include "llvm_expr_event_calls.h"
#include "llvm_expr_intent_observability_calls.h"
#include "llvm_expr_log_calls.h"
#include "llvm_expr_math_calls.h"
#include "llvm_expr_rc_calls.h"
#include "llvm_expr_result_option_calls.h"
#include "llvm_expr_slot_device_calls.h"
#include "llvm_expr_stdlib_scalar_io_calls.h"
#include "llvm_expr_task_channel_calls.h"
#include "llvm_expr_call_collections_extended.h"
#include "llvm_expr_call_dispatch.h"
