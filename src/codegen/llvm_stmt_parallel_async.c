#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"

/* =================================================================
 * Parallel / async / select statement emission
 * ================================================================= */

void
llvm_emit_parallel_block(ASTNode *node, LLVMGenCtx *ctx)
{
    size_t count = node->data.parallel.task_count;
    if (count == 0)
        return;

    typedef struct { const char *name; LLVMValueRef alloca; LLVMTypeRef type; } CapturedVar;
    CapturedVar captured[MAX_SCOPE_VARS];
    int n_captured = 0;

    for (int i = 0; i < ctx->scope_depth; i++) {
        LLVMScopeFrame *frame = &ctx->scopes[i];
        for (int j = 0; j < frame->count && n_captured < MAX_SCOPE_VARS; j++) {
            captured[n_captured++] = (CapturedVar){
                frame->entries[j].name,
                frame->entries[j].alloca,
                frame->entries[j].type
            };
        }
    }

    LLVMTypeRef *ctx_fields = pgy_arena_calloc(&ctx->scratch,
        (size_t)n_captured * sizeof(LLVMTypeRef));
    for (int i = 0; i < n_captured; i++)
        ctx_fields[i] = ctx->type_i8ptr;

    char ctx_name[64];
    snprintf(ctx_name, sizeof(ctx_name), "_pgy_par_ctx_%d", ctx->parallel_counter);
    LLVMTypeRef ctx_struct_type = LLVMStructCreateNamed(ctx->context, ctx_name);
    LLVMStructSetBody(ctx_struct_type, ctx_fields, (unsigned)n_captured, 0);

    LLVMValueRef ctx_alloca = LLVMBuildAlloca(ctx->builder, ctx_struct_type,
                                               "_pctx");
    for (int i = 0; i < n_captured; i++) {
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
    LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(ctx->builder);

    LLVMTypeRef wrapper_params[] = { ctx->type_i8ptr };
    LLVMTypeRef wrapper_type = LLVMFunctionType(ctx->type_i8ptr,
                                                 wrapper_params, 1, 0);

    LLVMValueRef *wrapper_fns = pgy_arena_calloc(&ctx->scratch,
        count * sizeof(LLVMValueRef));

    for (size_t i = 0; i < count; i++) {
        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name), "_pgy_par_%d_%zu",
                 ctx->parallel_counter, i);

        LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, wrapper_type);
        wrapper_fns[i] = fn;

        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(
            ctx->context, fn, "entry");
        LLVMPositionBuilderAtEnd(ctx->builder, entry);

        ctx->current_function = fn;
        ctx->current_ret_type = ctx->type_i8ptr;

        llvm_scope_push(ctx);

        LLVMValueRef arg0 = LLVMGetParam(fn, 0);
        LLVMValueRef ctx_ptr = LLVMBuildBitCast(ctx->builder, arg0,
            LLVMPointerType(ctx_struct_type, 0), "_pctx");

        for (int c = 0; c < n_captured; c++) {
            LLVMValueRef field_ptr = LLVMBuildStructGEP2(
                ctx->builder, ctx_struct_type, ctx_ptr, (unsigned)c,
                llvm_tmp_name(ctx));
            LLVMValueRef var_ptr = LLVMBuildLoad2(
                ctx->builder, ctx->type_i8ptr, field_ptr,
                llvm_tmp_name(ctx));
            llvm_scope_declare(ctx, captured[c].name, var_ptr, captured[c].type);
        }

        llvm_emit_statement(node->data.parallel.tasks[i], ctx);

        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))
                == NULL)
            LLVMBuildRet(ctx->builder, LLVMConstNull(ctx->type_i8ptr));

        llvm_scope_pop(ctx);
    }

    ctx->parallel_counter++;

    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret;
    LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);

    LLVMFuncEntry *spawn_fn = llvm_lookup_function(ctx, "pgy_spawn_export");
    LLVMFuncEntry *await_fn = llvm_lookup_function(ctx, "pgy_await_export");

    if (spawn_fn == NULL || await_fn == NULL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM parallel block requires registered runtime functions 'pgy_spawn_export' and 'pgy_await_export'; sequential fallback is disabled");
        return;
    }

    LLVMValueRef *handles = pgy_arena_calloc(&ctx->scratch,
        count * sizeof(LLVMValueRef));
    for (size_t i = 0; i < count; i++) {
        LLVMValueRef fn_ptr = LLVMBuildBitCast(
            ctx->builder, wrapper_fns[i], ctx->type_i8ptr,
            llvm_tmp_name(ctx));

        LLVMValueRef args[] = { fn_ptr, ctx_i8ptr };
        handles[i] = LLVMBuildCall2(ctx->builder, spawn_fn->fn_type,
                                     spawn_fn->fn, args, 2,
                                     llvm_tmp_name(ctx));
    }

    for (size_t i = 0; i < count; i++) {
        LLVMValueRef args[] = { handles[i] };
        LLVMBuildCall2(ctx->builder, await_fn->fn_type,
                       await_fn->fn, args, 1, "");
    }
}

