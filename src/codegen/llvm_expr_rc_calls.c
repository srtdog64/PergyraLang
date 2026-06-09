#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

#include <stdlib.h>
#include <string.h>

typedef enum LLVMRcOp {
    LLVM_RC_OP_NONE = 0,
    LLVM_RC_OP_RC_CLONE,
    LLVM_RC_OP_RC_DOWNGRADE,
    LLVM_RC_OP_RC_DROP,
    LLVM_RC_OP_RC_GET,
    LLVM_RC_OP_RC_NEW,
    LLVM_RC_OP_WEAK_DROP,
    LLVM_RC_OP_WEAK_UPGRADE,
} LLVMRcOp;

typedef struct LLVMRcSpec {
    const char *name;
    LLVMRcOp op;
} LLVMRcSpec;

static int
llvm_rc_spec_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const LLVMRcSpec *spec = (const LLVMRcSpec *)entry;

    return strcmp(name, spec->name);
}

static LLVMRcOp
llvm_rc_lookup(const char *callee_name)
{
    static const LLVMRcSpec kLLVMRcSpecs[] = {
        { "RcClone", LLVM_RC_OP_RC_CLONE },
        { "RcDowngrade", LLVM_RC_OP_RC_DOWNGRADE },
        { "RcDrop", LLVM_RC_OP_RC_DROP },
        { "RcGet", LLVM_RC_OP_RC_GET },
        { "RcNew", LLVM_RC_OP_RC_NEW },
        { "WeakDrop", LLVM_RC_OP_WEAK_DROP },
        { "WeakUpgrade", LLVM_RC_OP_WEAK_UPGRADE },
    };
    const LLVMRcSpec *match;

    if (callee_name == NULL)
        return LLVM_RC_OP_NONE;

    match = (const LLVMRcSpec *)bsearch(&callee_name, kLLVMRcSpecs,
        sizeof(kLLVMRcSpecs) / sizeof(kLLVMRcSpecs[0]),
        sizeof(kLLVMRcSpecs[0]), llvm_rc_spec_compare);
    return match != NULL ? match->op : LLVM_RC_OP_NONE;
}

static const char *
llvm_rc_suffix_from_inner(LLVMGenCtx *ctx, const char *inner)
{
    if (inner == NULL)
        return NULL;
    LLVMTypeRef ty = pergyra_type_to_llvm(ctx, inner);
    if (ctx->has_error || ty == NULL)
        return NULL;
    const char *suffix = llvm_type_to_suffix(ctx, ty);
    if (suffix == NULL || strcmp(suffix, "Unknown") == 0)
        return NULL;
    return suffix;
}

static bool
llvm_rc_expected_inner_copy(LLVMGenCtx *ctx, char *out, size_t out_size)
{
    const char *type_name;
    const char *open;
    const char *close;
    size_t len;

    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';
    if (ctx == NULL || ctx->expected_type_name == NULL)
        return true;
    type_name = ctx->expected_type_name;
    if (pgy_classify_type(type_name) != PGY_TK_RC)
        return true;
    open = strchr(type_name, '<');
    close = strrchr(type_name, '>');
    if (open == NULL || close == NULL || close <= open + 1)
        return true;
    len = (size_t)(close - open - 1);
    if (len >= out_size)
        return false;
    memcpy(out, open + 1, len);
    out[len] = '\0';
    return true;
}

static LLVMValueRef
llvm_rc_coerce_numeric(LLVMGenCtx *ctx, LLVMValueRef value, LLVMTypeRef target)
{
    LLVMTypeRef source;
    bool source_is_int;
    bool target_is_int;
    bool source_is_fp;
    bool target_is_fp;

    if (ctx == NULL || value == NULL || target == NULL)
        return value;
    source = LLVMTypeOf(value);
    if (source == target)
        return value;

    source_is_int = (source == ctx->type_i1 || source == ctx->type_i32
        || source == ctx->type_i64);
    target_is_int = (target == ctx->type_i1 || target == ctx->type_i32
        || target == ctx->type_i64);
    source_is_fp = (source == ctx->type_f32 || source == ctx->type_f64);
    target_is_fp = (target == ctx->type_f32 || target == ctx->type_f64);

    if (source_is_int && target_is_int)
        return LLVMBuildIntCast(ctx->builder, value, target,
                                llvm_tmp_name(ctx));
    if (source_is_int && target_is_fp)
        return LLVMBuildSIToFP(ctx->builder, value, target,
                               llvm_tmp_name(ctx));
    if (source_is_fp && target_is_int)
        return LLVMBuildFPToSI(ctx->builder, value, target,
                               llvm_tmp_name(ctx));
    if (source_is_fp && target_is_fp) {
        bool source_is_f32 = (source == ctx->type_f32);
        bool target_is_f64 = (target == ctx->type_f64);
        return source_is_f32 && target_is_f64
            ? LLVMBuildFPExt(ctx->builder, value, target, llvm_tmp_name(ctx))
            : LLVMBuildFPTrunc(ctx->builder, value, target,
                               llvm_tmp_name(ctx));
    }
    return value;
}

