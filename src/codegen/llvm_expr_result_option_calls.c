#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "codegen_match_variant_policy.h"

#include <stdlib.h>
#include <string.h>

typedef enum LLVMResultOptionOp {
    LLVM_RESULT_OPTION_OP_NONE = 0,
    LLVM_RESULT_OPTION_OP_ERR,
    LLVM_RESULT_OPTION_OP_IS_ERR,
    LLVM_RESULT_OPTION_OP_IS_NONE,
    LLVM_RESULT_OPTION_OP_IS_OK,
    LLVM_RESULT_OPTION_OP_IS_SOME,
    LLVM_RESULT_OPTION_OP_NONE_VALUE,
    LLVM_RESULT_OPTION_OP_OK,
    LLVM_RESULT_OPTION_OP_SOME,
    LLVM_RESULT_OPTION_OP_UNWRAP,
    LLVM_RESULT_OPTION_OP_UNWRAP_OPTION,
    LLVM_RESULT_OPTION_OP_UNWRAP_OR,
} LLVMResultOptionOp;

typedef struct LLVMResultOptionSpec {
    const char *name;
    size_t argc;
    LLVMResultOptionOp op;
} LLVMResultOptionSpec;

static int
llvm_result_option_spec_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const LLVMResultOptionSpec *spec = (const LLVMResultOptionSpec *)entry;

    return strcmp(name, spec->name);
}

static LLVMResultOptionOp
llvm_result_option_lookup(const char *callee_name, size_t argc)
{
    static const LLVMResultOptionSpec kLLVMResultOptionSpecs[] = {
        { "IsErr", 1, LLVM_RESULT_OPTION_OP_IS_ERR },
        { "IsNone", 1, LLVM_RESULT_OPTION_OP_IS_NONE },
        { "IsOk", 1, LLVM_RESULT_OPTION_OP_IS_OK },
        { "IsSome", 1, LLVM_RESULT_OPTION_OP_IS_SOME },
        { "Unwrap", 1, LLVM_RESULT_OPTION_OP_UNWRAP },
        { "UnwrapOption", 1, LLVM_RESULT_OPTION_OP_UNWRAP_OPTION },
        { "UnwrapOr", 2, LLVM_RESULT_OPTION_OP_UNWRAP_OR },
    };
    const LLVMResultOptionSpec *match;
    PgyCodegenMatchVariantKind variant_kind;

    if (callee_name == NULL)
        return LLVM_RESULT_OPTION_OP_NONE;
    variant_kind = pgy_codegen_match_variant_lookup(callee_name);
    if (variant_kind == PGY_MATCH_VARIANT_SOME)
        return argc == 1 ? LLVM_RESULT_OPTION_OP_SOME
                         : LLVM_RESULT_OPTION_OP_NONE;
    if (variant_kind == PGY_MATCH_VARIANT_NONE_CTOR)
        return argc == 0 ? LLVM_RESULT_OPTION_OP_NONE_VALUE
                         : LLVM_RESULT_OPTION_OP_NONE;
    if (variant_kind == PGY_MATCH_VARIANT_OK)
        return argc == 1 ? LLVM_RESULT_OPTION_OP_OK
                         : LLVM_RESULT_OPTION_OP_NONE;
    if (variant_kind == PGY_MATCH_VARIANT_ERR)
        return argc == 1 ? LLVM_RESULT_OPTION_OP_ERR
                         : LLVM_RESULT_OPTION_OP_NONE;

    match = (const LLVMResultOptionSpec *)bsearch(&callee_name,
        kLLVMResultOptionSpecs,
        sizeof(kLLVMResultOptionSpecs) / sizeof(kLLVMResultOptionSpecs[0]),
        sizeof(kLLVMResultOptionSpecs[0]), llvm_result_option_spec_compare);
    if (match == NULL || match->argc != argc)
        return LLVM_RESULT_OPTION_OP_NONE;
    return match->op;
}

static LLVMValueRef
llvm_result_option_error(LLVMGenCtx *ctx, ASTNode *node,
                         const char *message)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "%s", message != NULL ? message
                : "LLVM Result/Option lowering requires concrete metadata");
    }
    return NULL;
}

