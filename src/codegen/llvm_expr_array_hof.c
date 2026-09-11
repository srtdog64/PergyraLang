#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_array_hof.h"

#include <stdio.h>
#include <string.h>

#include "llvm_internal_api.h"
#include "parser/ast_api.h"

typedef struct {
    LLVMTypeRef    arr_struct_ty;
    LLVMTypeRef    elem_type;
    LLVMValueRef   data_ptr;
    LLVMValueRef   len;
    LLVMValueRef   result_alloca;
    LLVMFuncEntry *push_fn;
    LLVMFuncEntry *map_fn;
} LLVMArrayHofPlan;

static bool
llvm_array_hof_error_out(ASTNode *node, LLVMGenCtx *ctx,
                         const char *message, LLVMValueRef *out)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "%s",
            message != NULL ? message
                : "LLVM array map/filter could not be lowered");
    }
    if (out != NULL)
        *out = NULL;
    return true;
}

/* Resolve "<prefix>_<suffix>" in the array runtime registry. */
static LLVMFuncEntry *
llvm_array_hof_runtime_function(LLVMGenCtx *ctx, ASTNode *node,
                                const char *callee_name, const char *prefix,
                                const char *suffix)
{
    char fn_name[64];
    int written = snprintf(fn_name, sizeof(fn_name), "%s_%s", prefix, suffix);

    if (written < 0 || (size_t)written >= sizeof(fn_name)) {
        llvm_array_hof_error_out(node, ctx,
            "LLVM array map/filter runtime function name is too long", NULL);
        return NULL;
    }
    return llvm_required_runtime_function(ctx, node, "array", callee_name,
        fn_name);
}

/* Resolve the by-name function argument of ArrayMap/ArrayFilter. */
static LLVMFuncEntry *
llvm_array_hof_function_arg(LLVMGenCtx *ctx, ASTNode *node,
                           const char *callee_name)
{
    ASTNode *fn_arg = ast_call_argument(node, 1);
    (void)callee_name;
    if (fn_arg == NULL || fn_arg->type != AST_IDENTIFIER
        || ast_identifier_name(fn_arg) == NULL) {
        llvm_array_hof_error_out(node, ctx,
            "LLVM array map/filter requires a named function argument", NULL);
        return NULL;
    }
    LLVMFuncEntry *fn = llvm_lookup_function(ctx, ast_identifier_name(fn_arg));
    if (fn == NULL)
        llvm_array_hof_error_out(node, ctx,
            "LLVM array map/filter argument is not a known function", NULL);
    return fn;
}

/* Load the source array element pointer (field 0) and length (field 1). */
static bool
llvm_array_hof_load_source(LLVMGenCtx *ctx, LLVMValueRef arr_alloca,
                           const char *suffix, LLVMTypeRef elem_type,
                           LLVMArrayHofPlan *plan)
{
    plan->arr_struct_ty = llvm_array_struct_type(ctx, suffix);
    if (plan->arr_struct_ty == NULL)
        return false;
    plan->elem_type = elem_type;
    LLVMValueRef data_gep = LLVMBuildStructGEP2(ctx->builder,
        plan->arr_struct_ty, arr_alloca, 0, llvm_tmp_name(ctx));
    plan->data_ptr = LLVMBuildLoad2(ctx->builder,
        LLVMPointerType(elem_type, 0), data_gep, llvm_tmp_name(ctx));
    LLVMValueRef len_gep = LLVMBuildStructGEP2(ctx->builder,
        plan->arr_struct_ty, arr_alloca, 1, llvm_tmp_name(ctx));
    plan->len = LLVMBuildLoad2(ctx->builder, ctx->type_i64, len_gep,
        llvm_tmp_name(ctx));
    return true;
}

/* Allocate the result Array<T> and resolve the per-suffix push runtime. */
static bool
llvm_array_hof_make_result(LLVMGenCtx *ctx, ASTNode *node,
                           const char *callee_name, const char *suffix,
                           LLVMArrayHofPlan *plan)
{
    LLVMFuncEntry *new_fn = llvm_array_hof_runtime_function(ctx, node,
        callee_name, "pgy_array_new", suffix);
    if (new_fn == NULL)
        return false;
    plan->result_alloca = llvm_create_entry_alloca(ctx,
        plan->arr_struct_ty, llvm_tmp_name(ctx));
    LLVMValueRef nargs[] = { plan->len };
    LLVMValueRef result_val = LLVMBuildCall2(ctx->builder, new_fn->fn_type,
        new_fn->fn, nargs, 1, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, result_val, plan->result_alloca);
    plan->push_fn = llvm_array_hof_runtime_function(ctx, node, callee_name,
        "pgy_array_push", suffix);
    return plan->push_fn != NULL;
}

/* Map body: apply map_fn to elem and push the mapped value. */
static void
llvm_array_hof_body_map(LLVMGenCtx *ctx, LLVMArrayHofPlan *plan,
                        LLVMValueRef elem)
{
    LLVMValueRef cargs[] = { elem };
    LLVMValueRef mapped = LLVMBuildCall2(ctx->builder, plan->map_fn->fn_type,
        plan->map_fn->fn, cargs, 1, llvm_tmp_name(ctx));
    LLVMValueRef pargs[] = { plan->result_alloca, mapped };
    LLVMBuildCall2(ctx->builder, plan->push_fn->fn_type, plan->push_fn->fn,
        pargs, 2, "");
}