static LLVMValueRef
llvm_rc_error_recovery(LLVMGenCtx *ctx, ASTNode *node, const char *message)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "%s", message != NULL ? message
                : "LLVM Rc/Weak builtin requires concrete metadata");
    }
    return NULL;
}

static bool
llvm_rc_runtime_name(LLVMGenCtx *ctx, char *out, size_t out_size,
                     const char *prefix, const char *suffix,
                     const char *callee_name)
{
    int written;

    if (out == NULL || out_size == 0 || prefix == NULL || suffix == NULL)
        return false;

    written = snprintf(out, out_size, "%s%s", prefix, suffix);
    if (written >= 0 && (size_t)written < out_size)
        return true;

    llvm_set_error(ctx, "LLVM %s runtime name is too long",
        callee_name != NULL ? callee_name : "Rc/Weak builtin");
    return false;
}

static LLVMValueRef
llvm_rc_load_handle(LLVMGenCtx *ctx, LLVMVarEntry *var)
{
    if (var == NULL)
        return NULL;
    return LLVMBuildLoad2(ctx->builder, ctx->type_i8ptr,
                          var->alloca, llvm_tmp_name(ctx));
}

bool
llvm_emit_rc_builtin_call(ASTNode *node, LLVMGenCtx *ctx,
                          const char *callee_name, LLVMValueRef *out)
{
    LLVMRcOp op = llvm_rc_lookup(callee_name);
    bool is_weak_op = op == LLVM_RC_OP_WEAK_UPGRADE
        || op == LLVM_RC_OP_WEAK_DROP;

    if (op == LLVM_RC_OP_NONE)
        return false;

    if (out == NULL)
        return true;
    *out = NULL;

    if (ast_call_arg_count(node) != 1) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM %s requires exactly one argument", callee_name);
        return true;
    }

    ASTNode *arg = ast_call_argument(node, 0);
    if (op == LLVM_RC_OP_RC_NEW) {
        char expected_inner_buf[128];
        const char *expected_inner = NULL;
        if (!llvm_rc_expected_inner_copy(ctx, expected_inner_buf,
                                         sizeof(expected_inner_buf))) {
            *out = llvm_rc_error_recovery(ctx, node,
                "LLVM RcNew expected payload type is too long");
            return true;
        }
        if (expected_inner_buf[0] != '\0')
            expected_inner = expected_inner_buf;
        LLVMValueRef value = llvm_emit_expression(arg, ctx);
        if (value == NULL) {
            *out = llvm_rc_error_recovery(ctx, node,
                "LLVM RcNew could not lower payload expression");
            return true;
        }
        const char *suffix = expected_inner != NULL
            ? llvm_rc_suffix_from_inner(ctx, expected_inner)
            : llvm_type_to_suffix(ctx, LLVMTypeOf(value));
        if (suffix == NULL || strcmp(suffix, "Unknown") == 0) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM RcNew requires Int/Long/Float/Double/Bool/String value");
            return true;
        }
        if (expected_inner != NULL) {
            LLVMTypeRef target_type = pergyra_type_to_llvm(ctx, expected_inner);
            if (ctx->has_error || target_type == NULL) {
                *out = llvm_rc_error_recovery(ctx, node,
                    "LLVM RcNew expected payload type could not be lowered");
                return true;
            }
            value = llvm_rc_coerce_numeric(ctx, value, target_type);
        }
        char fn_name[64];
        if (!llvm_rc_runtime_name(ctx, fn_name, sizeof(fn_name),
                "pgy_rc_new_", suffix, callee_name))
            return true;
        LLVMFuncEntry *fn = llvm_required_runtime_function(ctx, node,
            "rc", callee_name, fn_name);
        if (fn == NULL)
            return true;
        LLVMValueRef args[] = { value };
        *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1,
                              llvm_tmp_name(ctx));
        return true;
    }

    if (arg == NULL || arg->type != AST_IDENTIFIER
        || ast_identifier_name(arg) == NULL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM %s requires an Rc<T> or Weak<T> binding argument",
            callee_name);
        return true;
    }

    const char *var_name = ast_identifier_name(arg);
    LLVMVarEntry var;
    bool has_var = llvm_scope_lookup_snapshot(ctx, var_name, &var);
    const char *inner = is_weak_op
        ? llvm_lookup_weak_inner(ctx, var_name)
        : llvm_lookup_rc_inner(ctx, var_name);
    const char *suffix = llvm_rc_suffix_from_inner(ctx, inner);

    if (!has_var || suffix == NULL) {
        llvm_set_error_at_with_hints(ctx, arg,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM %s on '%s' requires a concrete Rc<T>/Weak<T> inner type",
            callee_name, var_name);
        return true;
    }

    char fn_name[64];
    if (op == LLVM_RC_OP_RC_CLONE) {
        if (!llvm_rc_runtime_name(ctx, fn_name, sizeof(fn_name),
                "pgy_rc_clone_", suffix, callee_name))
            return true;
        LLVMFuncEntry *fn = llvm_required_runtime_function(ctx, node,
            "rc", callee_name, fn_name);
        if (fn == NULL)
            return true;
        LLVMValueRef handle = llvm_rc_load_handle(ctx, &var);
        LLVMValueRef args[] = { handle };
        *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1,
                              llvm_tmp_name(ctx));
        return true;
    }

    if (op == LLVM_RC_OP_RC_GET) {
        if (!llvm_rc_runtime_name(ctx, fn_name, sizeof(fn_name),
                "pgy_rc_get_", suffix, callee_name))
            return true;
        LLVMFuncEntry *fn = llvm_required_runtime_function(ctx, node,
            "rc", callee_name, fn_name);
        if (fn == NULL)
            return true;
        LLVMValueRef handle = llvm_rc_load_handle(ctx, &var);
        LLVMValueRef args[] = { handle };
        LLVMValueRef ptr = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                          args, 1, llvm_tmp_name(ctx));
        LLVMTypeRef value_ty = pergyra_type_to_llvm(ctx, inner);
        if (ctx->has_error || value_ty == NULL) {
            *out = llvm_rc_error_recovery(ctx, node,
                "LLVM RcGet payload type could not be lowered");
            return true;
        }
        *out = LLVMBuildLoad2(ctx->builder, value_ty, ptr, llvm_tmp_name(ctx));
        return true;
    }

    if (op == LLVM_RC_OP_RC_DROP) {
        if (!llvm_rc_runtime_name(ctx, fn_name, sizeof(fn_name),
                "pgy_rc_drop_", suffix, callee_name))
            return true;
        LLVMFuncEntry *fn = llvm_required_runtime_function(ctx, node,
            "rc", callee_name, fn_name);
        if (fn != NULL) {
            LLVMValueRef args[] = { var.alloca };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
        }
        *out = llvm_void_expression_placeholder(ctx, node, callee_name);
        return true;
    }

    if (op == LLVM_RC_OP_RC_DOWNGRADE) {
        if (!llvm_rc_runtime_name(ctx, fn_name, sizeof(fn_name),
                "pgy_rc_downgrade_", suffix, callee_name))
            return true;
        LLVMFuncEntry *fn = llvm_required_runtime_function(ctx, node,
            "rc", callee_name, fn_name);
        if (fn == NULL)
            return true;
        LLVMValueRef handle = llvm_rc_load_handle(ctx, &var);
        LLVMValueRef args[] = { handle };
        *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1,
                              llvm_tmp_name(ctx));
        return true;
    }

    if (op == LLVM_RC_OP_WEAK_UPGRADE) {
        if (!llvm_rc_runtime_name(ctx, fn_name, sizeof(fn_name),
                "pgy_weak_upgrade_", suffix, callee_name))
            return true;
        LLVMFuncEntry *fn = llvm_required_runtime_function(ctx, node,
            "weak", callee_name, fn_name);
        if (fn == NULL)
            return true;
        LLVMValueRef handle = llvm_rc_load_handle(ctx, &var);
        LLVMValueRef args[] = { handle };
        *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1,
                              llvm_tmp_name(ctx));
        return true;
    }

    if (op == LLVM_RC_OP_WEAK_DROP) {
        if (!llvm_rc_runtime_name(ctx, fn_name, sizeof(fn_name),
                "pgy_weak_drop_", suffix, callee_name))
            return true;
        LLVMFuncEntry *fn = llvm_required_runtime_function(ctx, node,
            "weak", callee_name, fn_name);
        if (fn != NULL) {
            LLVMValueRef args[] = { var.alloca };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
        }
        *out = llvm_void_expression_placeholder(ctx, node, callee_name);
        return true;
    }

    return false;
}

#endif /* PGY_LLVM_ENABLED */
