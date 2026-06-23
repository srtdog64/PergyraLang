#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_aggregate.h"

#include <string.h>

#include "llvm_backend_type_map_internal.h"
#include "llvm_expr_emit_support.h"
#include "llvm_expr_call_collections_map_exports.h"
#include "llvm_internal_api.h"

LLVMValueRef
llvm_emit_tuple_literal_expr(ASTNode *node, LLVMGenCtx *ctx)
{
    size_t n = ast_tuple_literal_count(node);
    if (n < 2)
        return llvm_expression_error(ctx, node,
            "LLVM tuple literal requires at least 2 elements");

    LLVMValueRef *vals = pgy_arena_calloc(&ctx->scratch,
        n * sizeof(LLVMValueRef));
    LLVMTypeRef *tys = pgy_arena_calloc(&ctx->scratch,
        n * sizeof(LLVMTypeRef));
    if (vals == NULL || tys == NULL)
        return llvm_expression_error(ctx, node,
            "LLVM tuple literal allocation failed");

    for (size_t i = 0; i < n; i++) {
        vals[i] = llvm_emit_expression(ast_tuple_literal_element(node, i), ctx);
        if (vals[i] == NULL) {
            if (ctx != NULL && !ctx->has_error) {
                llvm_set_error_at_with_hints(ctx, node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "LLVM tuple literal could not lower element %zu",
                    i);
            }
            return NULL;
        }
        tys[i] = LLVMTypeOf(vals[i]);
    }

    LLVMTypeRef tup_ty = LLVMStructTypeInContext(ctx->context, tys,
        (unsigned)n, 0);
    LLVMValueRef agg = LLVMGetUndef(tup_ty);
    for (size_t i = 0; i < n; i++)
        agg = LLVMBuildInsertValue(ctx->builder, agg, vals[i],
            (unsigned)i, llvm_tmp_name(ctx));
    return agg;
}

static const char *
llvm_sequence_expected_type_name(LLVMGenCtx *ctx)
{
    const char *expected_type_name;
    char *alias_target_type_name;

    if (ctx == NULL)
        return NULL;
    expected_type_name = ctx->expected_type_name;
    if (expected_type_name == NULL)
        return NULL;
    alias_target_type_name = llvm_render_alias_target_type_name_scratch(
        ctx, expected_type_name, &ctx->scratch);
    if (alias_target_type_name != NULL)
        return alias_target_type_name;
    return expected_type_name;
}

/* A `[...]` literal whose binding type is List<T>/Queue<T>. Mirrors the set
 * literal: the raw list/queue runtime (pgy_<kind>_new_raw_export +
 * pgy_<kind>_push[_string]_raw_export), keyed off the contextual element type. */
