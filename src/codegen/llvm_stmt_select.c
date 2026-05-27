#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "llvm_stmt_parallel_names.h"
#include "../parser/ast_api.h"

#include <string.h>

static bool
llvm_select_case_parts(ASTNode *case_node, ASTNode **channel_out,
                       const char **bind_name_out, ASTNode **body_out)
{
    if (case_node == NULL || case_node->type != AST_BLOCK
        || ast_block_statement_count(case_node) == 0)
        return false;

    ASTNode *first = ast_block_statement(case_node, 0);
    ASTNode *body = ast_block_statement_count(case_node) >= 2
        ? ast_block_statement(case_node, 1) : NULL;

    if (first->type == AST_CHANNEL_RECV) {
        if (channel_out != NULL)
            *channel_out = ast_channel_recv_channel(first);
        if (bind_name_out != NULL)
            *bind_name_out = NULL;
        if (body_out != NULL)
            *body_out = body;
        return true;
    }

    if (first->type == AST_ASSIGNMENT
        && ast_assignment_target(first) != NULL
        && ast_assignment_target(first)->type == AST_IDENTIFIER
        && ast_assignment_value(first) != NULL
        && ast_assignment_value(first)->type == AST_CHANNEL_RECV) {
        if (channel_out != NULL)
            *channel_out = ast_channel_recv_channel(ast_assignment_value(first));
        if (bind_name_out != NULL)
            *bind_name_out = ast_identifier_name(ast_assignment_target(first));
        if (body_out != NULL)
            *body_out = body;
        return true;
    }

    return false;
}

typedef struct {
    ASTNode      *case_node;
    ASTNode      *channel;
    ASTNode      *body;
    const char   *bind_name;
    const char   *channel_name;
    const char   *inner;
    LLVMVarEntry *channel_var;
    bool          valid;
} LLVMSelectCaseInfo;

static bool
llvm_select_case_info(ASTNode *case_node, LLVMGenCtx *ctx,
                      LLVMSelectCaseInfo *out)
{
    memset(out, 0, sizeof(*out));
    out->case_node = case_node;
    out->valid = llvm_select_case_parts(case_node, &out->channel,
                                        &out->bind_name, &out->body);

    if (!out->valid || out->channel == NULL
        || out->channel->type != AST_IDENTIFIER) {
        return true;
    }

    out->channel_name = ast_identifier_name(out->channel);
    out->inner = llvm_lookup_channel_inner(ctx, out->channel_name);
    out->channel_var = llvm_scope_lookup(ctx, out->channel_name);
    if (out->inner == NULL || out->inner[0] == '\0') {
        llvm_set_error_at_with_hints(ctx, out->channel,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM select channel '%s' requires concrete Channel<T> metadata",
            out->channel_name != NULL ? out->channel_name : "<channel>");
        return false;
    }
    return true;
}

static LLVMFuncEntry *
llvm_select_required_runtime_function(LLVMGenCtx *ctx,
                                      ASTNode *channel,
                                      const char *family,
                                      const char *function_name)
{
    LLVMFuncEntry *fn;

    if (ctx == NULL || function_name == NULL)
        return NULL;
    fn = llvm_lookup_function(ctx, function_name);
    if (fn != NULL)
        return fn;
    llvm_set_error_at_with_hints(ctx, channel,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_INSPECT_MIR_INVENTORY,
        "LLVM select %s requires registered runtime function '%s'",
        family != NULL ? family : "operation",
        function_name);
    return NULL;
}

static bool
llvm_select_emit_bound_receive_case(const LLVMSelectCaseInfo *info,
                                    LLVMBasicBlockRef case_bb,
                                    LLVMBasicBlockRef fail_bb,
                                    LLVMBasicBlockRef merge_bb,
                                    LLVMGenCtx *ctx)
{
    char fn_name[128];
    LLVMTypeRef val_ty = pergyra_type_to_llvm(ctx, info->inner);
    if (ctx->has_error || val_ty == NULL)
        return false;
    LLVMValueRef tmp = llvm_create_entry_alloca(ctx, val_ty, llvm_tmp_name(ctx));
    if (!llvm_select_channel_runtime_name(ctx, fn_name, sizeof(fn_name),
            "pgy_channel_try_recv_", info->inner))
        return false;
    LLVMFuncEntry *try_fn = llvm_select_required_runtime_function(
        ctx, info->channel, "receive", fn_name);
    if (try_fn == NULL)
        return false;

    LLVMValueRef args[] = { info->channel_var->alloca, tmp };
    LLVMValueRef ok = LLVMBuildCall2(ctx->builder, try_fn->fn_type,
        try_fn->fn, args, 2, llvm_tmp_name(ctx));
    LLVMBuildCondBr(ctx->builder, ok, case_bb, fail_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, case_bb);
    llvm_scope_push(ctx);
    {
        LLVMValueRef bind_alloca =
            llvm_create_entry_alloca(ctx, val_ty, info->bind_name);
        LLVMValueRef received = LLVMBuildLoad2(ctx->builder, val_ty, tmp,
            llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, received, bind_alloca);
        llvm_scope_declare(ctx, pergyra_strdup(info->bind_name),
                           bind_alloca, val_ty);
    }
    if (info->body != NULL)
        llvm_emit_statement(info->body, ctx);
    llvm_scope_pop(ctx);
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, merge_bb);
    return true;
}

