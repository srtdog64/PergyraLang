#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "codegen_slot_type_policy.h"
#include "parser/ast_api.h"

static char *
llvm_stmt_first_call_type_arg_name(ASTNode *call)
{
    GenericParam *param;

    if (call == NULL || call->type != AST_CALL
        || ast_call_generic_arg_count(call) < 1
        || ast_call_generic_arg(call, 0) == NULL) {
        return NULL;
    }
    param = ast_call_generic_arg(call, 0);
    ASTNode *constraint = ast_generic_param_constraint(param);
    if (constraint != NULL && constraint->type == AST_TYPE) {
        return llvm_stmt_render_type_arg(param);
    }
    return llvm_stmt_render_type_arg(param);
}

static void
llvm_stmt_emit_secure_claim_token(LLVMGenCtx *ctx, const char *name,
                                  const char *inner, LLVMTypeRef slot_ty,
                                  LLVMValueRef alloca_val)
{
    LLVMTypeRef token_ty = llvm_secure_token_type(ctx, inner);
    char token_name[256];
    snprintf(token_name, sizeof(token_name), "%s_token", name);
    LLVMValueRef token_alloca = llvm_stmt_create_slot_alloca(ctx, token_ty,
        token_name);
    LLVMBuildStore(ctx->builder, LLVMConstNull(token_ty), token_alloca);

    LLVMValueRef slot_ptr_i64 = LLVMBuildPtrToInt(ctx->builder,
        alloca_val, ctx->type_i64, llvm_tmp_name(ctx));
    LLVMValueRef token_id = LLVMBuildXor(ctx->builder, slot_ptr_i64,
        LLVMConstInt(ctx->type_i64, 0xDEADBEEFCAFEBABEULL, 0),
        llvm_tmp_name(ctx));

    LLVMValueRef slot_token_ptr = LLVMBuildStructGEP2(ctx->builder,
        slot_ty, alloca_val, 2, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, token_id, slot_token_ptr);

    LLVMValueRef token_id_ptr = LLVMBuildStructGEP2(ctx->builder,
        token_ty, token_alloca, 0, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, token_id, token_id_ptr);

    LLVMValueRef token_write_ptr = LLVMBuildStructGEP2(ctx->builder,
        token_ty, token_alloca, 1, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder,
        LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
        token_write_ptr);

    LLVMValueRef token_read_ptr = LLVMBuildStructGEP2(ctx->builder,
        token_ty, token_alloca, 2, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder,
        LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
        token_read_ptr);

    llvm_scope_declare(ctx, pergyra_strdup(token_name), token_alloca, token_ty);
}

static void
llvm_stmt_emit_claimed_slot_storage(LLVMGenCtx *ctx, LLVMTypeRef slot_ty,
                                    LLVMValueRef alloca_val)
{
    LLVMBuildStore(ctx->builder, LLVMConstNull(slot_ty), alloca_val);
    LLVMValueRef claimed_ptr = LLVMBuildStructGEP2(ctx->builder,
        slot_ty, alloca_val, 1, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder,
        LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
        claimed_ptr);
}

bool
llvm_stmt_emit_claim_slot_let(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *name;
    ASTNode *type_ann;
    ASTNode *init;

    if (node == NULL || node->type != AST_LET_DECL || ctx == NULL)
        return false;

    name = ast_let_name(node);
    type_ann = ast_let_type(node);
    init = ast_let_initializer(node);

    if (init == NULL || init->type != AST_CALL
        || ast_call_callee(init) == NULL
        || ast_call_callee(init)->type != AST_IDENTIFIER) {
        return false;
    }

    const char *callee = ast_identifier_name(ast_call_callee(init));
    if (pgy_codegen_call_name_is_claim_slot(callee)
        || pgy_codegen_call_name_is_claim_secure_slot(callee)) {
        char *inner = NULL;
        bool is_secure = pgy_codegen_call_name_is_claim_secure_slot(callee);
        GenericParams *generic_args = ast_type_generic_args(type_ann);
        GenericParam *inner_param = ast_generic_param_at(generic_args, 0);
        if (type_ann != NULL && type_ann->type == AST_TYPE
            && inner_param != NULL) {
            inner = llvm_stmt_render_type_arg(inner_param);
        }
        if (inner == NULL)
            inner = llvm_stmt_first_call_type_arg_name(init);
        if (inner == NULL) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM %s let-binding for '%s' requires an explicit %s<T> annotation",
                callee,
                name != NULL ? name : "<slot>",
                pgy_codegen_claim_slot_abi_prefix(callee));
            free(inner);
            return true;
        }

        LLVMTypeRef slot_ty = is_secure
            ? llvm_secure_slot_struct_type(ctx, inner)
            : llvm_slot_struct_type(ctx, inner);
        LLVMValueRef alloca_val = llvm_stmt_create_slot_alloca(ctx, slot_ty,
            name);

        llvm_stmt_emit_claimed_slot_storage(ctx, slot_ty, alloca_val);
        if (is_secure)
            llvm_stmt_emit_secure_claim_token(ctx, name, inner, slot_ty,
                alloca_val);

        llvm_scope_declare(ctx, name, alloca_val, slot_ty);
        llvm_register_slot_var(ctx, name, inner, is_secure);
        free(inner);
        return true;
    }

    if (pgy_codegen_call_name_is_claim_device_slot(callee)) {
        char *inner = NULL;
        GenericParams *generic_args = ast_type_generic_args(type_ann);
        GenericParam *inner_param = ast_generic_param_at(generic_args, 0);
        if (type_ann != NULL && type_ann->type == AST_TYPE
            && inner_param != NULL) {
            inner = llvm_stmt_render_type_arg(inner_param);
        }
        if (inner == NULL) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM ClaimDeviceSlot let-binding for '%s' requires an explicit DeviceSlot<T> annotation",
                name != NULL ? name : "<slot>");
            return true;
        }

        LLVMTypeRef slot_ty = llvm_slot_struct_type(ctx, inner);
        LLVMValueRef alloca_val = llvm_stmt_create_slot_alloca(ctx, slot_ty,
            name);

        llvm_stmt_emit_claimed_slot_storage(ctx, slot_ty, alloca_val);
        llvm_scope_declare(ctx, name, alloca_val, slot_ty);
        llvm_register_device_slot_var(ctx, name, inner);
        free(inner);
        return true;
    }

    return false;
}

#endif /* PGY_LLVM_ENABLED */
