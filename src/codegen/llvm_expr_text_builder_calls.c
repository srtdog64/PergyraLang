#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_text_builder_calls.h"

#include "../compiler/mir_abi_layout.h"

#include <string.h>

static bool
llvm_text_builder_named_local(ASTNode *node, LLVMGenCtx *ctx,
                              LLVMTypeRef expected, LLVMValueRef *out)
{
    LLVMVarEntry var;

    if (node == NULL || node->type != AST_IDENTIFIER
        || ast_identifier_name(node) == NULL
        || !llvm_scope_lookup_snapshot(ctx, ast_identifier_name(node), &var)
        || var.type != expected) {
        return false;
    }
    *out = var.alloca;
    return true;
}

static bool
llvm_text_builder_error(ASTNode *node, LLVMGenCtx *ctx,
                        const char *callee_name, LLVMValueRef *out)
{
    llvm_set_error_at_with_hints(ctx, node,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_MATCH_BUILTIN_SIGNATURE,
        "LLVM TextBuilder builtin '%s' requires MIR-owned ABI facts and named owner locals",
        callee_name != NULL ? callee_name : "<text-builder>");
    *out = NULL;
    return true;
}

bool
llvm_emit_text_builder_builtin_call(ASTNode *node, LLVMGenCtx *ctx,
                                    const char *callee_name,
                                    LLVMValueRef *out)
{
    const MIRInstruction *inst = ctx != NULL
        ? ctx->current_mir_instruction : NULL;
    const MIRTextBuilderRuntimeRow *declared =
        mir_text_builder_runtime_row_by_source_name(callee_name);
    const MIRTextBuilderRuntimeRow *row = inst != NULL
        ? inst->text_builder_runtime_row : NULL;
    LLVMFuncEntry *fn;
    LLVMValueRef args[3] = { NULL, NULL, NULL };
    LLVMValueRef storage;

    if (out == NULL)
        return false;
    *out = NULL;
    if (declared == NULL)
        return false;
    if (row != declared || row->source_name == NULL || callee_name == NULL
        || strcmp(row->source_name, callee_name) != 0)
        return llvm_text_builder_error(node, ctx, callee_name, out);
    fn = llvm_required_runtime_function(ctx, node,
        "text-builder", callee_name, row->llvm_export_fn);
    if (fn == NULL)
        return true;

    if (row->llvm_call_shape
        == MIR_TEXT_BUILDER_CALL_OUT_CAPACITY_TO_VOID) {
        LLVMValueRef capacity;
        capacity = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        if (capacity == NULL)
            return llvm_text_builder_error(node, ctx, callee_name, out);
        if (LLVMTypeOf(capacity) != ctx->type_i64)
            capacity = LLVMBuildSExtOrBitCast(ctx->builder, capacity,
                ctx->type_i64, llvm_tmp_name(ctx));
        storage = llvm_create_entry_alloca(ctx, ctx->type_text_builder,
            llvm_tmp_name(ctx));
        args[0] = storage;
        args[1] = capacity;
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        *out = LLVMBuildLoad2(ctx->builder, ctx->type_text_builder,
            storage, llvm_tmp_name(ctx));
        return true;
    }

    if (!llvm_text_builder_named_local(ast_call_argument(node, 0), ctx,
            ctx->type_text_builder, &args[0]))
        return llvm_text_builder_error(node, ctx, callee_name, out);
    if (row->llvm_call_shape
        == MIR_TEXT_BUILDER_CALL_BUILDER_STRING_TO_VOID) {
        args[1] = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        if (args[1] == NULL || LLVMTypeOf(args[1]) != ctx->type_i8ptr)
            return llvm_text_builder_error(node, ctx, callee_name, out);
        *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
    } else if (row->llvm_call_shape
               == MIR_TEXT_BUILDER_CALL_BUILDER_ALLOCATOR_TO_STRING) {
        if (!llvm_text_builder_named_local(ast_call_argument(node, 1), ctx,
                ctx->type_allocator, &args[1]))
            return llvm_text_builder_error(node, ctx, callee_name, out);
        *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2,
            llvm_tmp_name(ctx));
    } else if (row->llvm_call_shape
               == MIR_TEXT_BUILDER_CALL_BUILDER_TO_VOID) {
        *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
    } else {
        return llvm_text_builder_error(node, ctx, callee_name, out);
    }
    return true;
}

#endif
