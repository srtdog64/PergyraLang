#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"

static const char *
llvm_stmt_first_call_type_arg_name(ASTNode *call)
{
    GenericParam *param;

    if (call == NULL || call->type != AST_CALL
        || call->data.call.generic_args == NULL
        || call->data.call.generic_args->count < 1
        || call->data.call.generic_args->params == NULL
        || call->data.call.generic_args->params[0] == NULL) {
        return NULL;
    }
    param = call->data.call.generic_args->params[0];
    if (param->constraint != NULL
        && param->constraint->type == AST_TYPE
        && param->constraint->data.type.name != NULL) {
        return param->constraint->data.type.name;
    }
    return param->name;
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

    name = node->data.let_decl.name;
    type_ann = node->data.let_decl.type;
    init = node->data.let_decl.initializer;

    if (init == NULL || init->type != AST_CALL
        || init->data.call.callee == NULL
        || init->data.call.callee->type != AST_IDENTIFIER) {
        return false;
    }

    const char *callee = init->data.call.callee->data.identifier.name;
    if (strcmp(callee, "ClaimSlot") == 0
        || strcmp(callee, "ClaimSecureSlot") == 0) {
        const char *inner = NULL;
        bool is_secure = (strcmp(callee, "ClaimSecureSlot") == 0);
        if (type_ann != NULL && type_ann->type == AST_TYPE
            && type_ann->data.type.generic_args != NULL
            && type_ann->data.type.generic_args->count > 0) {
            inner = type_ann->data.type.generic_args->params[0]->name;
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
                is_secure ? "SecureSlot" : "Slot");
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
        return true;
    }

    if (strcmp(callee, "ClaimDeviceSlot") == 0) {
        const char *inner = NULL;
        if (type_ann != NULL && type_ann->type == AST_TYPE
            && type_ann->data.type.generic_args != NULL
            && type_ann->data.type.generic_args->count > 0) {
            inner = type_ann->data.type.generic_args->params[0]->name;
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
        return true;
    }

    return false;
}

#endif /* PGY_LLVM_ENABLED */
