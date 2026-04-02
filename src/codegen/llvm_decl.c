#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"

/* =================================================================
 * Function declaration emission
 * ================================================================= */

void
llvm_forward_declare_func(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *name = node->data.func_decl.name;
    size_t param_count = node->data.func_decl.param_count;

    /* Return type */
    LLVMTypeRef ret_type = ctx->type_void;
    if (node->data.func_decl.return_type != NULL)
        ret_type = ast_type_to_llvm(ctx, node->data.func_decl.return_type);

    /* Parameter types */
    LLVMTypeRef *param_types = NULL;
    if (param_count > 0) {
        param_types = calloc(param_count, sizeof(LLVMTypeRef));
        for (size_t i = 0; i < param_count; i++) {
            FuncParam *p = node->data.func_decl.params[i];
            param_types[i] = (p->type != NULL)
                ? ast_type_to_llvm(ctx, p->type)
                : ctx->type_i32;
        }
    }

    LLVMTypeRef fn_type = LLVMFunctionType(ret_type, param_types,
                                            (unsigned)param_count, 0);
    LLVMValueRef fn = LLVMAddFunction(ctx->module, name, fn_type);
    llvm_register_function(ctx, name, fn, fn_type, ret_type);

    free(param_types);
}

void
llvm_emit_func_decl(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *name = node->data.func_decl.name;

    LLVMFuncEntry *entry = llvm_lookup_function(ctx, name);
    if (entry == NULL)
        return;

    LLVMValueRef fn = entry->fn;
    LLVMTypeRef ret_type = entry->ret_type;

    /* Save context */
    LLVMValueRef saved_fn       = ctx->current_function;
    LLVMTypeRef  saved_ret_type = ctx->current_ret_type;

    ctx->current_function = fn;
    ctx->current_ret_type = ret_type;

    /* Create entry block */
    LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, bb);

    llvm_scope_push(ctx);

    /* Create allocas for parameters and store incoming values */
    for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
        FuncParam *p = node->data.func_decl.params[i];
        LLVMTypeRef pt = (p->type != NULL)
            ? ast_type_to_llvm(ctx, p->type)
            : ctx->type_i32;

        LLVMValueRef alloca = llvm_create_entry_alloca(ctx, pt, p->name);
        LLVMBuildStore(ctx->builder, LLVMGetParam(fn, (unsigned)i), alloca);
        llvm_scope_declare(ctx, p->name, alloca, pt);

        if (p->type != NULL && p->type->type == AST_TYPE
            && p->type->data.type.name != NULL
            && llvm_lookup_class(ctx, p->type->data.type.name) != NULL) {
            llvm_register_var_class(ctx, p->name, p->type->data.type.name);
        }
    }

    /* Emit body */
    if (node->data.func_decl.body != NULL)
        llvm_emit_block(node->data.func_decl.body, ctx);

    /* Add implicit return if no terminator */
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL) {
        if (ret_type == ctx->type_void)
            LLVMBuildRetVoid(ctx->builder);
        else
            LLVMBuildRet(ctx->builder,
                          LLVMConstInt(ret_type, 0, 0));
    }

    llvm_scope_pop(ctx);

    /* Restore context */
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret_type;

    /* Position builder back to the calling context */
    if (saved_fn != NULL) {
        LLVMBasicBlockRef last_bb = LLVMGetLastBasicBlock(saved_fn);
        if (last_bb != NULL)
            LLVMPositionBuilderAtEnd(ctx->builder, last_bb);
    }
}

#endif /* PGY_LLVM_ENABLED */
