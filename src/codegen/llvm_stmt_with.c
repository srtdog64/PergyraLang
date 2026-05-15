#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"

static bool
llvm_with_token_name(LLVMGenCtx *ctx, ASTNode *node,
                     char *out, size_t out_size, const char *alias)
{
    int written;

    if (out == NULL || out_size == 0)
        return false;

    written = snprintf(out, out_size, "%s_token",
        alias != NULL ? alias : "");
    if (written >= 0 && (size_t)written < out_size)
        return true;

    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_SPEC_LIMIT,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_REFACTOR_OR_RAISE_LIMIT,
            "LLVM with-slot token name is too long for alias '%s'",
            alias != NULL ? alias : "<alias>");
    }
    return false;
}

static bool
llvm_with_release_name(LLVMGenCtx *ctx, ASTNode *node,
                       char *out, size_t out_size,
                       const char *inner, bool is_secure)
{
    int written;

    if (out == NULL || out_size == 0)
        return false;

    written = snprintf(out, out_size,
        is_secure ? "pgy_secure_release_%s" : "pgy_release_%s",
        inner != NULL ? inner : "");
    if (written >= 0 && (size_t)written < out_size)
        return true;

    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_SPEC_LIMIT,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_REFACTOR_OR_RAISE_LIMIT,
            "LLVM with-slot cleanup runtime symbol is too long for type '%s'",
            inner != NULL ? inner : "<type>");
    }
    return false;
}

void
llvm_emit_with_stmt(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *alias = ast_with_alias(node);
    bool is_secure    = ast_with_is_secure(node);

    const char *inner = NULL;
    if (ast_with_slot_type(node) != NULL
        && ast_with_slot_type(node)->type == AST_TYPE
        && ast_type_name(ast_with_slot_type(node)) != NULL)
        inner = ast_type_name(ast_with_slot_type(node));
    if (inner == NULL || inner[0] == '\0') {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM with-slot alias '%s' requires concrete Slot<T> metadata",
            alias != NULL ? alias : "<alias>");
        return;
    }

    LLVMTypeRef slot_ty = is_secure
        ? llvm_secure_slot_struct_type(ctx, inner)
        : llvm_slot_struct_type(ctx, inner);
    LLVMValueRef alloca_val = llvm_stmt_create_slot_alloca(ctx, slot_ty, alias);

    char fn_name[64];
    LLVMBuildStore(ctx->builder, LLVMConstNull(slot_ty), alloca_val);
    LLVMValueRef claimed_ptr = LLVMBuildStructGEP2(ctx->builder,
        slot_ty, alloca_val, 1, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder,
        LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
        claimed_ptr);

    llvm_scope_push(ctx);
    if (is_secure) {
        LLVMTypeRef token_ty = llvm_secure_token_type(ctx, inner);
        char token_name[256];
        if (!llvm_with_token_name(ctx, node, token_name,
                sizeof(token_name), alias)) {
            llvm_scope_pop(ctx);
            return;
        }
        LLVMValueRef token_alloca = llvm_stmt_create_slot_alloca(ctx,
            token_ty, token_name);
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
        llvm_scope_declare(ctx, pergyra_strdup(token_name), token_alloca,
                           token_ty);
    }
    llvm_scope_declare(ctx, alias, alloca_val, slot_ty);
    llvm_register_slot_var(ctx, alias, inner, is_secure);

    if (ast_with_body(node) != NULL)
        llvm_emit_block(ast_with_body(node), ctx);

    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL) {
        LLVMFuncEntry *release_fn = NULL;
        if (llvm_with_release_name(ctx, node, fn_name, sizeof(fn_name),
                inner, is_secure))
            release_fn = llvm_lookup_function(ctx, fn_name);
        if (release_fn != NULL) {
            if (is_secure) {
                LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx,
                    alias);
                if (token_var != NULL) {
                    LLVMValueRef args[] = { alloca_val, token_var->alloca };
                    LLVMBuildCall2(ctx->builder, release_fn->fn_type,
                                   release_fn->fn, args, 2, "");
                } else {
                    llvm_set_error_at_with_hints(ctx, node,
                        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                        PGY_FIX_INSPECT_MIR_INVENTORY,
                        "LLVM secure with-slot cleanup requires paired token binding '%s_token'",
                        alias != NULL ? alias : "<slot>");
                }
            } else {
                LLVMValueRef args[] = { alloca_val };
                LLVMBuildCall2(ctx->builder, release_fn->fn_type,
                               release_fn->fn, args, 1, "");
            }
        } else if (!ctx->has_error && pgy_classify_type(inner) != PGY_TK_UNKNOWN) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM with-slot cleanup requires registered runtime function '%s'",
                fn_name);
        } else {
            LLVMValueRef occupied_ptr = LLVMBuildStructGEP2(ctx->builder,
                slot_ty, alloca_val, 1, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 0, 0),
                occupied_ptr);
            if (is_secure) {
                LLVMValueRef token_ptr = LLVMBuildStructGEP2(ctx->builder,
                    slot_ty, alloca_val, 2, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(ctx->type_i64, 0, 0), token_ptr);
            }
        }
    }

    llvm_scope_pop(ctx);
}

#endif /* PGY_LLVM_ENABLED */