static LLVMValueRef
llvm_emit_checked_result_option_unwrap(LLVMGenCtx *ctx, ASTNode *node,
                                       LLVMValueRef aggregate,
                                       unsigned value_index,
                                       const char *reason)
{
    LLVMValueRef tag;
    LLVMValueRef ok;
    LLVMValueRef current_fn;
    LLVMFuncEntry *panic_fn;
    LLVMBasicBlockRef ok_bb;
    LLVMBasicBlockRef fail_bb;

    if (ctx == NULL || aggregate == NULL)
        return NULL;

    tag = LLVMBuildExtractValue(ctx->builder, aggregate, 0, llvm_tmp_name(ctx));
    ok = LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
        LLVMConstInt(ctx->type_i32, 0, 0), llvm_tmp_name(ctx));
    current_fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(ctx->builder));
    if (current_fn == NULL)
        return llvm_result_option_error(ctx, node,
            "LLVM checked unwrap requires an active function insertion block");
    panic_fn = llvm_lookup_function(ctx,
        "pgy_runtime_panic_internal_invariant_export");
    if (panic_fn == NULL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM checked unwrap requires registered runtime function '%s'",
            "pgy_runtime_panic_internal_invariant_export");
        return NULL;
    }

    ok_bb = LLVMAppendBasicBlockInContext(ctx->context, current_fn,
        "unwrap.ok");
    fail_bb = LLVMAppendBasicBlockInContext(ctx->context, current_fn,
        "unwrap.panic");
    LLVMBuildCondBr(ctx->builder, ok, ok_bb, fail_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, fail_bb);
    {
        LLVMValueRef reason_arg = LLVMBuildGlobalStringPtr(ctx->builder,
            reason != NULL ? reason : "runtime invariant failed",
            llvm_tmp_name(ctx));
        LLVMBuildCall2(ctx->builder, panic_fn->fn_type, panic_fn->fn,
            &reason_arg, 1, "");
    }
    LLVMBuildUnreachable(ctx->builder);

    LLVMPositionBuilderAtEnd(ctx->builder, ok_bb);
    return LLVMBuildExtractValue(ctx->builder, aggregate, value_index,
        llvm_tmp_name(ctx));
}

static LLVMTypeRef
llvm_result_option_context_type(LLVMGenCtx *ctx, unsigned field_count,
                                LLVMTypeRef *fields_out)
{
    LLVMTypeRef candidate = NULL;

    if (ctx == NULL)
        return NULL;

    if (ctx->expected_type_name != NULL)
        candidate = pergyra_type_to_llvm(ctx, ctx->expected_type_name);
    if (candidate != NULL
        && LLVMGetTypeKind(candidate) == LLVMStructTypeKind
        && LLVMCountStructElementTypes(candidate) == field_count) {
        if (fields_out != NULL)
            LLVMGetStructElementTypes(candidate, fields_out);
        return candidate;
    }

    candidate = ctx->current_ret_type;
    if (candidate == NULL
        || LLVMGetTypeKind(candidate) != LLVMStructTypeKind
        || LLVMCountStructElementTypes(candidate) != field_count) {
        return NULL;
    }
    if (fields_out != NULL)
        LLVMGetStructElementTypes(candidate, fields_out);
    return candidate;
}

static bool
llvm_result_option_value_struct(LLVMValueRef aggregate, unsigned field_count,
                                LLVMTypeRef *fields_out)
{
    LLVMTypeRef aggregate_ty;

    if (aggregate == NULL)
        return false;
    aggregate_ty = LLVMTypeOf(aggregate);
    if (aggregate_ty == NULL
        || LLVMGetTypeKind(aggregate_ty) != LLVMStructTypeKind)
        return false;
    if (LLVMCountStructElementTypes(aggregate_ty) != field_count)
        return false;
    if (fields_out != NULL)
        LLVMGetStructElementTypes(aggregate_ty, fields_out);
    return true;
}