static LLVMValueRef
llvm_emit_seq_list_queue_literal(ASTNode *node, LLVMGenCtx *ctx,
                                 const char *kind,
                                 const char *expected_type_name)
{
    size_t count = ast_array_literal_count(node);
    bool is_list = strcmp(kind, "list") == 0;
    PgyTypeKind expected_kind = pgy_classify_type(expected_type_name);
    char elem_buf[128];
    char new_name[64];
    char push_name[64];
    char push_str_name[64];
    LLVMTypeRef seq_ty;
    LLVMTypeRef elem_ty;
    LLVMValueRef tmp;
    LLVMFuncEntry *new_fn;
    bool is_string;

    if (expected_type_name == NULL
        || expected_kind != (is_list ? PGY_TK_LIST : PGY_TK_QUEUE)
        || !llvm_constructed_arg_name_copy(expected_type_name, 0, elem_buf,
                sizeof(elem_buf))) {
        llvm_expr_set_missing_type_error(ctx, node, "sequence literal expression");
        return NULL;
    }
    seq_ty = is_list ? llvm_list_struct_type(ctx, elem_buf)
                     : llvm_queue_struct_type(ctx, elem_buf);
    elem_ty = pergyra_type_to_llvm(ctx, elem_buf);
    if (ctx->has_error || seq_ty == NULL || elem_ty == NULL)
        return llvm_expression_error(ctx, node,
            "LLVM sequence literal could not lower List/Queue<T> type");
    tmp = llvm_create_entry_alloca(ctx, seq_ty, llvm_tmp_name(ctx));
    if (tmp == NULL)
        return llvm_expression_error(ctx, node,
            "LLVM sequence literal could not allocate temporary");
    LLVMBuildStore(ctx->builder, LLVMConstNull(seq_ty), tmp);
    snprintf(new_name, sizeof(new_name), "pgy_%s_new_raw_export", kind);
    snprintf(push_name, sizeof(push_name), "pgy_%s_push_raw_export", kind);
    snprintf(push_str_name, sizeof(push_str_name),
        "pgy_%s_push_string_raw_export", kind);
    new_fn = llvm_lookup_function(ctx, new_name);
    if (new_fn == NULL)
        return llvm_expression_error(ctx, node,
            "LLVM sequence literal requires a registered runtime new function");
    {
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr,
                llvm_tmp_name(ctx)),
            llvm_sizeof_type_i64(ctx, elem_ty)
        };
        LLVMBuildCall2(ctx->builder, new_fn->fn_type, new_fn->fn, args, 2, "");
    }
    is_string = strcmp(elem_buf, "String") == 0;
    for (size_t i = 0; i < count; i++) {
        LLVMValueRef value = llvm_emit_expression(
            ast_array_literal_element(node, i), ctx);
        if (value == NULL)
            return llvm_expression_error(ctx, node,
                "LLVM sequence literal could not lower an element");
        if (LLVMTypeOf(value) != elem_ty) {
            if ((elem_ty == ctx->type_i32 || elem_ty == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32
                    || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, elem_ty,
                    llvm_tmp_name(ctx));
            else if ((elem_ty == ctx->type_f32 || elem_ty == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32
                    || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, elem_ty,
                    llvm_tmp_name(ctx));
        }
        if (is_string) {
            LLVMFuncEntry *add_fn = llvm_lookup_function(ctx, push_str_name);
            LLVMValueRef args[2];
            if (add_fn == NULL)
                return llvm_expression_error(ctx, node,
                    "LLVM sequence literal requires the string push runtime function");
            args[0] = LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr,
                llvm_tmp_name(ctx));
            args[1] = value;
            LLVMBuildCall2(ctx->builder, add_fn->fn_type, add_fn->fn, args, 2, "");
        } else {
            LLVMValueRef etmp = llvm_create_entry_alloca(ctx, elem_ty,
                llvm_tmp_name(ctx));
            LLVMFuncEntry *add_fn;
            LLVMValueRef args[3];
            if (etmp == NULL)
                return llvm_expression_error(ctx, node,
                    "LLVM sequence literal could not allocate element temporary");
            LLVMBuildStore(ctx->builder, value, etmp);
            add_fn = llvm_lookup_function(ctx, push_name);
            if (add_fn == NULL)
                return llvm_expression_error(ctx, node,
                    "LLVM sequence literal requires the push runtime function");
            args[0] = LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr,
                llvm_tmp_name(ctx));
            args[1] = LLVMBuildBitCast(ctx->builder, etmp, ctx->type_i8ptr,
                llvm_tmp_name(ctx));
            args[2] = llvm_sizeof_type_i64(ctx, elem_ty);
            LLVMBuildCall2(ctx->builder, add_fn->fn_type, add_fn->fn, args, 3, "");
        }
    }
    return LLVMBuildLoad2(ctx->builder, seq_ty, tmp, llvm_tmp_name(ctx));
}

