#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"

static const char *
llvm_destructure_binding_name(const MIRInstruction *inst,
                              ASTNode *node,
                              size_t index)
{
    if (inst != NULL)
        return mir_instruction_destructure_binding_name_at(inst, index);
    return ast_let_destructure_name(node, index);
}

static void
llvm_emit_destructure_parts(ASTNode *init,
                            ASTNode *diagnostic_node,
                            const MIRInstruction *inst,
                            ASTNode *node,
                            size_t binding_count,
                            LLVMGenCtx *ctx)
{
    /* let (a, b, c) = expr;
     * Two shapes supported:
     *   1) Tuple: struct { T0, T1, ... } -> ExtractValue per field.
     *   2) Array-like: struct { T* data, i64 size, i64 cap } -> GEP + load. */
    if (init == NULL)
        return;
    LLVMValueRef rhs_val = llvm_emit_expression(init, ctx);
    if (rhs_val == NULL) {
        if (ctx != NULL && !ctx->has_error) {
            llvm_set_error_at_with_hints(ctx, init,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM destructuring let could not lower initializer expression");
        }
        return;
    }
    LLVMTypeRef rhs_ty = LLVMTypeOf(rhs_val);
    if (LLVMGetTypeKind(rhs_ty) != LLVMStructTypeKind) {
        llvm_set_error_with_hints(ctx, PGY_CODE_LLVM_TYPE_UNSUPPORTED, PGY_CAUSE_LLVM_TYPE_UNSUPPORTED, PGY_FIX_ANNOTATE_CONCRETE_TYPE, "destructuring requires an Array-like or tuple struct initializer");
        return;
    }

    /* Heuristic: tuple if struct field count equals the binding count
     * AND the first field is not a pointer (array-like has pointer as
     * the first field for `data`). */
    unsigned field_count = LLVMCountStructElementTypes(rhs_ty);
    bool is_tuple = false;
    if (field_count == (unsigned)binding_count) {
        LLVMTypeRef f0 = LLVMStructGetTypeAtIndex(rhs_ty, 0);
        if (f0 != NULL && LLVMGetTypeKind(f0) != LLVMPointerTypeKind)
            is_tuple = true;
    }

    if (is_tuple) {
        for (size_t i = 0; i < binding_count; i++) {
            const char *bname =
                llvm_destructure_binding_name(inst, node, i);
            if (bname == NULL) {
                llvm_set_error_at_with_hints(ctx, diagnostic_node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "LLVM destructuring let binding name metadata is missing");
                return;
            }
            LLVMTypeRef ft = LLVMStructGetTypeAtIndex(rhs_ty, (unsigned)i);
            LLVMValueRef v = LLVMBuildExtractValue(ctx->builder, rhs_val,
                (unsigned)i, llvm_tmp_name(ctx));
            LLVMValueRef alloca = llvm_create_entry_alloca(ctx, ft, bname);
            LLVMBuildStore(ctx->builder, v, alloca);
            llvm_scope_declare(ctx, pergyra_strdup(bname), alloca, ft);
        }
        return;
    }

    /* Array-like path (unchanged) */
    LLVMValueRef data_ptr = LLVMBuildExtractValue(ctx->builder,
        rhs_val, 0, llvm_tmp_name(ctx));
    LLVMTypeRef elem_type = llvm_stmt_resolve_array_elem_type(
        ctx, init, data_ptr);
    if (elem_type == NULL)
        return;
    for (size_t i = 0; i < binding_count; i++) {
        const char *bname = llvm_destructure_binding_name(inst, node, i);
        if (bname == NULL) {
            llvm_set_error_at_with_hints(ctx, diagnostic_node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM destructuring let binding name metadata is missing");
            return;
        }
        LLVMValueRef idx = LLVMConstInt(ctx->type_i64,
            (unsigned long long)i, 0);
        LLVMValueRef gep = LLVMBuildGEP2(ctx->builder,
            elem_type, data_ptr, &idx, 1, llvm_tmp_name(ctx));
        LLVMValueRef val = LLVMBuildLoad2(ctx->builder, elem_type,
            gep, llvm_tmp_name(ctx));
        LLVMValueRef alloca = llvm_create_entry_alloca(
            ctx, elem_type, bname);
        LLVMBuildStore(ctx->builder, val, alloca);
        llvm_scope_declare(ctx, pergyra_strdup(bname), alloca, elem_type);
    }
}

void
llvm_emit_let_destructure_stmt(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node == NULL || node->type != AST_LET_DESTRUCTURE)
        return;
    llvm_emit_destructure_parts(ast_let_destructure_initializer(node),
                                node,
                                NULL,
                                node,
                                ast_let_destructure_name_count(node),
                                ctx);
}

void
llvm_emit_mir_destructure_inst(const MIRInstruction *inst, LLVMGenCtx *ctx)
{
    if (inst == NULL || inst->kind != MIR_INST_DESTRUCTURE) {
        llvm_set_mir_topology_invalid(ctx,
            "LLVM MIR DESTRUCTURE instruction missing MIR destructure facts");
        return;
    }
    if (inst->expr0 == NULL
        || mir_instruction_destructure_binding_count(inst) == 0) {
        llvm_set_mir_topology_invalid(ctx,
            "LLVM MIR DESTRUCTURE instruction missing MIR destructure facts");
        return;
    }
    llvm_emit_destructure_parts(inst->expr0,
                                inst->expr0,
                                inst,
                                NULL,
                                mir_instruction_destructure_binding_count(inst),
                                ctx);
}
#endif /* PGY_LLVM_ENABLED */