static bool
llvm_select_case_parts(ASTNode *case_node, ASTNode **channel_out,
                       const char **bind_name_out, ASTNode **body_out)
{
    if (case_node == NULL || case_node->type != AST_BLOCK
        || case_node->data.block.count == 0)
        return false;

    ASTNode *first = case_node->data.block.statements[0];
    ASTNode *body = case_node->data.block.count >= 2
        ? case_node->data.block.statements[1] : NULL;

    if (first->type == AST_CHANNEL_RECV) {
        if (channel_out != NULL)
            *channel_out = first->data.channel_recv.channel;
        if (bind_name_out != NULL)
            *bind_name_out = NULL;
        if (body_out != NULL)
            *body_out = body;
        return true;
    }

    if (first->type == AST_ASSIGNMENT
        && first->data.assignment.target != NULL
        && first->data.assignment.target->type == AST_IDENTIFIER
        && first->data.assignment.value != NULL
        && first->data.assignment.value->type == AST_CHANNEL_RECV) {
        if (channel_out != NULL)
            *channel_out = first->data.assignment.value->data.channel_recv.channel;
        if (bind_name_out != NULL)
            *bind_name_out = first->data.assignment.target->data.identifier.name;
        if (body_out != NULL)
            *body_out = body;
        return true;
    }

    return false;
}