LLVMValueRef
llvm_emit_array_literal_expr(ASTNode *node, LLVMGenCtx *ctx)
{
    size_t count = ast_array_literal_count(node);
    const char *inner_name = NULL;
    /* A `[...]` bound to List<T>/Queue<T> lowers as a list/queue, not array. */
    const char *expected_type_name = llvm_sequence_expected_type_name(ctx);
    if (ctx != NULL && ctx->has_error)
        return NULL;
    if (expected_type_name != NULL) {
        PgyTypeKind expected_kind = pgy_classify_type(expected_type_name);
        if (expected_kind == PGY_TK_LIST)
            return llvm_emit_seq_list_queue_literal(
                node, ctx, "list", expected_type_name);
        if (expected_kind == PGY_TK_QUEUE)
            return llvm_emit_seq_list_queue_literal(
                node, ctx, "queue", expected_type_name);
    }
    char inner_name_buf[256];
    LLVMTypeRef elem_type = NULL;
    LLVMValueRef first_value = NULL;

    if (count > 0) {
        first_value = llvm_emit_expression(ast_array_literal_element(node, 0),
            ctx);
        if (first_value == NULL)
            return llvm_expression_error(ctx, node,
                "LLVM array literal could not lower element 0");
        elem_type = LLVMTypeOf(first_value);
        const char *suffix = llvm_type_to_suffix(ctx, elem_type);
        if (suffix != NULL && strcmp(suffix, "Unknown") != 0)
            inner_name = suffix;
    } else if (expected_type_name != NULL
               && pgy_classify_type(expected_type_name)
                    == PGY_TK_ARRAY) {
        if (llvm_constructed_arg_name_copy(expected_type_name, 0,
                inner_name_buf, sizeof(inner_name_buf))) {
            inner_name = inner_name_buf;
        }
    }

    if (inner_name == NULL || inner_name[0] == '\0'
        || strcmp(inner_name, "Unknown") == 0) {
        llvm_expr_set_missing_type_error(ctx, node,
            "array literal expression");
        return NULL;
    }

    LLVMTypeRef array_type = llvm_array_struct_type(ctx, inner_name);
    if (ctx->has_error || array_type == NULL)
        return llvm_expression_error(ctx, node,
            "LLVM array literal could not lower Array<T> type");

    LLVMValueRef tmp = llvm_create_entry_alloca(ctx, array_type,
        llvm_tmp_name(ctx));
    if (tmp == NULL)
        return llvm_expression_error(ctx, node,
            "LLVM array literal could not allocate array temporary");

    char push_fn_name[64];
    if (!llvm_expr_runtime_name(ctx, node, push_fn_name,
            sizeof(push_fn_name), "pgy_array_push_", inner_name))
        return NULL;
    LLVMFuncEntry *push_fn = llvm_lookup_function(ctx, push_fn_name);
    if (push_fn == NULL && count > 0) {
        llvm_required_runtime_function(ctx, node,
            "array literal expression", "ArrayPush", push_fn_name);
        return NULL;
    }

    LLVMBuildStore(ctx->builder, LLVMConstNull(array_type), tmp);
    for (size_t i = 0; i < count; i++) {
        LLVMValueRef elem = i == 0 ? first_value
            : llvm_emit_expression(ast_array_literal_element(node, i), ctx);
        if (elem == NULL) {
            if (ctx != NULL && !ctx->has_error) {
                llvm_set_error_at_with_hints(ctx, node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "LLVM array literal could not lower element %zu",
                    i);
            }
            return NULL;
        }
        LLVMValueRef args[] = { tmp, elem };
        LLVMBuildCall2(ctx->builder, push_fn->fn_type, push_fn->fn, args, 2, "");
    }
    return LLVMBuildLoad2(ctx->builder, array_type, tmp, llvm_tmp_name(ctx));
}

