#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "parser/ast_api.h"

static LLVMFuncEntry *
llvm_stmt_for_in_required_runtime(LLVMGenCtx *ctx,
                                  ASTNode *node,
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
            "LLVM statement for-in lowering requires registered runtime function '%s'",
            function_name != NULL ? function_name : "<missing>");
    }
    return fn;
}


void
llvm_emit_while_loop(ASTNode *node, LLVMGenCtx *ctx)
{
    LLVMValueRef fn = ctx->current_function;
    LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "while.cond");
    LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "while.body");
    LLVMBasicBlockRef exit_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "while.exit");

    LLVMBuildBr(ctx->builder, cond_bb);

    /* Condition */
    LLVMPositionBuilderAtEnd(ctx->builder, cond_bb);
    LLVMValueRef cond = llvm_emit_expression(ast_while_condition(node),
                                              ctx);
    if (cond != NULL && LLVMTypeOf(cond) != ctx->type_i1)
        cond = LLVMBuildICmp(ctx->builder, LLVMIntNE, cond,
                              LLVMConstInt(LLVMTypeOf(cond), 0, 0),
                              llvm_tmp_name(ctx));
    if (cond == NULL)
        cond = LLVMConstInt(ctx->type_i1, 0, 0);

    LLVMBuildCondBr(ctx->builder, cond, body_bb, exit_bb);

    /* Body */
    LLVMPositionBuilderAtEnd(ctx->builder, body_bb);
    if (ctx->loop_depth < MAX_SCOPE_DEPTH) {
        ctx->loop_labels[ctx->loop_depth] = ast_while_label(node);
        ctx->loop_continue_blocks[ctx->loop_depth] = cond_bb;
        ctx->loop_break_blocks[ctx->loop_depth] = exit_bb;
        ctx->loop_defer_base_depth[ctx->loop_depth] = ctx->defer_scope_depth;
        ctx->loop_depth++;
    }
    if (ast_while_body(node) != NULL)
        llvm_emit_statement(ast_while_body(node), ctx);
    if (ctx->loop_depth > 0) {
        ctx->loop_depth--;
        ctx->loop_labels[ctx->loop_depth] = NULL;
    }
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, cond_bb);

    /* Exit */
    LLVMPositionBuilderAtEnd(ctx->builder, exit_bb);
}