static LLVMValueRef
llvm_coerce_result_option_payload(LLVMGenCtx *ctx,
                                  LLVMValueRef val,
                                  LLVMTypeRef target_ty)
{
    LLVMTypeRef val_ty;

    if (ctx == NULL || val == NULL || target_ty == NULL)
        return val;
    val_ty = LLVMTypeOf(val);
    if (val_ty == target_ty)
        return val;
    if ((target_ty == ctx->type_i32 || target_ty == ctx->type_i64)
        && (val_ty == ctx->type_i32 || val_ty == ctx->type_i64)) {
        return (LLVMGetIntTypeWidth(target_ty) > LLVMGetIntTypeWidth(val_ty))
            ? LLVMBuildSExt(ctx->builder, val, target_ty, llvm_tmp_name(ctx))
            : LLVMBuildTrunc(ctx->builder, val, target_ty, llvm_tmp_name(ctx));
    }
    if ((target_ty == ctx->type_f32 || target_ty == ctx->type_f64)
        && (val_ty == ctx->type_i32 || val_ty == ctx->type_i64))
        return LLVMBuildSIToFP(ctx->builder, val, target_ty, llvm_tmp_name(ctx));
    if ((target_ty == ctx->type_i32 || target_ty == ctx->type_i64)
        && (val_ty == ctx->type_f32 || val_ty == ctx->type_f64))
        return LLVMBuildFPToSI(ctx->builder, val, target_ty, llvm_tmp_name(ctx));
    if (LLVMGetTypeKind(target_ty) == LLVMPointerTypeKind
        && LLVMGetTypeKind(val_ty) == LLVMPointerTypeKind)
        return LLVMBuildBitCast(ctx->builder, val, target_ty, llvm_tmp_name(ctx));
    return val;
}