static bool
llvm_map_literal_emit_entry(LLVMGenCtx *ctx, ASTNode *node, size_t i,
                            LLVMValueRef map_alloca, const char *key_name,
                            const char *value_name, LLVMTypeRef value_ty)
{
    LLVMValueRef key = llvm_emit_expression(ast_map_literal_key(node, i), ctx);
    LLVMValueRef value = llvm_emit_expression(ast_map_literal_value(node, i), ctx);
    LLVMFuncEntry *fn;
    LLVMValueRef vtmp;

    if (key == NULL || value == NULL) {
        llvm_expression_error(ctx, node,
            "LLVM map literal could not lower an entry key or value");
        return false;
    }
    if (value_name != NULL && strcmp(value_name, "String") == 0) {
        fn = llvm_required_hashmap_raw_string_value_export(ctx, node,
            "map literal", "set", key_name);
        if (fn == NULL)
            return false;
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, map_alloca, ctx->type_i8ptr,
                llvm_tmp_name(ctx)),
            key, value
        };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        return true;
    }
    vtmp = llvm_create_entry_alloca(ctx, value_ty, llvm_tmp_name(ctx));
    if (vtmp == NULL) {
        llvm_expression_error(ctx, node,
            "LLVM map literal could not allocate value temporary");
        return false;
    }
    LLVMBuildStore(ctx->builder, value, vtmp);
    fn = llvm_required_hashmap_raw_export(ctx, node, "map literal", "set",
        key_name);
    if (fn == NULL)
        return false;
    LLVMValueRef args[] = {
        LLVMBuildBitCast(ctx->builder, map_alloca, ctx->type_i8ptr,
            llvm_tmp_name(ctx)),
        key,
        LLVMBuildBitCast(ctx->builder, vtmp, ctx->type_i8ptr,
            llvm_tmp_name(ctx)),
        llvm_sizeof_type_i64(ctx, value_ty)
    };
    LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 4, "");
    return true;
}

LLVMValueRef
llvm_emit_map_literal_expr(ASTNode *node, LLVMGenCtx *ctx)
{
    size_t count = ast_map_literal_count(node);
    const char *map_type = ctx->expected_type_name;
    char key_buf[128];
    char value_buf[128];
    LLVMTypeRef map_ty;
    LLVMTypeRef value_ty;
    LLVMValueRef tmp;
    LLVMFuncEntry *new_fn;

    if (map_type == NULL || pgy_classify_type(map_type) != PGY_TK_HASHMAP
        || !llvm_constructed_arg_name_copy(map_type, 0, key_buf, sizeof(key_buf))
        || !llvm_constructed_arg_name_copy(map_type, 1, value_buf,
                sizeof(value_buf))) {
        llvm_expr_set_missing_type_error(ctx, node, "map literal expression");
        return NULL;
    }
    map_ty = llvm_hashmap_struct_type(ctx, value_buf);
    value_ty = pergyra_type_to_llvm(ctx, value_buf);
    if (ctx->has_error || map_ty == NULL || value_ty == NULL)
        return llvm_expression_error(ctx, node,
            "LLVM map literal could not lower HashMap<K, V> type");
    tmp = llvm_create_entry_alloca(ctx, map_ty, llvm_tmp_name(ctx));
    if (tmp == NULL)
        return llvm_expression_error(ctx, node,
            "LLVM map literal could not allocate map temporary");
    new_fn = llvm_lookup_function(ctx, "pgy_map_new_raw_export");
    if (new_fn == NULL)
        return llvm_expression_error(ctx, node,
            "LLVM map literal requires registered runtime function 'pgy_map_new_raw_export'");
    {
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr,
                llvm_tmp_name(ctx)),
            llvm_sizeof_type_i64(ctx, value_ty)
        };
        LLVMBuildCall2(ctx->builder, new_fn->fn_type, new_fn->fn, args, 2, "");
    }
    for (size_t i = 0; i < count; i++) {
        if (!llvm_map_literal_emit_entry(ctx, node, i, tmp, key_buf,
                value_buf, value_ty))
            return NULL;
    }
    return LLVMBuildLoad2(ctx->builder, map_ty, tmp, llvm_tmp_name(ctx));
}

