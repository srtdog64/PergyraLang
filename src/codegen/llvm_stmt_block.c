#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "../compiler/mir_abi_layout.h"

void
llvm_emit_block(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node == NULL || ctx->has_error)
        return;

    if (node->type != AST_BLOCK)
        return;

    LLVMLexicalRegistrySnapshot lexical_snapshot =
        llvm_lexical_registry_snapshot(ctx);
    llvm_defer_scope_push(ctx);
    llvm_scope_push(ctx);
    for (size_t i = 0; i < ast_block_statement_count(node); i++) {
        llvm_emit_statement(ast_block_statement(node, i), ctx);
        if (LLVMGetBasicBlockTerminator(
                LLVMGetInsertBlock(ctx->builder)) != NULL)
            break;
    }

    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL) {
        llvm_emit_defers_from(ctx, ctx->defer_scope_depth - 1);
        for (int i = ctx->slot_var_count - 1;
             i >= lexical_snapshot.slot_var_count;
             i--) {
            if (ctx->slot_vars[i].released)
                continue;
            const char *inner = ctx->slot_vars[i].inner_type;
            const char *vname = ctx->slot_vars[i].var_name;
            bool is_secure = ctx->slot_vars[i].is_secure;
            const char *runtime_fn = mir_abi_resource_runtime_fn_by_kind(
                is_secure ? MIR_RESOURCE_ABI_SECURE_SLOT
                          : MIR_RESOURCE_ABI_SLOT,
                inner, "Release");
            LLVMFuncEntry *fn = runtime_fn != NULL
                ? llvm_lookup_function(ctx, runtime_fn)
                : NULL;
            LLVMVarEntry var;
            if (!llvm_scope_lookup_snapshot(ctx, vname, &var)
                || var.alloca != ctx->slot_vars[i].binding) {
                continue;
            }
            if (fn != NULL) {
                if (is_secure) {
                    LLVMVarEntry token_var;
                    if (llvm_lookup_secure_token_var(ctx, vname, &token_var)) {
                        LLVMValueRef args[] = { var.alloca, token_var.alloca };
                        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                       args, 2, "");
                    } else {
                        llvm_set_error_at_with_hints(ctx, node,
                            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                            PGY_FIX_INSPECT_MIR_INVENTORY,
                            "LLVM secure slot auto-release requires paired token binding '%s_token'",
                            vname != NULL ? vname : "<slot>");
                        break;
                    }
                } else {
                    LLVMValueRef args[] = { var.alloca };
                    LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                   args, 1, "");
                }
            } else if (pgy_classify_type(inner) != PGY_TK_UNKNOWN) {
                if (runtime_fn != NULL) {
                    llvm_required_runtime_function(ctx, node,
                        is_secure ? "secure slot" : "slot",
                        "auto-release", runtime_fn);
                } else {
                    llvm_set_error_at_with_hints(ctx, node,
                        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                        PGY_FIX_INSPECT_MIR_INVENTORY,
                        "LLVM auto-release requires MIR ABI runtime function row");
                }
                break;
            } else if (is_secure) {
                LLVMValueRef occ_ptr = LLVMBuildStructGEP2(ctx->builder,
                    var.type, var.alloca, 1, llvm_tmp_name(ctx));
                LLVMValueRef token_ptr = LLVMBuildStructGEP2(ctx->builder,
                    var.type, var.alloca, 2, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 0, 0),
                    occ_ptr);
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(ctx->type_i64, 0, 0), token_ptr);
            } else {
                LLVMValueRef occ_ptr = LLVMBuildStructGEP2(ctx->builder,
                    var.type, var.alloca, 1, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 0, 0),
                    occ_ptr);
            }
            ctx->slot_vars[i].released = true;
        }
    }

    llvm_scope_pop(ctx);
    llvm_lexical_registry_restore(ctx, lexical_snapshot);
    llvm_defer_scope_pop(ctx);
}

#endif /* PGY_LLVM_ENABLED */