/* Filter body: push elem only when the predicate returns nonzero. */
static void
llvm_array_hof_body_filter(LLVMGenCtx *ctx, LLVMArrayHofPlan *plan,
                           LLVMValueRef elem, LLVMBasicBlockRef incr_bb)
{
    LLVMValueRef cargs[] = { elem };
    LLVMValueRef keep = LLVMBuildCall2(ctx->builder, plan->map_fn->fn_type,
        plan->map_fn->fn, cargs, 1, llvm_tmp_name(ctx));
    LLVMValueRef zero = LLVMConstInt(LLVMTypeOf(keep), 0, 0);
    LLVMValueRef cond = LLVMBuildICmp(ctx->builder, LLVMIntNE, keep, zero,
        llvm_tmp_name(ctx));
    LLVMValueRef fn0 = LLVMGetBasicBlockParent(LLVMGetInsertBlock(ctx->builder));
    LLVMBasicBlockRef push_bb = LLVMAppendBasicBlockInContext(ctx->context,
        fn0, "hof.push");
    LLVMBuildCondBr(ctx->builder, cond, push_bb, incr_bb);
    LLVMPositionBuilderAtEnd(ctx->builder, push_bb);
    LLVMValueRef pargs[] = { plan->result_alloca, elem };
    LLVMBuildCall2(ctx->builder, plan->push_fn->fn_type, plan->push_fn->fn,
        pargs, 2, "");
    LLVMBuildBr(ctx->builder, incr_bb);
}

/* Dispatch the per-element body and ensure control reaches incr_bb. */
static void
llvm_array_hof_emit_body(LLVMGenCtx *ctx, LLVMArrayHofPlan *plan,
                         int is_filter, LLVMValueRef elem,
                         LLVMBasicBlockRef incr_bb)
{
    if (!is_filter) {
        llvm_array_hof_body_map(ctx, plan, elem);
        LLVMBuildBr(ctx->builder, incr_bb);
        return;
    }
    llvm_array_hof_body_filter(ctx, plan, elem, incr_bb);
}

/* Emit the counted loop that drives map/filter over the source array.
 * The body is a linear IR sequence, so this stays one cohesive routine. */
static bool
llvm_array_hof_emit_loop(LLVMGenCtx *ctx, LLVMArrayHofPlan *plan,
                         int is_filter, LLVMValueRef *out)
{
    LLVMValueRef i_alloca = llvm_create_entry_alloca(ctx, ctx->type_i64,
        llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i64, 0, 0), i_alloca);
    LLVMValueRef fn0 = LLVMGetBasicBlockParent(LLVMGetInsertBlock(ctx->builder));
    LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlockInContext(ctx->context, fn0, "hof.cond");
    LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(ctx->context, fn0, "hof.body");
    LLVMBasicBlockRef incr_bb = LLVMAppendBasicBlockInContext(ctx->context, fn0, "hof.incr");
    LLVMBasicBlockRef exit_bb = LLVMAppendBasicBlockInContext(ctx->context, fn0, "hof.exit");
    LLVMBuildBr(ctx->builder, cond_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, cond_bb);
    LLVMValueRef ci = LLVMBuildLoad2(ctx->builder, ctx->type_i64, i_alloca, llvm_tmp_name(ctx));
    LLVMValueRef cond = LLVMBuildICmp(ctx->builder, LLVMIntULT, ci, plan->len, llvm_tmp_name(ctx));
    LLVMBuildCondBr(ctx->builder, cond, body_bb, exit_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, body_bb);
    LLVMValueRef bi = LLVMBuildLoad2(ctx->builder, ctx->type_i64, i_alloca, llvm_tmp_name(ctx));
    LLVMValueRef idxs[] = { bi };
    LLVMValueRef elem_ptr = LLVMBuildInBoundsGEP2(ctx->builder, plan->elem_type,
        plan->data_ptr, idxs, 1, llvm_tmp_name(ctx));
    LLVMValueRef elem = LLVMBuildLoad2(ctx->builder, plan->elem_type, elem_ptr, llvm_tmp_name(ctx));
    llvm_array_hof_emit_body(ctx, plan, is_filter, elem, incr_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, incr_bb);
    LLVMValueRef ii = LLVMBuildLoad2(ctx->builder, ctx->type_i64, i_alloca, llvm_tmp_name(ctx));
    LLVMValueRef next = LLVMBuildAdd(ctx->builder, ii,
        LLVMConstInt(ctx->type_i64, 1, 0), llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, next, i_alloca);
    LLVMBuildBr(ctx->builder, cond_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, exit_bb);
    *out = LLVMBuildLoad2(ctx->builder, plan->arr_struct_ty,
        plan->result_alloca, llvm_tmp_name(ctx));
    return true;
}

/* ArrayMap/ArrayFilter: build a new Array<T> by running the named function
 * over each source element, mirroring the C backend's inlined loop. */
bool
llvm_array_emit_hof(ASTNode *node, LLVMGenCtx *ctx, const char *callee_name,
                    LLVMValueRef arr_alloca, LLVMArrayVarEntry *entry,
                    const char *suffix, int is_filter, LLVMValueRef *out)
{
    LLVMArrayHofPlan plan;

    if (ctx == NULL || entry == NULL || arr_alloca == NULL || suffix == NULL)
        return llvm_array_hof_error_out(node, ctx,
            "LLVM array map/filter receiver facts are incomplete", out);
    memset(&plan, 0, sizeof(plan));
    plan.map_fn = llvm_array_hof_function_arg(ctx, node, callee_name);
    if (plan.map_fn == NULL)
        return true;
    if (!llvm_array_hof_load_source(ctx, arr_alloca, suffix, entry->elem_type,
            &plan))
        return llvm_array_hof_error_out(node, ctx,
            "LLVM array map/filter cannot resolve Array<T> struct type", out);
    if (!llvm_array_hof_make_result(ctx, node, callee_name, suffix, &plan))
        return true;
    return llvm_array_hof_emit_loop(ctx, &plan, is_filter, out);
}

#endif /* PGY_LLVM_ENABLED */