LLVMValueRef
llvm_emit_set_literal_expr(ASTNode *node, LLVMGenCtx *ctx)
{
    size_t count = ast_set_literal_count(node);
    const char *set_type = ctx->expected_type_name;
    char elem_buf[128];
    LLVMTypeRef set_ty;
    LLVMTypeRef elem_ty;
    LLVMValueRef tmp;
    LLVMFuncEntry *new_fn;
    bool is_string;

    if (set_type == NULL || pgy_classify_type(set_type) != PGY_TK_SET
        || !llvm_constructed_arg_name_copy(set_type, 0, elem_buf,
                sizeof(elem_buf))) {
        llvm_expr_set_missing_type_error(ctx, node, "set literal expression");
        return NULL;
    }
    set_ty = llvm_set_struct_type(ctx, elem_buf);
    elem_ty = pergyra_type_to_llvm(ctx, elem_buf);
    if (ctx->has_error || set_ty == NULL || elem_ty == NULL)
        return llvm_expression_error(ctx, node,
            "LLVM set literal could not lower Set<T> type");
    tmp = llvm_create_entry_alloca(ctx, set_ty, llvm_tmp_name(ctx));
    if (tmp == NULL)
        return llvm_expression_error(ctx, node,
            "LLVM set literal could not allocate set temporary");
    LLVMBuildStore(ctx->builder, LLVMConstNull(set_ty), tmp);
    new_fn = llvm_lookup_function(ctx, "pgy_set_new_raw_export");
    if (new_fn == NULL)
        return llvm_expression_error(ctx, node,
            "LLVM set literal requires registered runtime function 'pgy_set_new_raw_export'");
    {
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr,
                llvm_tmp_name(ctx)),
            llvm_sizeof_type_i64(ctx, elem_ty)
        };
        LLVMBuildCall2(ctx->builder, new_fn->fn_type, new_fn->fn, args, 2, "");
    }
    is_string = strcmp(elem_buf, "String") == 0;
    for (size_t i = 0; i < count; i++) {
        LLVMValueRef value = llvm_emit_expression(
            ast_set_literal_element(node, i), ctx);
        if (value == NULL)
            return llvm_expression_error(ctx, node,
                "LLVM set literal could not lower an element");
        if (LLVMTypeOf(value) != elem_ty) {
            if ((elem_ty == ctx->type_i32 || elem_ty == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32
                    || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, elem_ty,
                    llvm_tmp_name(ctx));
            else if ((elem_ty == ctx->type_f32 || elem_ty == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32
                    || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, elem_ty,
                    llvm_tmp_name(ctx));
        }
        if (is_string) {
            LLVMFuncEntry *add_fn = llvm_lookup_function(ctx,
                "pgy_set_add_string_raw_export");
            LLVMValueRef args[2];
            if (add_fn == NULL)
                return llvm_expression_error(ctx, node,
                    "LLVM set literal requires 'pgy_set_add_string_raw_export'");
            args[0] = LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr,
                llvm_tmp_name(ctx));
            args[1] = value;
            LLVMBuildCall2(ctx->builder, add_fn->fn_type, add_fn->fn, args, 2, "");
        } else {
            LLVMValueRef etmp = llvm_create_entry_alloca(ctx, elem_ty,
                llvm_tmp_name(ctx));
            LLVMFuncEntry *add_fn;
            LLVMValueRef args[3];
            if (etmp == NULL)
                return llvm_expression_error(ctx, node,
                    "LLVM set literal could not allocate element temporary");
            LLVMBuildStore(ctx->builder, value, etmp);
            add_fn = llvm_lookup_function(ctx, "pgy_set_add_raw_export");
            if (add_fn == NULL)
                return llvm_expression_error(ctx, node,
                    "LLVM set literal requires 'pgy_set_add_raw_export'");
            args[0] = LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr,
                llvm_tmp_name(ctx));
            args[1] = LLVMBuildBitCast(ctx->builder, etmp, ctx->type_i8ptr,
                llvm_tmp_name(ctx));
            args[2] = llvm_sizeof_type_i64(ctx, elem_ty);
            LLVMBuildCall2(ctx->builder, add_fn->fn_type, add_fn->fn, args, 3, "");
        }
    }
    return LLVMBuildLoad2(ctx->builder, set_ty, tmp, llvm_tmp_name(ctx));
}

