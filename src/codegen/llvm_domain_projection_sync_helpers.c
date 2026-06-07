#ifdef PGY_LLVM_ENABLED

#include "llvm_domain_projection_sync_helpers.h"

#include "llvm_domain_projection_value_helpers.h"
#include "llvm_internal_api.h"
#include "llvm_inventory_decl_lookup.h"
#include "llvm_domain_projection_sync_body_helpers.h"

void
llvm_emit_domain_projection_sync(ASTNode *stmt,
                                 const char *decl_name,
                                 LLVMClassTypeEntry *decl_cls,
                                 LLVMValueRef sync_fn,
                                 LLVMGenCtx *ctx)
{
    LLVMValueRef saved_fn;
    LLVMTypeRef saved_ret;
    LLVMTypeRef saved_function_ret;
    const char *saved_return_type_name;
    ASTNode *saved_return_callable_type;
    ASTNode *saved_host_decl;
    LLVMBasicBlockRef saved_bb;
    LLVMLexicalRegistrySnapshot lexical_snapshot;
    LLVMBasicBlockRef bb;

    if (stmt == NULL || decl_name == NULL || decl_cls == NULL || sync_fn == NULL || ctx == NULL)
        return;

    saved_fn = ctx->current_function;
    saved_ret = ctx->current_ret_type;
    saved_function_ret = ctx->current_function_ret_type;
    saved_return_type_name = ctx->current_return_type_name;
    saved_return_callable_type = ctx->current_return_callable_type;
    saved_bb = LLVMGetInsertBlock(ctx->builder);
    lexical_snapshot = llvm_lexical_registry_snapshot(ctx);
    saved_host_decl = llvm_bind_current_host_decl(ctx, stmt);
    bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, bb);
    ctx->current_function = sync_fn;
    ctx->current_ret_type = ctx->type_void;
    ctx->current_function_ret_type = ctx->type_void;
    ctx->current_return_type_name = NULL;
    ctx->current_return_callable_type = NULL;

    llvm_scope_push(ctx);
    {
        LLVMTypeRef self_ptr_t = LLVMPointerType(decl_cls->struct_type, 0);
        LLVMValueRef sa = llvm_create_entry_alloca(ctx, self_ptr_t, "self.addr");
        LLVMBuildStore(ctx->builder, LLVMGetParam(sync_fn, 0), sa);
        llvm_scope_declare(ctx, "self", sa, self_ptr_t);
        llvm_register_var_class(ctx, "self", decl_name);
    }

    llvm_emit_domain_projection_sync_body(stmt, decl_cls, sync_fn, ctx);

    LLVMBuildRetVoid(ctx->builder);
    llvm_scope_pop(ctx);
    llvm_lexical_registry_restore(ctx, lexical_snapshot);
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret;
    ctx->current_function_ret_type = saved_function_ret;
    ctx->current_return_type_name = saved_return_type_name;
    ctx->current_return_callable_type = saved_return_callable_type;
    llvm_restore_current_host_decl(ctx, saved_host_decl);

    if (saved_bb != NULL)
        LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);
}

#endif