LLVMValueRef
llvm_emit_result_option_call(ASTNode *node, LLVMGenCtx *ctx, const char *callee_name)
{
    /* Built-in: Ok(value) -> Result<T, E>; prefer active expected layout. */
    size_t argc = ast_call_arg_count(node);
    LLVMResultOptionOp op = llvm_result_option_lookup(callee_name, argc);

    if (op == LLVM_RESULT_OPTION_OP_OK) {
        LLVMValueRef val;
        LLVMTypeRef result_ty = NULL;
        LLVMTypeRef fields[3];
        result_ty = llvm_result_option_context_type(ctx, 3, fields);
        if (result_ty == NULL) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM Ok(value) requires contextual Result<T, E>; anonymous Result layout fallback is disabled");
            return NULL;
        }
        val = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        if (val == NULL)
            return llvm_result_option_error(ctx, node,
                "LLVM Ok(value) could not lower payload expression");
        LLVMValueRef r = LLVMGetUndef(result_ty);
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstInt(ctx->type_i32, 0, 0), 0, llvm_tmp_name(ctx));
        if (LLVMTypeOf(val) != fields[1]) {
            LLVMTypeRef val_ty = LLVMTypeOf(val);
            if ((fields[1] == ctx->type_i32 || fields[1] == ctx->type_i64)
                && (val_ty == ctx->type_i32 || val_ty == ctx->type_i64)) {
                val = (LLVMGetIntTypeWidth(fields[1]) > LLVMGetIntTypeWidth(val_ty))
                    ? LLVMBuildSExt(ctx->builder, val, fields[1], llvm_tmp_name(ctx))
                    : LLVMBuildTrunc(ctx->builder, val, fields[1], llvm_tmp_name(ctx));
            } else if ((fields[1] == ctx->type_f32 || fields[1] == ctx->type_f64)
                       && (val_ty == ctx->type_i32 || val_ty == ctx->type_i64)) {
                val = LLVMBuildSIToFP(ctx->builder, val, fields[1], llvm_tmp_name(ctx));
            } else if ((fields[1] == ctx->type_i32 || fields[1] == ctx->type_i64)
                       && (val_ty == ctx->type_f32 || val_ty == ctx->type_f64)) {
                val = LLVMBuildFPToSI(ctx->builder, val, fields[1], llvm_tmp_name(ctx));
            } else if (LLVMGetTypeKind(fields[1]) == LLVMPointerTypeKind
                       && LLVMGetTypeKind(val_ty) == LLVMPointerTypeKind) {
                val = LLVMBuildBitCast(ctx->builder, val, fields[1], llvm_tmp_name(ctx));
            }
        }
        r = LLVMBuildInsertValue(ctx->builder, r, val, 1, llvm_tmp_name(ctx));
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstNull(fields[2]), 2, llvm_tmp_name(ctx));
        return r;
    }

    /* Built-in: Err(value) -> Result<T, E>; prefer active expected layout. */
    if (op == LLVM_RESULT_OPTION_OP_ERR) {
        LLVMValueRef val;
        LLVMTypeRef result_ty = NULL;
        LLVMTypeRef fields[3];
        result_ty = llvm_result_option_context_type(ctx, 3, fields);
        if (result_ty == NULL) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM Err(value) requires contextual Result<T, E>; anonymous Result layout fallback is disabled");
            return NULL;
        }
        val = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        if (val == NULL)
            return llvm_result_option_error(ctx, node,
                "LLVM Err(value) could not lower payload expression");
        LLVMValueRef r = LLVMGetUndef(result_ty);
        if (LLVMTypeOf(val) != fields[2]) {
            LLVMTypeRef val_ty = LLVMTypeOf(val);
            if ((fields[2] == ctx->type_i32 || fields[2] == ctx->type_i64)
                && (val_ty == ctx->type_i32 || val_ty == ctx->type_i64)) {
                val = (LLVMGetIntTypeWidth(fields[2]) > LLVMGetIntTypeWidth(val_ty))
                    ? LLVMBuildSExt(ctx->builder, val, fields[2], llvm_tmp_name(ctx))
                    : LLVMBuildTrunc(ctx->builder, val, fields[2], llvm_tmp_name(ctx));
            } else if ((fields[2] == ctx->type_f32 || fields[2] == ctx->type_f64)
                       && (val_ty == ctx->type_i32 || val_ty == ctx->type_i64)) {
                val = LLVMBuildSIToFP(ctx->builder, val, fields[2], llvm_tmp_name(ctx));
            } else if ((fields[2] == ctx->type_i32 || fields[2] == ctx->type_i64)
                       && (val_ty == ctx->type_f32 || val_ty == ctx->type_f64)) {
                val = LLVMBuildFPToSI(ctx->builder, val, fields[2], llvm_tmp_name(ctx));
            } else if (LLVMGetTypeKind(fields[2]) == LLVMPointerTypeKind
                       && LLVMGetTypeKind(val_ty) == LLVMPointerTypeKind) {
                val = LLVMBuildBitCast(ctx->builder, val, fields[2], llvm_tmp_name(ctx));
            }
        }
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstInt(ctx->type_i32, 1, 0), 0, llvm_tmp_name(ctx));
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstNull(fields[1]), 1, llvm_tmp_name(ctx));
        r = LLVMBuildInsertValue(ctx->builder, r, val, 2, llvm_tmp_name(ctx));
        return r;
    }

    /* Built-in: IsOk(result) extracts the ok field. */
    if (op == LLVM_RESULT_OPTION_OP_IS_OK) {
        LLVMValueRef r = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        LLVMTypeRef fields[3];
        if (!llvm_result_option_value_struct(r, 3, fields)
            || fields[0] != ctx->type_i32) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM IsOk(result) requires concrete Result<T, E> aggregate operand");
            return NULL;
        }
        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, r, 0, llvm_tmp_name(ctx));
        return LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32, 0, 0), llvm_tmp_name(ctx));
    }

    /* Built-in: IsErr(result) checks !ok. */
    if (op == LLVM_RESULT_OPTION_OP_IS_ERR) {
        LLVMValueRef r = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        LLVMTypeRef fields[3];
        if (!llvm_result_option_value_struct(r, 3, fields)
            || fields[0] != ctx->type_i32) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM IsErr(result) requires concrete Result<T, E> aggregate operand");
            return NULL;
        }
        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, r, 0, llvm_tmp_name(ctx));
        return LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32, 1, 0), llvm_tmp_name(ctx));
    }

    /* Built-in: Unwrap(result) extracts the value field. */
    if (op == LLVM_RESULT_OPTION_OP_UNWRAP) {
        LLVMValueRef r = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        LLVMTypeRef fields[3];
        if (!llvm_result_option_value_struct(r, 3, fields)
            || fields[0] != ctx->type_i32) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM Unwrap(result) requires concrete Result<T, E> aggregate operand");
            return NULL;
        }
        return llvm_emit_checked_result_option_unwrap(ctx, node, r, 1,
            "Result unwrap on Err value");
    }

    /* Built-in: UnwrapOr(result, default) emits ok ? value : default. */
    if (op == LLVM_RESULT_OPTION_OP_UNWRAP_OR) {
        LLVMValueRef r = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        LLVMValueRef def = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        LLVMTypeRef fields[3];
        if (r == NULL || def == NULL)
            return llvm_result_option_error(ctx, node,
                "LLVM UnwrapOr(result, default) could not lower operand expression");
        if (!llvm_result_option_value_struct(r, 3, fields)
            || fields[0] != ctx->type_i32) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM UnwrapOr(result, default) requires concrete Result<T, E> aggregate operand");
            return NULL;
        }
        def = llvm_coerce_result_option_payload(ctx, def, fields[1]);
        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, r, 0, llvm_tmp_name(ctx));
        LLVMValueRef ok = LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32, 0, 0), llvm_tmp_name(ctx));
        LLVMValueRef val = LLVMBuildExtractValue(ctx->builder, r, 1, llvm_tmp_name(ctx));
        return LLVMBuildSelect(ctx->builder, ok, val, def, llvm_tmp_name(ctx));
    }

    /* Built-in: Some(value) creates the active Option<T> some-tag payload. */
    if (op == LLVM_RESULT_OPTION_OP_SOME) {
        LLVMValueRef val;
        LLVMTypeRef option_ty = NULL;
        LLVMTypeRef fields[2];
        option_ty = llvm_result_option_context_type(ctx, 2, fields);
        if (option_ty == NULL || fields[0] != ctx->type_i32) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM Some(value) requires contextual Option<T>; anonymous Option layout fallback is disabled");
            return NULL;
        }
        val = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        if (val == NULL)
            return llvm_result_option_error(ctx, node,
                "LLVM Some(value) could not lower payload expression");
        val = llvm_coerce_result_option_payload(ctx, val, fields[1]);
        LLVMValueRef o = LLVMGetUndef(option_ty);
        o = LLVMBuildInsertValue(ctx->builder, o,
            LLVMConstInt(ctx->type_i32, 0, 0), 0, llvm_tmp_name(ctx));
        o = LLVMBuildInsertValue(ctx->builder, o, val, 1, llvm_tmp_name(ctx));
        return o;
    }

    /* Built-in: None() creates the active Option<T> none-tag payload. */
    if (op == LLVM_RESULT_OPTION_OP_NONE_VALUE) {
        LLVMTypeRef fields[2];
        LLVMTypeRef option_ty = llvm_result_option_context_type(ctx, 2, fields);
        if (option_ty == NULL || fields[0] != ctx->type_i32) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM None() requires contextual Option<T>; Option<Int> fallback is disabled");
            return NULL;
        }
        LLVMTypeRef value_ty = fields[1];
        LLVMValueRef o = LLVMGetUndef(option_ty);
        o = LLVMBuildInsertValue(ctx->builder, o,
            LLVMConstInt(ctx->type_i32, 1, 0), 0, llvm_tmp_name(ctx));
        o = LLVMBuildInsertValue(ctx->builder, o,
            LLVMConstNull(value_ty), 1, llvm_tmp_name(ctx));
        return o;
    }

    /* Built-in: IsSome(option) / IsNone(option) */
    if (op == LLVM_RESULT_OPTION_OP_IS_SOME
        || op == LLVM_RESULT_OPTION_OP_IS_NONE) {
        LLVMValueRef o = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        LLVMTypeRef fields[2];
        if (!llvm_result_option_value_struct(o, 2, fields)
            || fields[0] != ctx->type_i32) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM %s(option) requires concrete Option<T> aggregate operand",
                callee_name);
            return NULL;
        }
        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, o, 0, llvm_tmp_name(ctx));
        return LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32,
                op == LLVM_RESULT_OPTION_OP_IS_SOME ? 0 : 1, 0),
            llvm_tmp_name(ctx));
    }

    /* Built-in: UnwrapOption(option) extracts the value field. */
    if (op == LLVM_RESULT_OPTION_OP_UNWRAP_OPTION) {
        LLVMValueRef o = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        LLVMTypeRef fields[2];
        if (!llvm_result_option_value_struct(o, 2, fields)
            || fields[0] != ctx->type_i32) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM UnwrapOption(option) requires concrete Option<T> aggregate operand");
            return NULL;
        }
        return llvm_emit_checked_result_option_unwrap(ctx, node, o, 1,
            "Option unwrap on None value");
    }

    return NULL;
}

#endif /* PGY_LLVM_ENABLED */