LLVMValueRef
llvm_emit_cast_expr(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *target = ast_cast_target_type(node);
    LLVMValueRef v = llvm_emit_expression(ast_cast_operand(node), ctx);
    LLVMTypeRef src;

    if (v == NULL)
        return NULL;
    src = LLVMTypeOf(v);
    if (target != NULL && strcmp(target, "Int") == 0) {
        if (src == ctx->type_f32 || src == ctx->type_f64)
            return LLVMBuildFPToSI(ctx->builder, v, ctx->type_i32,
                llvm_tmp_name(ctx));
        if (src == ctx->type_i64)
            return LLVMBuildTrunc(ctx->builder, v, ctx->type_i32,
                llvm_tmp_name(ctx));
        return v;
    }
    if (target != NULL && strcmp(target, "Long") == 0) {
        if (src == ctx->type_f32 || src == ctx->type_f64)
            return LLVMBuildFPToSI(ctx->builder, v, ctx->type_i64,
                llvm_tmp_name(ctx));
        if (src == ctx->type_i32)
            return LLVMBuildSExt(ctx->builder, v, ctx->type_i64,
                llvm_tmp_name(ctx));
        return v;
    }
    if (target != NULL && strcmp(target, "Float") == 0) {
        if (src == ctx->type_i32 || src == ctx->type_i64)
            return LLVMBuildSIToFP(ctx->builder, v, ctx->type_f32,
                llvm_tmp_name(ctx));
        if (src == ctx->type_f64)
            return LLVMBuildFPTrunc(ctx->builder, v, ctx->type_f32,
                llvm_tmp_name(ctx));
        return v;
    }
    return llvm_expression_error(ctx, node,
        "LLVM backend: cast target is not lowered (numeric Int/Long/Float only)");
}

/*
 * Scalar name for an LLVM value's type, matching ast_type_name_canonical_scalar
 * so the `expr is Type` predicate folds to the same boolean as the C backend.
 */
static const char *
llvm_value_canonical_scalar(LLVMGenCtx *ctx, LLVMValueRef value)
{
    LLVMTypeRef type = LLVMTypeOf(value);

    if (type == ctx->type_i32)
        return "Int";
    if (type == ctx->type_i64)
        return "Long";
    if (type == ctx->type_f32 || type == ctx->type_f64)
        return "Float";
    if (type == ctx->type_i1)
        return "Bool";
    return NULL;
}

LLVMValueRef
llvm_emit_type_test_expr(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *target = ast_type_test_target_type(node);
    const char *tgt_canon = ast_type_name_canonical_scalar(target);
    LLVMValueRef v;
    const char *src_canon;
    int result;

    if (tgt_canon == NULL)
        return llvm_expression_error(ctx, node,
            "LLVM backend: type-test target is not lowered "
            "(Int/Long/Float/Bool only)");
    /* Evaluate the operand so any side effects run before the folded result. */
    v = llvm_emit_expression(ast_type_test_operand(node), ctx);
    if (v == NULL)
        return NULL;
    src_canon = llvm_value_canonical_scalar(ctx, v);
    result = (src_canon != NULL && strcmp(src_canon, tgt_canon) == 0) ? 1 : 0;
    return LLVMConstInt(ctx->type_i1, (unsigned long long)result, 0);
}