static bool
llvm_select_emit_ready_consume_case(const LLVMSelectCaseInfo *info,
                                    LLVMBasicBlockRef case_bb,
                                    LLVMBasicBlockRef fail_bb,
                                    LLVMBasicBlockRef merge_bb,
                                    LLVMGenCtx *ctx)
{
    char fn_name[128];
    if (!llvm_select_channel_runtime_name(ctx, fn_name, sizeof(fn_name),
            "pgy_channel_ready_", info->inner))
        return false;
    LLVMFuncEntry *ready_fn = llvm_select_required_runtime_function(
        ctx, info->channel, "readiness", fn_name);
    if (ready_fn == NULL)
        return false;

    LLVMValueRef args[] = { info->channel_var->alloca };
    LLVMValueRef ready = LLVMBuildCall2(ctx->builder, ready_fn->fn_type,
        ready_fn->fn, args, 1, llvm_tmp_name(ctx));
    LLVMBuildCondBr(ctx->builder, ready, case_bb, fail_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, case_bb);
    {
        char recv_name[128];
        if (!llvm_select_channel_runtime_name(ctx, recv_name, sizeof(recv_name),
                "pgy_channel_recv_val_", info->inner))
            return false;
        LLVMFuncEntry *recv_fn = llvm_select_required_runtime_function(
            ctx, info->channel, "consume", recv_name);
        if (recv_fn == NULL)
            return false;
        LLVMValueRef recv_args[] = { info->channel_var->alloca };
        (void)LLVMBuildCall2(ctx->builder, recv_fn->fn_type,
            recv_fn->fn, recv_args, 1, "");
    }
    if (info->body != NULL)
        llvm_emit_statement(info->body, ctx);
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, merge_bb);
    return true;
}

void
llvm_emit_select_stmt(ASTNode *node, LLVMGenCtx *ctx)
{
    size_t case_count = ast_select_case_count(node);
    LLVMValueRef fn = ctx->current_function;
    LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "select.end");

    if (case_count == 0) {
        if (ast_select_default_case(node) != NULL)
            llvm_emit_statement(ast_select_default_case(node), ctx);
        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
            LLVMBuildBr(ctx->builder, merge_bb);
        LLVMPositionBuilderAtEnd(ctx->builder, merge_bb);
        return;
    }

    {
        int select_id = ctx->tmp_counter++;
        char rr_name[64];
        if (!llvm_parallel_counter_name(ctx, rr_name, sizeof(rr_name),
                "__pgy_select_rr_", select_id))
            return;

        LLVMValueRef rr_global = LLVMAddGlobal(ctx->module, ctx->type_i32, rr_name);
        LLVMSetInitializer(rr_global, LLVMConstInt(ctx->type_i32, 0, 0));
        LLVMSetLinkage(rr_global, LLVMInternalLinkage);

        LLVMValueRef rr_cur = LLVMBuildAtomicRMW(ctx->builder,
            LLVMAtomicRMWBinOpAdd, rr_global,
            LLVMConstInt(ctx->type_i32, 1, 0),
            LLVMAtomicOrderingMonotonic,
            /*singleThread=*/0);

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
                ASTNode *case_node = ast_select_case(node, i);
                LLVMSelectCaseInfo info;
                if (!llvm_select_case_info(case_node, ctx, &info))
                    return;

                LLVMBasicBlockRef case_bb = LLVMAppendBasicBlockInContext(
                    ctx->context, fn, "select.case");
                LLVMBasicBlockRef fail_bb = LLVMAppendBasicBlockInContext(
                    ctx->context, fn, "select.next");

                if (next_check_bb != NULL)
                    LLVMPositionBuilderAtEnd(ctx->builder, next_check_bb);

                if (info.valid && info.channel != NULL
                    && info.channel->type == AST_IDENTIFIER
                    && info.channel_var != NULL) {
                    bool emitted = info.bind_name != NULL
                        ? llvm_select_emit_bound_receive_case(
                            &info, case_bb, fail_bb, merge_bb, ctx)
                        : llvm_select_emit_ready_consume_case(
                            &info, case_bb, fail_bb, merge_bb, ctx);
                    if (!emitted)
                        return;
                    next_check_bb = fail_bb;
                    continue;
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

    if (ast_select_default_case(node) != NULL)
        llvm_emit_statement(ast_select_default_case(node), ctx);
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, merge_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, merge_bb);
}

#endif /* PGY_LLVM_ENABLED */
