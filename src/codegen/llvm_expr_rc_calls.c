#include "llvm_internal.h"

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

static const char *
llvm_rc_expected_inner(LLVMGenCtx *ctx)
{
    static char inner[128];
    const char *type_name;
    const char *open;
    const char *close;
    size_t len;

    if (ctx == NULL || ctx->expected_type_name == NULL)
        return NULL;
    type_name = ctx->expected_type_name;
    if (strncmp(type_name, "Rc<", 3) != 0)
        return NULL;
    open = strchr(type_name, '<');
    close = strrchr(type_name, '>');
    if (open == NULL || close == NULL || close <= open + 1)
        return NULL;
    len = (size_t)(close - open - 1);
    if (len >= sizeof(inner))
        len = sizeof(inner) - 1;
    memcpy(inner, open + 1, len);
    inner[len] = '\0';
    return inner;
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
    bool is_rc_new = strcmp(callee_name, "RcNew") == 0;
    bool is_rc_clone = strcmp(callee_name, "RcClone") == 0;
    bool is_rc_get = strcmp(callee_name, "RcGet") == 0;
    bool is_rc_drop = strcmp(callee_name, "RcDrop") == 0;
    bool is_rc_downgrade = strcmp(callee_name, "RcDowngrade") == 0;
    bool is_weak_upgrade = strcmp(callee_name, "WeakUpgrade") == 0;
    bool is_weak_drop = strcmp(callee_name, "WeakDrop") == 0;

    if (!is_rc_new && !is_rc_clone && !is_rc_get && !is_rc_drop
        && !is_rc_downgrade && !is_weak_upgrade && !is_weak_drop)
        return false;

    if (out == NULL)
        return true;
    *out = NULL;

    if (node->data.call.arg_count != 1) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM %s requires exactly one argument", callee_name);
        return true;
    }

    ASTNode *arg = node->data.call.arguments[0];
    if (is_rc_new) {
        const char *expected_inner = llvm_rc_expected_inner(ctx);
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
        snprintf(fn_name, sizeof(fn_name), "pgy_rc_new_%s", suffix);
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
        || arg->data.identifier.name == NULL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM %s requires an Rc<T> or Weak<T> binding argument",
            callee_name);
        return true;
    }

    const char *var_name = arg->data.identifier.name;
    LLVMVarEntry *var = llvm_scope_lookup(ctx, var_name);
    const char *inner = (is_weak_upgrade || is_weak_drop)
        ? llvm_lookup_weak_inner(ctx, var_name)
        : llvm_lookup_rc_inner(ctx, var_name);
    const char *suffix = llvm_rc_suffix_from_inner(ctx, inner);

    if (var == NULL || suffix == NULL) {
        llvm_set_error_at_with_hints(ctx, arg,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM %s on '%s' requires a concrete Rc<T>/Weak<T> inner type",
            callee_name, var_name);
        return true;
    }

    char fn_name[64];
    if (is_rc_clone) {
        snprintf(fn_name, sizeof(fn_name), "pgy_rc_clone_%s", suffix);
        LLVMFuncEntry *fn = llvm_required_runtime_function(ctx, node,
            "rc", callee_name, fn_name);
        if (fn == NULL)
            return true;
        LLVMValueRef handle = llvm_rc_load_handle(ctx, var);
        LLVMValueRef args[] = { handle };
        *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1,
                              llvm_tmp_name(ctx));
        return true;
    }

    if (is_rc_get) {
        snprintf(fn_name, sizeof(fn_name), "pgy_rc_get_%s", suffix);
        LLVMFuncEntry *fn = llvm_required_runtime_function(ctx, node,
            "rc", callee_name, fn_name);
        if (fn == NULL)
            return true;
        LLVMValueRef handle = llvm_rc_load_handle(ctx, var);
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

    if (is_rc_drop) {
        snprintf(fn_name, sizeof(fn_name), "pgy_rc_drop_%s", suffix);
        LLVMFuncEntry *fn = llvm_required_runtime_function(ctx, node,
            "rc", callee_name, fn_name);
        if (fn != NULL) {
            LLVMValueRef args[] = { var->alloca };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
        }
        *out = LLVMConstInt(ctx->type_i32, 0, 0);
        return true;
    }

    if (is_rc_downgrade) {
        snprintf(fn_name, sizeof(fn_name), "pgy_rc_downgrade_%s", suffix);
        LLVMFuncEntry *fn = llvm_required_runtime_function(ctx, node,
            "rc", callee_name, fn_name);
        if (fn == NULL)
            return true;
        LLVMValueRef handle = llvm_rc_load_handle(ctx, var);
        LLVMValueRef args[] = { handle };
        *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1,
                              llvm_tmp_name(ctx));
        return true;
    }

    if (is_weak_upgrade) {
        snprintf(fn_name, sizeof(fn_name), "pgy_weak_upgrade_%s", suffix);
        LLVMFuncEntry *fn = llvm_required_runtime_function(ctx, node,
            "weak", callee_name, fn_name);
        if (fn == NULL)
            return true;
        LLVMValueRef handle = llvm_rc_load_handle(ctx, var);
        LLVMValueRef args[] = { handle };
        *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1,
                              llvm_tmp_name(ctx));
        return true;
    }

    if (is_weak_drop) {
        snprintf(fn_name, sizeof(fn_name), "pgy_weak_drop_%s", suffix);
        LLVMFuncEntry *fn = llvm_required_runtime_function(ctx, node,
            "weak", callee_name, fn_name);
        if (fn != NULL) {
            LLVMValueRef args[] = { var->alloca };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
        }
        *out = LLVMConstInt(ctx->type_i32, 0, 0);
        return true;
    }

    return false;
}
