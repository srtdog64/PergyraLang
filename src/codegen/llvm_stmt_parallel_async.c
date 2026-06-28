#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "llvm_stmt_parallel_names.h"
#include "transpiler_type_mapping.h"
#include "../common/execution_lane_kind.h"
#include "../parser/ast_analysis.h"
#include "../parser/ast_api.h"

/* =================================================================
 * Parallel / async / select statement emission
 * ================================================================= */

static bool
llvm_capture_entry_is_required(LLVMGenCtx *ctx,
                               const ASTNode *body,
                               LLVMScopeFrame *frame,
                               int index)
{
    if (ctx == NULL || body == NULL || frame == NULL || index < 0
        || index >= frame->count || frame->entries[index].name == NULL) {
        return false;
    }
    if (!ast_contains_free_identifier_ref(body, frame->entries[index].name)) {
        return false;
    }

    return llvm_scope_frame_entry_is_current(ctx, frame, index);
}

static const char *
llvm_capture_shared_collection_kind(LLVMGenCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;
    if (llvm_lookup_array_var(ctx, name) != NULL)
        return codegen_worker_boundary_storage_kind_from_constructor_name(
            "Array/Slice", false, true);
    if (llvm_lookup_list_inner(ctx, name) != NULL)
        return codegen_worker_boundary_storage_kind_from_constructor_name(
            "List", false, false);
    if (llvm_lookup_queue_inner(ctx, name) != NULL)
        return codegen_worker_boundary_storage_kind_from_constructor_name(
            "Queue", false, false);
    if (llvm_lookup_set_inner(ctx, name) != NULL)
        return codegen_worker_boundary_storage_kind_from_constructor_name(
            "Set", false, false);
    if (llvm_lookup_map_value(ctx, name) != NULL)
        return codegen_worker_boundary_storage_kind_from_constructor_name(
            "HashMap", false, false);
    return NULL;
}

static bool
llvm_capture_reject_shared_collection(LLVMGenCtx *ctx, ASTNode *site,
                                      const char *boundary,
                                      const char *name)
{
    const char *kind = llvm_capture_shared_collection_kind(ctx, name);

    if (kind == NULL)
        return false;
    llvm_set_error_at_with_hints(ctx, site,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_INSPECT_MIR_INVENTORY,
        "LLVM %s capture '%s' cannot share mutable collection '%s' by pointer; use a channel/result boundary or copy before spawning",
        boundary != NULL ? boundary : "worker",
        name != NULL ? name : "<binding>",
        kind);
    return true;
}