void
llvm_emit_async_block(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode fake_block = {0};
    fake_block.type = AST_BLOCK;
    fake_block.data.block.statements = node->data.async_block.statements;
    fake_block.data.block.count = node->data.async_block.statement_count;

    LLVMValueRef saved_fn = ctx->current_function;
    LLVMTypeRef saved_ret = ctx->current_ret_type;
    LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(ctx->builder);

    int saved_parallel_counter = ctx->parallel_counter;
    typedef struct { const char *name; LLVMValueRef alloca; LLVMTypeRef type; } CapturedVar;
    CapturedVar captured[MAX_SCOPE_VARS];
    int n_captured = 0;
    for (int i = 0; i < ctx->scope_depth; i++) {
        LLVMScopeFrame *frame = &ctx->scopes[i];
        for (int j = 0; j < frame->count && n_captured < MAX_SCOPE_VARS; j++) {
            captured[n_captured++] = (CapturedVar){
                frame->entries[j].name,
                frame->entries[j].alloca,
                frame->entries[j].type
            };
        }
    }
    bool has_captures = n_captured > 0;
    LLVMValueRef ctx_alloca = NULL;
    LLVMTypeRef ctx_struct_type = NULL;

    if (has_captures) {
        LLVMTypeRef *fields = pgy_arena_calloc(&ctx->scratch,
            (size_t)n_captured * sizeof(LLVMTypeRef));
        for (int i = 0; i < n_captured; i++)
            fields[i] = ctx->type_i8ptr;
        ctx_struct_type = LLVMStructCreateNamed(ctx->context, llvm_tmp_name(ctx));
        LLVMStructSetBody(ctx_struct_type, fields, (unsigned)n_captured, 0);

        ctx_alloca = LLVMBuildAlloca(ctx->builder, ctx_struct_type, "_actx");
        for (int i = 0; i < n_captured; i++) {
            LLVMValueRef gep = LLVMBuildStructGEP2(ctx->builder, ctx_struct_type,
                ctx_alloca, (unsigned)i, llvm_tmp_name(ctx));
            LLVMValueRef cast = LLVMBuildBitCast(ctx->builder, captured[i].alloca,
                ctx->type_i8ptr, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, cast, gep);
        }
    }

    LLVMValueRef ctx_i8ptr = has_captures
        ? LLVMBuildBitCast(ctx->builder, ctx_alloca, ctx->type_i8ptr, llvm_tmp_name(ctx))
        : LLVMConstNull(ctx->type_i8ptr);

    LLVMTypeRef wrapper_params[] = { ctx->type_i8ptr };
    LLVMTypeRef wrapper_type = LLVMFunctionType(ctx->type_i8ptr, wrapper_params, 1, 0);
    char fn_name[64];
    snprintf(fn_name, sizeof(fn_name), "_pgy_async_%d_0", ctx->parallel_counter++);
    LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, wrapper_type);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx->context, fn, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, entry);
    ctx->current_function = fn;
    ctx->current_ret_type = ctx->type_i8ptr;
    llvm_scope_push(ctx);
    if (has_captures) {
        LLVMValueRef arg0 = LLVMGetParam(fn, 0);
        LLVMValueRef ctx_ptr = LLVMBuildBitCast(ctx->builder, arg0,
            LLVMPointerType(ctx_struct_type, 0), "_actx");
        for (int i = 0; i < n_captured; i++) {
            LLVMValueRef field_ptr = LLVMBuildStructGEP2(ctx->builder, ctx_struct_type,
                ctx_ptr, (unsigned)i, llvm_tmp_name(ctx));
            LLVMValueRef var_ptr_i8 = LLVMBuildLoad2(ctx->builder, ctx->type_i8ptr,
                field_ptr, llvm_tmp_name(ctx));
            LLVMValueRef var_ptr = LLVMBuildBitCast(ctx->builder, var_ptr_i8,
                LLVMPointerType(captured[i].type, 0), llvm_tmp_name(ctx));
            llvm_scope_declare(ctx, captured[i].name, var_ptr, captured[i].type);
        }
    }
    llvm_emit_statement(&fake_block, ctx);
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildRet(ctx->builder, LLVMConstNull(ctx->type_i8ptr));
    llvm_scope_pop(ctx);

    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret;
    LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);

    LLVMFuncEntry *spawn_fn = llvm_lookup_function(ctx, "pgy_async_spawn_export");
    LLVMFuncEntry *detach_fn = llvm_lookup_function(ctx, "pgy_async_detach_export");
    if (spawn_fn == NULL || detach_fn == NULL) {
        ctx->parallel_counter = saved_parallel_counter;
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM async block requires registered runtime functions 'pgy_async_spawn_export' and 'pgy_async_detach_export'; synchronous fallback is disabled");
        return;
    }

    LLVMValueRef fn_ptr = LLVMBuildBitCast(ctx->builder, fn, ctx->type_i8ptr, llvm_tmp_name(ctx));
    LLVMValueRef spawn_args[] = { fn_ptr, ctx_i8ptr };
    LLVMValueRef handle = LLVMBuildCall2(ctx->builder, spawn_fn->fn_type, spawn_fn->fn,
        spawn_args, 2, llvm_tmp_name(ctx));
    LLVMValueRef detach_args[] = { handle };
    LLVMBuildCall2(ctx->builder, detach_fn->fn_type, detach_fn->fn, detach_args, 1, "");
}