void
llvm_emit_for_loop(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *var_name = ast_for_variable(node);
    ASTNode *iterable_node = ast_for_iterable(node);
    ASTNode *body_node = ast_for_body(node);

    if (iterable_node != NULL) {
        if (iterable_node->type == AST_IDENTIFIER) {
            const char *iter_name = ast_identifier_name(iterable_node);
            const char *list_inner = llvm_lookup_list_inner(ctx, iter_name);
            LLVMVarEntry *list_var = llvm_scope_lookup(ctx, iter_name);
            if (list_inner != NULL && list_var != NULL) {
                LLVMTypeRef elem_ty = pergyra_type_to_llvm(ctx, list_inner);
                if (ctx->has_error || elem_ty == NULL)
                    return;
                LLVMValueRef idx_alloca;
                LLVMValueRef fn = ctx->current_function;
                LLVMBasicBlockRef cond_bb;
                LLVMBasicBlockRef body_bb;
                LLVMBasicBlockRef incr_bb;
                LLVMBasicBlockRef exit_bb;
                LLVMFuncEntry *size_fn = llvm_stmt_for_in_required_runtime(ctx,
                    node, "pgy_list_size_raw_export");
                LLVMFuncEntry *get_fn = llvm_stmt_for_in_required_runtime(ctx,
                    node, "pgy_list_get_raw_export");

                if (size_fn == NULL || get_fn == NULL)
                    return;

                llvm_scope_push(ctx);
                idx_alloca = llvm_create_entry_alloca(ctx, ctx->type_i32, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i32, 0, 0), idx_alloca);
                {
                    LLVMValueRef item_alloca = llvm_create_entry_alloca(ctx, elem_ty, var_name);
                    llvm_scope_declare(ctx, var_name, item_alloca, elem_ty);
                    {
                        LLVMClassTypeEntry *cls = llvm_stmt_lookup_class_by_type(ctx, elem_ty);
                        if (cls != NULL)
                            llvm_register_var_class(ctx, var_name, cls->class_name);
                    }
                }

                cond_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "forin.list.cond");
                body_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "forin.list.body");
                incr_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "forin.list.incr");
                exit_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "forin.list.exit");
                LLVMBuildBr(ctx->builder, cond_bb);

                LLVMPositionBuilderAtEnd(ctx->builder, cond_bb);
                {
                    LLVMValueRef idx = LLVMBuildLoad2(ctx->builder, ctx->type_i32, idx_alloca, llvm_tmp_name(ctx));
                    LLVMValueRef size_call = LLVMConstInt(ctx->type_i32, 0, 0);
                    if (size_fn != NULL) {
                        LLVMValueRef args[] = {
                            LLVMBuildBitCast(ctx->builder, list_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx))
                        };
                        size_call = LLVMBuildCall2(ctx->builder, size_fn->fn_type, size_fn->fn, args, 1, llvm_tmp_name(ctx));
                    }
                    LLVMValueRef cond = LLVMBuildICmp(ctx->builder, LLVMIntSLT, idx, size_call, llvm_tmp_name(ctx));
                    LLVMBuildCondBr(ctx->builder, cond, body_bb, exit_bb);
                }

                LLVMPositionBuilderAtEnd(ctx->builder, body_bb);
                {
                    LLVMValueRef idx = LLVMBuildLoad2(ctx->builder, ctx->type_i32, idx_alloca, llvm_tmp_name(ctx));
                    LLVMVarEntry *loop_var = llvm_scope_lookup(ctx, var_name);
                    if (get_fn != NULL && loop_var != NULL) {
                        LLVMValueRef args[] = {
                            LLVMBuildBitCast(ctx->builder, list_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                            idx,
                            LLVMBuildBitCast(ctx->builder, loop_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                            llvm_sizeof_type_i64(ctx, elem_ty)
                        };
                        LLVMBuildCall2(ctx->builder, get_fn->fn_type, get_fn->fn, args, 4, "");
                    }
                }
                if (ctx->loop_depth < MAX_SCOPE_DEPTH) {
                    ctx->loop_labels[ctx->loop_depth] = ast_for_label(node);
                    ctx->loop_continue_blocks[ctx->loop_depth] = incr_bb;
                    ctx->loop_break_blocks[ctx->loop_depth] = exit_bb;
                    ctx->loop_defer_base_depth[ctx->loop_depth] = ctx->defer_scope_depth;
                    ctx->loop_depth++;
                }
                if (body_node != NULL)
                    llvm_emit_statement(body_node, ctx);
                if (ctx->loop_depth > 0) {
                    ctx->loop_depth--;
                    ctx->loop_labels[ctx->loop_depth] = NULL;
                }
                if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
                    LLVMBuildBr(ctx->builder, incr_bb);

                LLVMPositionBuilderAtEnd(ctx->builder, incr_bb);
                {
                    LLVMValueRef idx = LLVMBuildLoad2(ctx->builder, ctx->type_i32, idx_alloca, llvm_tmp_name(ctx));
                    LLVMValueRef next = LLVMBuildAdd(ctx->builder, idx, LLVMConstInt(ctx->type_i32, 1, 0), llvm_tmp_name(ctx));
                    LLVMBuildStore(ctx->builder, next, idx_alloca);
                    LLVMBuildBr(ctx->builder, cond_bb);
                }

                LLVMPositionBuilderAtEnd(ctx->builder, exit_bb);
                llvm_scope_pop(ctx);
                return;
            }
        }

        LLVMValueRef iterable = llvm_emit_expression(iterable_node, ctx);
        LLVMTypeRef iterable_ty;
        LLVMTypeRef field_types[5];
        LLVMTypeRef elem_ty;
        LLVMValueRef data_ptr;
        LLVMValueRef count64;
        LLVMValueRef idx_alloca;
        LLVMValueRef fn;
        LLVMBasicBlockRef cond_bb;
        LLVMBasicBlockRef body_bb;
        LLVMBasicBlockRef incr_bb;
        LLVMBasicBlockRef exit_bb;

        if (iterable == NULL)
            return;
        iterable_ty = LLVMTypeOf(iterable);
        if (LLVMGetTypeKind(iterable_ty) != LLVMStructTypeKind
            || LLVMCountStructElementTypes(iterable_ty) < 2) {
            return;
        }

        LLVMGetStructElementTypes(iterable_ty, field_types);
        elem_ty = LLVMGetElementType(field_types[0]);
        data_ptr = LLVMBuildExtractValue(ctx->builder, iterable, 0, llvm_tmp_name(ctx));
        count64 = LLVMBuildExtractValue(ctx->builder, iterable, 1, llvm_tmp_name(ctx));

        llvm_scope_push(ctx);
        idx_alloca = llvm_create_entry_alloca(ctx, ctx->type_i64, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i64, 0, 0), idx_alloca);

        {
            LLVMValueRef item_alloca = llvm_create_entry_alloca(ctx, elem_ty, var_name);
            llvm_scope_declare(ctx, var_name, item_alloca, elem_ty);
            {
                LLVMClassTypeEntry *cls = llvm_stmt_lookup_class_by_type(ctx, elem_ty);
                if (cls != NULL)
                    llvm_register_var_class(ctx, var_name, cls->class_name);
            }
        }

        fn = ctx->current_function;
        cond_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "forin.cond");
        body_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "forin.body");
        incr_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "forin.incr");
        exit_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "forin.exit");

        LLVMBuildBr(ctx->builder, cond_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, cond_bb);
        {
            LLVMValueRef idx = LLVMBuildLoad2(ctx->builder, ctx->type_i64, idx_alloca, llvm_tmp_name(ctx));
            LLVMValueRef cond = LLVMBuildICmp(ctx->builder, LLVMIntULT, idx, count64, llvm_tmp_name(ctx));
            LLVMBuildCondBr(ctx->builder, cond, body_bb, exit_bb);
        }

        LLVMPositionBuilderAtEnd(ctx->builder, body_bb);
        {
            LLVMValueRef idx = LLVMBuildLoad2(ctx->builder, ctx->type_i64, idx_alloca, llvm_tmp_name(ctx));
            LLVMValueRef item_ptr = LLVMBuildGEP2(ctx->builder, elem_ty, data_ptr, &idx, 1, llvm_tmp_name(ctx));
            LLVMValueRef item = LLVMBuildLoad2(ctx->builder, elem_ty, item_ptr, llvm_tmp_name(ctx));
            LLVMVarEntry *loop_var = llvm_scope_lookup(ctx, var_name);
            if (loop_var != NULL)
                LLVMBuildStore(ctx->builder, item, loop_var->alloca);
        }
        if (ctx->loop_depth < MAX_SCOPE_DEPTH) {
            ctx->loop_labels[ctx->loop_depth] = ast_for_label(node);
            ctx->loop_continue_blocks[ctx->loop_depth] = incr_bb;
            ctx->loop_break_blocks[ctx->loop_depth] = exit_bb;
            ctx->loop_defer_base_depth[ctx->loop_depth] = ctx->defer_scope_depth;
            ctx->loop_depth++;
        }
        if (body_node != NULL)
            llvm_emit_statement(body_node, ctx);
        if (ctx->loop_depth > 0) {
            ctx->loop_depth--;
            ctx->loop_labels[ctx->loop_depth] = NULL;
        }
        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
            LLVMBuildBr(ctx->builder, incr_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, incr_bb);
        {
            LLVMValueRef idx = LLVMBuildLoad2(ctx->builder, ctx->type_i64, idx_alloca, llvm_tmp_name(ctx));
            LLVMValueRef next = LLVMBuildAdd(ctx->builder, idx,
                LLVMConstInt(ctx->type_i64, 1, 0), llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, next, idx_alloca);
            LLVMBuildBr(ctx->builder, cond_bb);
        }

        LLVMPositionBuilderAtEnd(ctx->builder, exit_bb);
        llvm_scope_pop(ctx);
        return;
    }

    llvm_scope_push(ctx);

    /* Create loop variable */
    LLVMValueRef var_alloca = llvm_create_entry_alloca(ctx, ctx->type_i32,
                                                        var_name);
    LLVMValueRef start = llvm_emit_expression(ast_for_range_start(node), ctx);
    if (start == NULL)
        start = LLVMConstInt(ctx->type_i32, 0, 0);
    LLVMBuildStore(ctx->builder, start, var_alloca);
    llvm_scope_declare(ctx, var_name, var_alloca, ctx->type_i32);

    LLVMValueRef fn = ctx->current_function;
    LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "for.cond");
    LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "for.body");
    LLVMBasicBlockRef incr_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "for.incr");
    LLVMBasicBlockRef exit_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "for.exit");

    LLVMBuildBr(ctx->builder, cond_bb);

    /* Condition: i < end */
    LLVMPositionBuilderAtEnd(ctx->builder, cond_bb);
    LLVMValueRef current = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                                           var_alloca, llvm_tmp_name(ctx));
    LLVMValueRef end = llvm_emit_expression(ast_for_range_end(node), ctx);
    if (end == NULL)
        end = LLVMConstInt(ctx->type_i32, 0, 0);
    LLVMValueRef cond = LLVMBuildICmp(ctx->builder, LLVMIntSLT, current, end,
                                       llvm_tmp_name(ctx));
    LLVMBuildCondBr(ctx->builder, cond, body_bb, exit_bb);

    /* Body */
    LLVMPositionBuilderAtEnd(ctx->builder, body_bb);
    if (ctx->loop_depth < MAX_SCOPE_DEPTH) {
        ctx->loop_labels[ctx->loop_depth] = ast_for_label(node);
        ctx->loop_continue_blocks[ctx->loop_depth] = incr_bb;
        ctx->loop_break_blocks[ctx->loop_depth] = exit_bb;
        ctx->loop_defer_base_depth[ctx->loop_depth] = ctx->defer_scope_depth;
        ctx->loop_depth++;
    }
    if (body_node != NULL)
        llvm_emit_statement(body_node, ctx);
    if (ctx->loop_depth > 0) {
        ctx->loop_depth--;
        ctx->loop_labels[ctx->loop_depth] = NULL;
    }
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, incr_bb);

    /* Increment: i = i + 1 */
    LLVMPositionBuilderAtEnd(ctx->builder, incr_bb);
    LLVMValueRef cur2 = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                                        var_alloca, llvm_tmp_name(ctx));
    LLVMValueRef next = LLVMBuildAdd(ctx->builder, cur2,
                                      LLVMConstInt(ctx->type_i32, 1, 0),
                                      llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, next, var_alloca);
    LLVMBuildBr(ctx->builder, cond_bb);

    /* Exit */
    LLVMPositionBuilderAtEnd(ctx->builder, exit_bb);

    llvm_scope_pop(ctx);
}

#endif /* PGY_LLVM_ENABLED */