static bool
llvm_emit_task_handle_nonnull_guard(LLVMGenCtx *ctx, ASTNode *site,
                                    LLVMValueRef handle, const char *reason)
{
    LLVMFuncEntry *panic_fn;
    LLVMValueRef task;
    LLVMValueRef is_null;
    LLVMBasicBlockRef fail_bb;
    LLVMBasicBlockRef cont_bb;

    if (ctx == NULL || handle == NULL)
        return false;
    if (ctx->current_function == NULL) {
        llvm_set_error_at_with_hints(ctx, site,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM task handle guard requires an active function");
        return false;
    }

    panic_fn = llvm_lookup_function(ctx,
        "pgy_runtime_panic_internal_invariant_export");
    if (panic_fn == NULL) {
        llvm_set_error_at_with_hints(ctx, site,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM task handle guard requires registered runtime function '%s'",
            "pgy_runtime_panic_internal_invariant_export");
        return false;
    }

    task = LLVMBuildExtractValue(ctx->builder, handle, 0, llvm_tmp_name(ctx));
    is_null = LLVMBuildICmp(ctx->builder, LLVMIntEQ, task,
        LLVMConstNull(ctx->type_i8ptr), llvm_tmp_name(ctx));
    fail_bb = LLVMAppendBasicBlockInContext(ctx->context,
        ctx->current_function, "pgy.task.spawn.fail");
    cont_bb = LLVMAppendBasicBlockInContext(ctx->context,
        ctx->current_function, "pgy.task.spawn.cont");
    LLVMBuildCondBr(ctx->builder, is_null, fail_bb, cont_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, fail_bb);
    LLVMValueRef reason_arg = LLVMBuildGlobalStringPtr(ctx->builder,
        reason != NULL ? reason : "task spawn failed", llvm_tmp_name(ctx));
    LLVMBuildCall2(ctx->builder, panic_fn->fn_type, panic_fn->fn,
        &reason_arg, 1, "");
    LLVMBuildUnreachable(ctx->builder);

    LLVMPositionBuilderAtEnd(ctx->builder, cont_bb);
    return true;
}

void
llvm_emit_parallel_block(ASTNode *node, LLVMGenCtx *ctx)
{
    size_t count = ast_parallel_task_count(node);
    if (count == 0)
        return;

    LLVMFuncEntry *spawn_fn = llvm_lookup_function(ctx,
        "pgy_lane_spawn_dispatch_export");
    LLVMFuncEntry *await_fn = llvm_lookup_function(ctx, "pgy_await_export");
    if (spawn_fn == NULL || await_fn == NULL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM parallel block requires registered runtime functions 'pgy_lane_spawn_dispatch_export' and 'pgy_await_export'; sequential fallback is disabled");
        return;
    }

    typedef struct {
        const char *name;
        LLVMValueRef alloca;
        LLVMTypeRef type;
        const char *channel_inner;
        const char *future_inner;
        const char *slot_inner;
        bool future_is_remote;
        bool slot_is_secure;
    } CapturedVar;
    CapturedVar *captured = NULL;
    size_t capture_count = 0;
    size_t n_captured = 0;

    for (int i = 0; i < ctx->scope_depth; i++) {
        LLVMScopeFrame *frame = &ctx->scopes[i];
        for (int j = 0; j < frame->count; j++) {
            if (!llvm_capture_entry_is_required(ctx, node, frame, j))
                continue;
            if (capture_count == SIZE_MAX) {
                llvm_set_error(ctx,
                    "LLVM parallel capture registry capacity overflow");
                return;
            }
            capture_count++;
        }
    }
    if (capture_count > UINT_MAX) {
        llvm_set_error(ctx,
            "LLVM parallel capture registry exceeds LLVM struct field limit");
        return;
    }
    if (capture_count > SIZE_MAX / sizeof(*captured)) {
        llvm_set_error(ctx,
            "LLVM parallel capture registry allocation overflow");
        return;
    }
    if (capture_count > 0) {
        captured = pgy_arena_calloc(&ctx->scratch,
            capture_count * sizeof(*captured));
        if (captured == NULL) {
            llvm_set_error(ctx,
                "out of memory allocating LLVM parallel capture registry");
            return;
        }
    }

    for (int i = 0; i < ctx->scope_depth; i++) {
        LLVMScopeFrame *frame = &ctx->scopes[i];
        for (int j = 0; j < frame->count; j++) {
            if (!llvm_capture_entry_is_required(ctx, node, frame, j))
                continue;
            if (frame->entries[j].alloca == NULL) {
                llvm_set_error_at_with_hints(ctx, node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "LLVM parallel capture requires storage-backed binding '%s'; type-only scope entries cannot cross a wrapper boundary",
                    frame->entries[j].name != NULL
                        ? frame->entries[j].name
                        : "<binding>");
                return;
            }
            if (llvm_capture_reject_shared_collection(ctx, node, "parallel",
                    frame->entries[j].name)) {
                return;
            }
            captured[n_captured++] = (CapturedVar){
                frame->entries[j].name,
                frame->entries[j].alloca,
                frame->entries[j].type,
                llvm_lookup_channel_inner(ctx, frame->entries[j].name),
                llvm_lookup_future_inner(ctx, frame->entries[j].name),
                llvm_lookup_slot_inner(ctx, frame->entries[j].name),
                llvm_lookup_future_is_remote(ctx, frame->entries[j].name),
                llvm_lookup_slot_is_secure(ctx, frame->entries[j].name)
            };
        }
    }

    LLVMTypeRef *ctx_fields = NULL;
    if (n_captured > 0) {
        if (n_captured > SIZE_MAX / sizeof(*ctx_fields)) {
            llvm_set_error(ctx,
                "LLVM parallel capture field allocation overflow");
            return;
        }
        ctx_fields = pgy_arena_calloc(&ctx->scratch,
            n_captured * sizeof(*ctx_fields));
        if (ctx_fields == NULL) {
            llvm_set_error(ctx,
                "out of memory allocating LLVM parallel capture field types");
            return;
        }
    }
    for (size_t i = 0; i < n_captured; i++)
        ctx_fields[i] = ctx->type_i8ptr;

    char ctx_name[64];
    if (!llvm_parallel_counter_name(ctx, ctx_name, sizeof(ctx_name),
            "_pgy_par_ctx_", ctx->parallel_counter))
        return;
    LLVMTypeRef ctx_struct_type = LLVMStructCreateNamed(ctx->context, ctx_name);
    LLVMStructSetBody(ctx_struct_type, ctx_fields, (unsigned)n_captured, 0);

    LLVMValueRef ctx_alloca = LLVMBuildAlloca(ctx->builder, ctx_struct_type,
                                               "_pctx");
    for (size_t i = 0; i < n_captured; i++) {
        LLVMValueRef gep = LLVMBuildStructGEP2(ctx->builder, ctx_struct_type,
                                                 ctx_alloca, (unsigned)i,
                                                 llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, captured[i].alloca, gep);
    }

    LLVMValueRef ctx_i8ptr = LLVMBuildBitCast(ctx->builder, ctx_alloca,
                                               ctx->type_i8ptr,
                                               llvm_tmp_name(ctx));

    LLVMValueRef saved_fn = ctx->current_function;
    LLVMTypeRef saved_ret = ctx->current_ret_type;
    LLVMTypeRef saved_function_ret = ctx->current_function_ret_type;
    const char *saved_return_type_name = ctx->current_return_type_name;
    ASTNode *saved_return_callable_type = ctx->current_return_callable_type;
    LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(ctx->builder);

    LLVMTypeRef wrapper_params[] = { ctx->type_i8ptr };
    LLVMTypeRef wrapper_type = LLVMFunctionType(ctx->type_i8ptr,
                                                 wrapper_params, 1, 0);

    LLVMValueRef *wrapper_fns;
    if (count > SIZE_MAX / sizeof(*wrapper_fns)) {
        llvm_set_error(ctx,
            "LLVM parallel wrapper registry allocation overflow");
        return;
    }
    wrapper_fns = pgy_arena_calloc(&ctx->scratch,
        count * sizeof(*wrapper_fns));
    if (wrapper_fns == NULL) {
        llvm_set_error(ctx,
            "out of memory allocating LLVM parallel wrapper registry");
        return;
    }

    for (size_t i = 0; i < count; i++) {
        char fn_name[64];
        if (!llvm_parallel_task_name(ctx, fn_name, sizeof(fn_name),
                ctx->parallel_counter, i))
            return;

        LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, wrapper_type);
        wrapper_fns[i] = fn;

        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(
            ctx->context, fn, "entry");
        LLVMPositionBuilderAtEnd(ctx->builder, entry);

        ctx->current_function = fn;
        ctx->current_ret_type = ctx->type_i8ptr;
        ctx->current_function_ret_type = ctx->type_i8ptr;
        ctx->current_return_type_name = NULL;
        ctx->current_return_callable_type = NULL;

        LLVMLexicalRegistrySnapshot lexical_snapshot =
            llvm_lexical_registry_snapshot(ctx);
        llvm_scope_push(ctx);

        LLVMValueRef arg0 = LLVMGetParam(fn, 0);
        LLVMValueRef ctx_ptr = LLVMBuildBitCast(ctx->builder, arg0,
            LLVMPointerType(ctx_struct_type, 0), "_pctx");

        for (size_t c = 0; c < n_captured; c++) {
            LLVMValueRef field_ptr = LLVMBuildStructGEP2(
                ctx->builder, ctx_struct_type, ctx_ptr, (unsigned)c,
                llvm_tmp_name(ctx));
            LLVMValueRef var_ptr = LLVMBuildLoad2(
                ctx->builder, ctx->type_i8ptr, field_ptr,
                llvm_tmp_name(ctx));
            llvm_scope_declare(ctx, captured[c].name, var_ptr, captured[c].type);
            if (captured[c].channel_inner != NULL)
                llvm_register_channel_var_binding(ctx, captured[c].name,
                    var_ptr, captured[c].channel_inner);
            if (captured[c].future_inner != NULL)
                llvm_register_future_var_binding(ctx, captured[c].name,
                    var_ptr, captured[c].future_inner,
                    captured[c].future_is_remote);
            if (captured[c].slot_inner != NULL)
                llvm_register_slot_var_binding(ctx, captured[c].name,
                    var_ptr, captured[c].slot_inner,
                    captured[c].slot_is_secure);
        }

        llvm_emit_statement(ast_parallel_task(node, i), ctx);

        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))
                == NULL)
            LLVMBuildRet(ctx->builder, LLVMConstNull(ctx->type_i8ptr));

        llvm_scope_pop(ctx);
        llvm_lexical_registry_restore(ctx, lexical_snapshot);
    }

    ctx->parallel_counter++;

    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret;
    ctx->current_function_ret_type = saved_function_ret;
    ctx->current_return_type_name = saved_return_type_name;
    ctx->current_return_callable_type = saved_return_callable_type;
    if (saved_bb != NULL)
        LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);

    LLVMValueRef *handles;
    if (count > SIZE_MAX / sizeof(*handles)) {
        llvm_set_error(ctx,
            "LLVM parallel handle registry allocation overflow");
        return;
    }
    handles = pgy_arena_calloc(&ctx->scratch,
        count * sizeof(*handles));
    if (handles == NULL) {
        llvm_set_error(ctx,
            "out of memory allocating LLVM parallel handle registry");
        return;
    }
    for (size_t i = 0; i < count; i++) {
        LLVMValueRef fn_ptr = LLVMBuildBitCast(
            ctx->builder, wrapper_fns[i], ctx->type_i8ptr,
            llvm_tmp_name(ctx));

        LLVMValueRef args[] = {
            LLVMConstInt(ctx->type_i32, PGY_LANE_WORKER_POOL, 0),
            fn_ptr,
            ctx_i8ptr
        };
        handles[i] = LLVMBuildCall2(ctx->builder, spawn_fn->fn_type,
                                     spawn_fn->fn, args, 3,
                                     llvm_tmp_name(ctx));
        if (!llvm_emit_task_handle_nonnull_guard(ctx, node, handles[i],
                "LLVM parallel task spawn failed")) {
            return;
        }
    }

    for (size_t i = 0; i < count; i++) {
        LLVMValueRef args[] = { handles[i] };
        LLVMBuildCall2(ctx->builder, await_fn->fn_type,
                       await_fn->fn, args, 1, "");
    }
}