void
llvm_emit_select_stmt(ASTNode *node, LLVMGenCtx *ctx)
{
    size_t case_count = node->data.select_stmt.case_count;
    LLVMValueRef fn = ctx->current_function;
    LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "select.end");

    if (case_count == 0) {
        if (node->data.select_stmt.default_case != NULL)
            llvm_emit_statement(node->data.select_stmt.default_case, ctx);
        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
            LLVMBuildBr(ctx->builder, merge_bb);
        LLVMPositionBuilderAtEnd(ctx->builder, merge_bb);
        return;
    }

    {
        int select_id = ctx->tmp_counter++;
        char rr_name[64];
        snprintf(rr_name, sizeof(rr_name), "__pgy_select_rr_%d", select_id);

        LLVMValueRef rr_global = LLVMAddGlobal(ctx->module, ctx->type_i32, rr_name);
        LLVMSetInitializer(rr_global, LLVMConstInt(ctx->type_i32, 0, 0));
        LLVMSetLinkage(rr_global, LLVMInternalLinkage);

        LLVMValueRef rr_cur = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
            rr_global, llvm_tmp_name(ctx));
        LLVMValueRef rr_next = LLVMBuildAdd(ctx->builder, rr_cur,
            LLVMConstInt(ctx->type_i32, 1, 0), llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, rr_next, rr_global);

        LLVMValueRef start = LLVMBuildURem(ctx->builder, rr_cur,
            LLVMConstInt(ctx->type_i32, (unsigned long long)case_count, 0),
            llvm_tmp_name(ctx));

        LLVMBasicBlockRef default_bb = LLVMAppendBasicBlockInContext(
            ctx->context, fn, "select.default");
        LLVMBasicBlockRef *rotation_bbs = pgy_arena_calloc(&ctx->scratch,
            case_count * sizeof(LLVMBasicBlockRef));
        for (size_t i = 0; i < case_count; i++) {
            rotation_bbs[i] = LLVMAppendBasicBlockInContext(
                ctx->context, fn, "select.rotation");
        }

        LLVMValueRef dispatch = LLVMBuildSwitch(ctx->builder, start,
            rotation_bbs[0], (unsigned)(case_count > 0 ? case_count - 1 : 0));
        for (size_t i = 1; i < case_count; i++) {
            LLVMAddCase(dispatch, LLVMConstInt(ctx->type_i32,
                (unsigned long long)i, 0), rotation_bbs[i]);
        }

        for (size_t start_idx = 0; start_idx < case_count; start_idx++) {
            LLVMBasicBlockRef next_check_bb = NULL;
            LLVMPositionBuilderAtEnd(ctx->builder, rotation_bbs[start_idx]);

            for (size_t offset = 0; offset < case_count; offset++) {
                size_t i = (start_idx + offset) % case_count;
                ASTNode *case_node = node->data.select_stmt.cases[i];
                ASTNode *channel = NULL;
                ASTNode *body = NULL;
                const char *bind_name = NULL;
                bool valid_case = llvm_select_case_parts(case_node, &channel, &bind_name, &body);

                LLVMBasicBlockRef case_bb = LLVMAppendBasicBlockInContext(
                    ctx->context, fn, "select.case");
                LLVMBasicBlockRef fail_bb = LLVMAppendBasicBlockInContext(
                    ctx->context, fn, "select.next");

                if (next_check_bb != NULL)
                    LLVMPositionBuilderAtEnd(ctx->builder, next_check_bb);

                if (valid_case && channel != NULL && channel->type == AST_IDENTIFIER) {
                    const char *channel_name = channel->data.identifier.name;
                    const char *inner = llvm_lookup_channel_inner(ctx, channel_name);
                    LLVMVarEntry *ch_var = llvm_scope_lookup(ctx, channel_name);
                    if (inner == NULL || inner[0] == '\0') {
                        llvm_set_error_at_with_hints(ctx, channel,
                            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                            PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING,
                            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                            "LLVM select channel '%s' requires concrete Channel<T> metadata",
                            channel_name != NULL ? channel_name : "<channel>");
                        return;
                    }

                    if (ch_var != NULL) {
                        char fn_name[128];
                        if (bind_name != NULL) {
                            LLVMTypeRef val_ty = pergyra_type_to_llvm(ctx, inner);
                            LLVMValueRef tmp = llvm_create_entry_alloca(ctx, val_ty, llvm_tmp_name(ctx));
                            snprintf(fn_name, sizeof(fn_name), "pgy_channel_try_recv_%s", inner);
                            LLVMFuncEntry *try_fn = llvm_lookup_function(ctx, fn_name);
                            if (try_fn == NULL) {
                                llvm_set_error_at_with_hints(ctx, channel,
                                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                                    PGY_FIX_INSPECT_MIR_INVENTORY,
                                    "LLVM select receive requires registered runtime function '%s'",
                                    fn_name);
                                return;
                            }

                            LLVMValueRef args[] = { ch_var->alloca, tmp };
                            LLVMValueRef ok = LLVMBuildCall2(ctx->builder, try_fn->fn_type,
                                try_fn->fn, args, 2, llvm_tmp_name(ctx));
                            LLVMBuildCondBr(ctx->builder, ok, case_bb, fail_bb);

                            LLVMPositionBuilderAtEnd(ctx->builder, case_bb);
                            llvm_scope_push(ctx);
                            {
                                LLVMValueRef bind_alloca =
                                    llvm_create_entry_alloca(ctx, val_ty, bind_name);
                                LLVMValueRef received = LLVMBuildLoad2(ctx->builder, val_ty, tmp,
                                    llvm_tmp_name(ctx));
                                LLVMBuildStore(ctx->builder, received, bind_alloca);
                                llvm_scope_declare(ctx, pergyra_strdup(bind_name),
                                                   bind_alloca, val_ty);
                            }
                            if (body != NULL)
                                llvm_emit_statement(body, ctx);
                            llvm_scope_pop(ctx);
                            if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
                                LLVMBuildBr(ctx->builder, merge_bb);
                            next_check_bb = fail_bb;
                            continue;
                        } else {
                            snprintf(fn_name, sizeof(fn_name), "pgy_channel_ready_%s", inner);
                            LLVMFuncEntry *ready_fn = llvm_lookup_function(ctx, fn_name);
                            if (ready_fn == NULL) {
                                llvm_set_error_at_with_hints(ctx, channel,
                                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                                    PGY_FIX_INSPECT_MIR_INVENTORY,
                                    "LLVM select readiness requires registered runtime function '%s'",
                                    fn_name);
                                return;
                            }

                            LLVMValueRef args[] = { ch_var->alloca };
                            LLVMValueRef ready = LLVMBuildCall2(ctx->builder, ready_fn->fn_type,
                                ready_fn->fn, args, 1, llvm_tmp_name(ctx));
                            LLVMBuildCondBr(ctx->builder, ready, case_bb, fail_bb);

                            LLVMPositionBuilderAtEnd(ctx->builder, case_bb);
                            {
                                char recv_name[128];
                                snprintf(recv_name, sizeof(recv_name), "pgy_channel_recv_val_%s", inner);
                                LLVMFuncEntry *recv_fn = llvm_lookup_function(ctx, recv_name);
                                if (recv_fn == NULL) {
                                    llvm_set_error_at_with_hints(ctx, channel,
                                        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                                        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                                        PGY_FIX_INSPECT_MIR_INVENTORY,
                                        "LLVM select consume requires registered runtime function '%s'",
                                        recv_name);
                                    return;
                                }
                                LLVMValueRef recv_args[] = { ch_var->alloca };
                                (void)LLVMBuildCall2(ctx->builder, recv_fn->fn_type,
                                    recv_fn->fn, recv_args, 1, "");
                            }
                            if (body != NULL)
                                llvm_emit_statement(body, ctx);
                            if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
                                LLVMBuildBr(ctx->builder, merge_bb);
                            next_check_bb = fail_bb;
                            continue;
                        }
                    }
                }

                LLVMBuildBr(ctx->builder, case_bb);
                LLVMPositionBuilderAtEnd(ctx->builder, case_bb);
                if (case_node != NULL)
                    llvm_emit_statement(case_node, ctx);
                if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
                    LLVMBuildBr(ctx->builder, merge_bb);
                next_check_bb = fail_bb;
            }

            if (next_check_bb != NULL)
                LLVMPositionBuilderAtEnd(ctx->builder, next_check_bb);
            LLVMBuildBr(ctx->builder, default_bb);
        }

        LLVMPositionBuilderAtEnd(ctx->builder, default_bb);
    }

    if (node->data.select_stmt.default_case != NULL)
        llvm_emit_statement(node->data.select_stmt.default_case, ctx);
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, merge_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, merge_bb);
}

#endif /* PGY_LLVM_ENABLED */