LLVMValueRef
llvm_emit_array_access_expr(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *array_node = ast_array_access_array(node);
    LLVMValueRef arr = llvm_emit_expression(array_node, ctx);
    LLVMValueRef idx = llvm_emit_expression(ast_array_access_index(node), ctx);
    if (arr == NULL || idx == NULL)
        return llvm_expression_error(ctx, node,
            "LLVM array access could not lower receiver or index expression");

    if (array_node != NULL && array_node->type == AST_IDENTIFIER) {
        const char *name = ast_identifier_name(array_node);
        LLVMVarEntry arr_var;
        bool has_arr_var = llvm_scope_lookup_snapshot(ctx, name, &arr_var);
        LLVMArrayVarEntry *entry = llvm_lookup_array_var(ctx, name);
        if (has_arr_var && entry != NULL) {
            const char *suffix = llvm_type_to_suffix(ctx, entry->elem_type);
            if (suffix != NULL && strcmp(suffix, "Unknown") != 0) {
                const char *struct_name = LLVMGetStructName(arr_var.type);
                if (struct_name != NULL
                    && strncmp(struct_name, "PgyArray_", 9) == 0) {
                    LLVMValueRef aggregate = LLVMBuildLoad2(ctx->builder,
                        arr_var.type, arr_var.alloca, llvm_tmp_name(ctx));
                    LLVMValueRef inlined = llvm_emit_inline_array_get(ctx,
                        aggregate, entry->elem_type, idx, struct_name);
                    if (inlined != NULL)
                        return inlined;
                    if (ctx->has_error)
                        return NULL;
                }
                const char *fn_prefix = "pgy_array_get_";
                char fn_name[64];
                if (struct_name != NULL
                    && strncmp(struct_name, "PgySlice_", 9) == 0) {
                    fn_prefix = "pgy_slice_get_";
                }
                if (!llvm_expr_runtime_name(ctx, node, fn_name,
                        sizeof(fn_name), fn_prefix, suffix))
                    return NULL;
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
                if (fn != NULL) {
                    LLVMValueRef index64 = idx;
                    if (LLVMTypeOf(index64) != ctx->type_i64)
                        index64 = LLVMBuildSExtOrBitCast(ctx->builder, index64,
                            ctx->type_i64, llvm_tmp_name(ctx));
                    LLVMValueRef args[] = { arr_var.alloca, index64 };
                    return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                        args, 2, llvm_tmp_name(ctx));
                }
                llvm_required_runtime_function(ctx, node,
                    "indexed collection access",
                    struct_name != NULL
                        && strncmp(struct_name, "PgySlice_", 9) == 0
                        ? "SliceGet" : "ArrayGet",
                    fn_name);
                return NULL;
            }
            return llvm_expression_error(ctx, node,
                "LLVM indexed collection access requires concrete Array<T>/Slice<T> element metadata");
        }
    }

    LLVMTypeRef arr_ty = LLVMTypeOf(arr);
    if (arr_ty == ctx->type_i8ptr) {
        LLVMValueRef gep = LLVMBuildGEP2(ctx->builder,
            LLVMInt8TypeInContext(ctx->context),
            arr, &idx, 1, llvm_tmp_name(ctx));
        return LLVMBuildLoad2(ctx->builder,
            LLVMInt8TypeInContext(ctx->context),
            gep, llvm_tmp_name(ctx));
    }

    if (LLVMGetTypeKind(arr_ty) == LLVMPointerTypeKind) {
        LLVMTypeRef elem_ty = LLVMGetElementType(arr_ty);
        if (elem_ty != NULL) {
            LLVMValueRef gep = LLVMBuildGEP2(ctx->builder,
                elem_ty, arr, &idx, 1, llvm_tmp_name(ctx));
            return LLVMBuildLoad2(ctx->builder, elem_ty,
                gep, llvm_tmp_name(ctx));
        }
    }

    if (LLVMGetTypeKind(arr_ty) == LLVMStructTypeKind) {
        const char *struct_name = LLVMGetStructName(arr_ty);
        LLVMValueRef checked = llvm_emit_checked_collection_get(
            ctx, arr, arr_ty, idx, struct_name);
        if (checked != NULL)
            return checked;
        if (ctx->has_error)
            return NULL;

        LLVMValueRef data_ptr = llvm_array_data_ptr(ctx, arr);
        LLVMTypeRef elem_ty = llvm_stmt_resolve_array_elem_type(
            ctx, array_node, data_ptr);
        if (elem_ty == NULL)
            return llvm_expression_error(ctx, node,
                "LLVM aggregate array access requires concrete element metadata");
        LLVMValueRef gep = LLVMBuildGEP2(ctx->builder,
            elem_ty, data_ptr, &idx, 1, llvm_tmp_name(ctx));
        return LLVMBuildLoad2(ctx->builder, elem_ty,
            gep, llvm_tmp_name(ctx));
    }
    return llvm_expression_error(ctx, node,
        "LLVM array access receiver is not an array, slice, string, or pointer");
}

#endif /* PGY_LLVM_ENABLED */