void
llvm_emit_async_block(ASTNode *node, LLVMGenCtx *ctx)
{
    size_t statement_count = 0;
    ASTNode **statements = ast_async_block_statements(node, &statement_count);
    LLVMFuncEntry *spawn_fn = llvm_lookup_function(ctx,
        "pgy_lane_spawn_dispatch_export");
    LLVMFuncEntry *detach_fn = llvm_lookup_function(ctx, "pgy_async_detach_export");
    if (spawn_fn == NULL || detach_fn == NULL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM async block requires registered runtime functions 'pgy_lane_spawn_dispatch_export' and 'pgy_async_detach_export'; synchronous fallback is disabled");
        return;
    }

    LLVMValueRef saved_fn = ctx->current_function;
    LLVMTypeRef saved_ret = ctx->current_ret_type;
    LLVMTypeRef saved_function_ret = ctx->current_function_ret_type;
    const char *saved_return_type_name = ctx->current_return_type_name;
    ASTNode *saved_return_callable_type = ctx->current_return_callable_type;
    LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(ctx->builder);

    for (int i = 0; i < ctx->scope_depth; i++) {
        LLVMScopeFrame *frame = &ctx->scopes[i];
        for (int j = 0; j < frame->count; j++) {
            const char *name;
            const char *channel_inner;
            const char *slot_inner;
            if (!llvm_capture_entry_is_required(ctx, node, frame, j))
                continue;
            name = frame->entries[j].name;
            channel_inner = llvm_lookup_channel_inner(ctx, name);
            slot_inner = llvm_lookup_slot_inner(ctx, name);
            if (frame->entries[j].alloca == NULL) {
                llvm_set_error_at_with_hints(ctx, node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "LLVM async capture requires storage-backed binding '%s'; type-only scope entries cannot cross a wrapper boundary",
                    frame->entries[j].name != NULL
                        ? frame->entries[j].name
                        : "<binding>");
                return;
            }
            if (slot_inner != NULL) {
                llvm_set_error_at_with_hints(ctx, node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "LLVM async block cannot capture Slot<T> local '%s' by pointer; use a named task boundary or explicit handoff",
                    name != NULL ? name : "<binding>");
                return;
            }
            if (channel_inner != NULL) {
                llvm_set_error_at_with_hints(ctx, node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "LLVM async block cannot capture Channel<T> local '%s' by pointer; use parallel or a named task boundary with explicit handoff",
                    name != NULL ? name : "<binding>");
                return;
            }
            if (llvm_capture_reject_shared_collection(ctx, node, "async",
                    name)) {
                return;
            }
            if (channel_inner == NULL) {
                llvm_set_error_at_with_hints(ctx, node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "LLVM async block cannot capture non-Channel local '%s' by pointer; use a named task boundary or explicit value handoff",
                    name != NULL ? name : "<binding>");
                return;
            }
        }
    }
    LLVMValueRef ctx_i8ptr = LLVMConstNull(ctx->type_i8ptr);

    LLVMTypeRef wrapper_params[] = { ctx->type_i8ptr };
    LLVMTypeRef wrapper_type = LLVMFunctionType(ctx->type_i8ptr, wrapper_params, 1, 0);
    char fn_name[64];
    if (!llvm_async_wrapper_name(ctx, fn_name, sizeof(fn_name),
            ctx->parallel_counter))
        return;
    ctx->parallel_counter++;
    LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, wrapper_type);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx->context, fn, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, entry);
    ctx->current_function = fn;
    ctx->current_ret_type = ctx->type_i8ptr;
    ctx->current_function_ret_type = ctx->type_i8ptr;
    ctx->current_return_type_name = NULL;
    ctx->current_return_callable_type = NULL;
    LLVMLexicalRegistrySnapshot lexical_snapshot =
        llvm_lexical_registry_snapshot(ctx);
    llvm_scope_push(ctx);
    (void)LLVMGetParam(fn, 0);
    for (size_t i = 0; i < statement_count; i++)
        llvm_emit_statement(statements[i], ctx);
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildRet(ctx->builder, LLVMConstNull(ctx->type_i8ptr));
    llvm_scope_pop(ctx);
    llvm_lexical_registry_restore(ctx, lexical_snapshot);

    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret;
    ctx->current_function_ret_type = saved_function_ret;
    ctx->current_return_type_name = saved_return_type_name;
    ctx->current_return_callable_type = saved_return_callable_type;
    if (saved_bb != NULL)
        LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);

    LLVMValueRef fn_ptr = LLVMBuildBitCast(ctx->builder, fn, ctx->type_i8ptr, llvm_tmp_name(ctx));
    LLVMValueRef spawn_args[] = {
        LLVMConstInt(ctx->type_i32, PGY_LANE_LOCAL_ASYNC, 0),
        fn_ptr,
        ctx_i8ptr
    };
    LLVMValueRef handle = LLVMBuildCall2(ctx->builder, spawn_fn->fn_type, spawn_fn->fn,
        spawn_args, 3, llvm_tmp_name(ctx));
    if (!llvm_emit_task_handle_nonnull_guard(ctx, node, handle,
            "LLVM async block spawn failed")) {
        return;
    }
    LLVMValueRef detach_args[] = { handle };
    LLVMBuildCall2(ctx->builder, detach_fn->fn_type, detach_fn->fn, detach_args, 1, "");
}

#endif /* PGY_LLVM_ENABLED */
