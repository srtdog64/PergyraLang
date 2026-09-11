#ifdef PGY_LLVM_ENABLED
#include "llvm_expr_array_calls.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "llvm_expr_array_hof.h"
#include "llvm_expr_array_raw_nominal_calls.h"
#include "llvm_expr_box_array_calls.h"
#include "llvm_expr_member_lvalue.h"
#include "llvm_internal_api.h"
#include "parser/ast_api.h"

typedef enum {
    LLVM_ARRAY_BUILTIN_NONE = 0,
    LLVM_ARRAY_BUILTIN_DROP_OWNED_STRINGS,
    LLVM_ARRAY_BUILTIN_DROP_STORAGE,
    LLVM_ARRAY_BUILTIN_LENGTH,
    LLVM_ARRAY_BUILTIN_POP,
    LLVM_ARRAY_BUILTIN_PUSH,
    LLVM_ARRAY_BUILTIN_SET,
    LLVM_ARRAY_BUILTIN_SLICE_COPY,
    LLVM_ARRAY_BUILTIN_SORT,
    LLVM_ARRAY_BUILTIN_REVERSE,
    LLVM_ARRAY_BUILTIN_MAP,
    LLVM_ARRAY_BUILTIN_FILTER,
    LLVM_ARRAY_BUILTIN_PUSH_OWNED_STRING,
} LLVMArrayBuiltinOp;

typedef struct {
    const char *name;
    unsigned argc;
    LLVMArrayBuiltinOp op;
} LLVMArrayBuiltinSpec;

static const LLVMArrayBuiltinSpec kArrayBuiltinSpecs[] = {
    {"ArrayDropOwnedStrings", 1, LLVM_ARRAY_BUILTIN_DROP_OWNED_STRINGS},
    {"ArrayFilter", 2, LLVM_ARRAY_BUILTIN_FILTER},
    {"ArrayLength", 1, LLVM_ARRAY_BUILTIN_LENGTH},
    {"ArrayMap", 2, LLVM_ARRAY_BUILTIN_MAP},
    {"ArrayPop", 1, LLVM_ARRAY_BUILTIN_POP},
    {"ArrayPush", 2, LLVM_ARRAY_BUILTIN_PUSH},
    {"ArrayPushOwnedString", 2, LLVM_ARRAY_BUILTIN_PUSH_OWNED_STRING},
    {"ArrayReverse", 1, LLVM_ARRAY_BUILTIN_REVERSE},
    {"ArraySet", 3, LLVM_ARRAY_BUILTIN_SET},
    {"ArraySort", 1, LLVM_ARRAY_BUILTIN_SORT},
    {"CompilerRetireArrayStorage", 1, LLVM_ARRAY_BUILTIN_DROP_STORAGE},
    {"SliceCopy", 1, LLVM_ARRAY_BUILTIN_SLICE_COPY},
};

static int
llvm_array_builtin_spec_compare(const void *key, const void *entry)
{
    const char *name = (const char *)key;
    const LLVMArrayBuiltinSpec *spec = (const LLVMArrayBuiltinSpec *)entry;
    return strcmp(name, spec->name);
}

static LLVMArrayBuiltinOp
llvm_array_builtin_lookup(const char *callee_name, unsigned argc)
{
    const LLVMArrayBuiltinSpec *spec;

    if (callee_name == NULL)
        return LLVM_ARRAY_BUILTIN_NONE;
    spec = (const LLVMArrayBuiltinSpec *)bsearch(
        callee_name,
        kArrayBuiltinSpecs,
        sizeof(kArrayBuiltinSpecs) / sizeof(kArrayBuiltinSpecs[0]),
        sizeof(kArrayBuiltinSpecs[0]),
        llvm_array_builtin_spec_compare);
    if (spec == NULL || spec->argc != argc)
        return LLVM_ARRAY_BUILTIN_NONE;
    return spec->op;
}

static LLVMValueRef
llvm_array_required_receiver_binding(LLVMGenCtx *ctx, ASTNode *node,
                                     ASTNode *receiver,
                                     const char *callee_name,
                                     LLVMArrayVarEntry **entry_out)
{
    LLVMVarEntry var;

    if (entry_out != NULL)
        *entry_out = NULL;
    /* A field can hold the array as readily as a local can. Its storage is
     * the projected field pointer and its element type is carried by the
     * runtime struct name, so no registered local is involved. */
    if (receiver != NULL && receiver->type == AST_MEMBER_ACCESS) {
        LLVMTypeRef field_type = NULL;
        LLVMValueRef field_ptr =
            llvm_emit_member_lvalue_ptr(receiver, ctx, &field_type);
        const char *struct_name = field_type != NULL
            && LLVMGetTypeKind(field_type) == LLVMStructTypeKind
            ? LLVMGetStructName(field_type) : NULL;
        if (field_ptr == NULL || struct_name == NULL
            || strncmp(struct_name, "PgyArray_", 9) != 0) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM array operation '%s' requires an Array<T> member receiver",
                callee_name);
            return NULL;
        }
        LLVMArrayVarEntry *member_entry =
            pgy_arena_alloc(&ctx->scratch, sizeof(*member_entry));
        if (member_entry == NULL) {
            llvm_set_mir_memory_exhausted(ctx,
                "LLVM array member receiver allocation failed for '%s'",
                callee_name);
            return NULL;
        }
        member_entry->var_name = struct_name;
        member_entry->binding = field_ptr;
        member_entry->elem_type = pergyra_type_to_llvm(ctx, struct_name + 9);
        member_entry->elem_name = struct_name + 9;
        member_entry->length = -1;
        if (member_entry->elem_type == NULL) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM array operation '%s' requires concrete Array<T> element metadata",
                callee_name);
            return NULL;
        }
        if (entry_out != NULL)
            *entry_out = member_entry;
        return field_ptr;
    }
    if (receiver == NULL || receiver->type != AST_IDENTIFIER
        || ast_identifier_name(receiver) == NULL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM array operation '%s' requires an identifier or member receiver",
            callee_name);
        return NULL;
    }

    const char *name = ast_identifier_name(receiver);
    LLVMArrayVarEntry *entry = llvm_lookup_array_var(ctx, name);
    if (!llvm_scope_lookup_snapshot(ctx, name, &var)
        || var.alloca == NULL || entry == NULL) {
        llvm_set_error_at_with_hints(ctx, receiver,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM array operation '%s' requires registered Array<T> local '%s'",
            callee_name, name);
        return NULL;
    }

    if (entry_out != NULL)
        *entry_out = entry;
    return var.alloca;
}

static const char *
llvm_array_required_elem_suffix(LLVMGenCtx *ctx, ASTNode *node,
                                LLVMArrayVarEntry *entry,
                                const char *callee_name)
{
    const char *suffix = entry != NULL
        ? llvm_type_to_suffix(ctx, entry->elem_type)
        : NULL;
    if (suffix != NULL && strcmp(suffix, "Unknown") != 0)
        return suffix;
    llvm_set_error_at_with_hints(ctx, node,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "LLVM array operation '%s' requires concrete Array<T> element metadata",
        callee_name);
    return NULL;
}

static bool
llvm_array_format_runtime_name(char *out, size_t out_size,
                               const char *prefix, const char *suffix)
{
    int written;

    if (out == NULL || out_size == 0 || prefix == NULL || suffix == NULL)
        return false;
    written = snprintf(out, out_size, "%s_%s", prefix, suffix);
    return written >= 0 && (size_t)written < out_size;
}

static bool
llvm_array_error_out(ASTNode *node, LLVMGenCtx *ctx,
                     const char *message, LLVMValueRef *out)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "%s",
            message != NULL ? message
                : "LLVM array builtin could not be lowered");
    }
    if (out != NULL)
        *out = NULL;
    return true;
}

/*
 * Resolve "<prefix>_<suffix>" in the array runtime registry. On failure the
 * diagnostic is already reported and *out cleared, so callers return true.
 */
static LLVMFuncEntry *
llvm_array_required_suffix_runtime(LLVMGenCtx *ctx, ASTNode *node,
                                   const char *callee_name, const char *prefix,
                                   const char *suffix, const char *missing_msg,
                                   LLVMValueRef *out)
{
    char fn_name[64];

    if (!llvm_array_format_runtime_name(fn_name, sizeof(fn_name),
            prefix, suffix)) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM array operation '%s' runtime function name is too long",
            callee_name != NULL ? callee_name : "<unknown>");
        if (out != NULL)
            *out = NULL;
        return NULL;
    }
    LLVMFuncEntry *fn = llvm_required_runtime_function(ctx, node,
        "array", callee_name, fn_name);
    if (fn == NULL)
        llvm_array_error_out(node, ctx, missing_msg, out);
    return fn;
}

static const char *
llvm_slice_value_suffix(LLVMGenCtx *ctx, LLVMValueRef slice)
{
    LLVMTypeRef ty;

    if (ctx == NULL || slice == NULL)
        return NULL;
    ty = LLVMTypeOf(slice);
    if (ty == ctx->slice_type_Int)    return "Int";
    if (ty == ctx->slice_type_Long)   return "Long";
    if (ty == ctx->slice_type_Float)  return "Float";
    if (ty == ctx->slice_type_Double) return "Double";
    if (ty == ctx->slice_type_Bool)   return "Bool";
    if (ty == ctx->slice_type_String) return "String";
    return NULL;
}

/* ArrayMap/ArrayFilter: resolve the receiver, then hand the counted loop
 * to the higher-order owner. */
static bool
llvm_array_hof_emit(ASTNode *node, LLVMGenCtx *ctx,
                    const char *callee_name, int is_filter, LLVMValueRef *out)
{
    LLVMArrayVarEntry *entry = NULL;
    LLVMValueRef arr_alloca = llvm_array_required_receiver_binding(
        ctx, node, ast_call_argument(node, 0), callee_name, &entry);
    if (arr_alloca == NULL)
        return llvm_array_error_out(node, ctx,
            "LLVM array map/filter requires registered Array<T> receiver", out);
    const char *suffix = llvm_array_required_elem_suffix(ctx, node, entry,
        callee_name);
    if (suffix == NULL)
        return true;
    return llvm_array_emit_hof(node, ctx, callee_name, arr_alloca, entry,
        suffix, is_filter, out);
}

bool
llvm_emit_array_builtin_call(ASTNode *node, LLVMGenCtx *ctx,
                             const char *callee_name, LLVMValueRef *out)
{
    LLVMArrayBuiltinOp op;

    if (out == NULL)
        return false;
    if (llvm_emit_box_array_builtin_call(node, ctx, callee_name, out))
        return true;

    op = llvm_array_builtin_lookup(callee_name,
        (unsigned)ast_call_arg_count(node));

    if (op == LLVM_ARRAY_BUILTIN_MAP)
        return llvm_array_hof_emit(node, ctx, callee_name, 0, out);

    if (op == LLVM_ARRAY_BUILTIN_FILTER)
        return llvm_array_hof_emit(node, ctx, callee_name, 1, out);

    if (op == LLVM_ARRAY_BUILTIN_LENGTH) {
        LLVMValueRef arr = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        if (arr != NULL && LLVMGetTypeKind(LLVMTypeOf(arr)) == LLVMStructTypeKind) {
            LLVMValueRef len = llvm_array_length_i64(ctx, arr);
            *out = LLVMBuildTrunc(ctx->builder, len, ctx->type_i32, llvm_tmp_name(ctx));
            return true;
        }
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM ArrayLength requires concrete Array<T> aggregate operand");
        *out = NULL;
        return true;
    }

    if (op == LLVM_ARRAY_BUILTIN_SLICE_COPY) {
        LLVMValueRef slice = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        const char *suffix = llvm_slice_value_suffix(ctx, slice);
        if (suffix == NULL) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM SliceCopy requires concrete Slice<T> operand");
            *out = NULL;
            return true;
        }

        LLVMFuncEntry *fn = llvm_array_required_suffix_runtime(ctx, node,
            callee_name, "pgy_slice_copy", suffix,
            "LLVM SliceCopy requires registered runtime function", out);
        if (fn == NULL)
            return true;

        LLVMValueRef slice_addr = llvm_create_entry_alloca(ctx,
            LLVMTypeOf(slice), "slice.copy.addr");
        LLVMBuildStore(ctx->builder, slice, slice_addr);
        LLVMValueRef args[] = { slice_addr };
        *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
            args, 1, llvm_tmp_name(ctx));
        return true;
    }

    if (op == LLVM_ARRAY_BUILTIN_DROP_OWNED_STRINGS) {
        ASTNode *arr_arg = ast_call_argument(node, 0);
        LLVMArrayVarEntry *entry = NULL;
        LLVMValueRef arr_alloca = llvm_array_required_receiver_binding(
            ctx, node, arr_arg, callee_name, &entry);
        if (arr_alloca == NULL)
            return llvm_array_error_out(node, ctx,
                "LLVM ArrayDropOwnedStrings requires registered Array<String> receiver", out);
        const char *suffix = llvm_type_to_suffix(ctx, entry->elem_type);
        if (suffix == NULL || strcmp(suffix, "String") != 0)
            return llvm_array_error_out(node, ctx,
                "LLVM ArrayDropOwnedStrings requires Array<String>", out);
        LLVMFuncEntry *fn = llvm_array_required_suffix_runtime(ctx, node,
            callee_name, "pgy_array_drop_owned", suffix,
            "LLVM ArrayDropOwnedStrings requires registered runtime function",
            out);
        if (fn == NULL)
            return true;
        LLVMValueRef args[] = { arr_alloca };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
        *out = llvm_void_expression_placeholder(ctx, node, callee_name);
        return true;
    }

    if (op == LLVM_ARRAY_BUILTIN_DROP_STORAGE) {
        ASTNode *arr_arg = ast_call_argument(node, 0);
        LLVMArrayVarEntry *entry = NULL;
        LLVMValueRef arr_alloca = llvm_array_required_receiver_binding(
            ctx, node, arr_arg, callee_name, &entry);
        if (arr_alloca == NULL)
            return llvm_array_error_out(node, ctx,
                "LLVM CompilerRetireArrayStorage requires registered Array<T> receiver",
                out);
        return llvm_array_emit_storage_drop(
            ctx, node, callee_name, arr_alloca, entry, out);
    }

    if (op == LLVM_ARRAY_BUILTIN_PUSH ||
        op == LLVM_ARRAY_BUILTIN_PUSH_OWNED_STRING) {
        ASTNode *arr_arg = ast_call_argument(node, 0);
        LLVMArrayVarEntry *entry = NULL;
        LLVMValueRef arr_alloca = llvm_array_required_receiver_binding(
            ctx, node, arr_arg, callee_name, &entry);
        if (arr_alloca == NULL)
            return llvm_array_error_out(node, ctx,
                "LLVM ArrayPush requires registered Array<T> receiver", out);
        const char *suffix = llvm_type_to_suffix(ctx, entry->elem_type);
        bool use_raw_nominal = llvm_array_entry_uses_raw_nominal(ctx, entry);
        if ((suffix == NULL || strcmp(suffix, "Unknown") == 0)
            && !use_raw_nominal) {
            llvm_array_required_elem_suffix(ctx, node, entry, callee_name);
            return true;
        }

        LLVMValueRef value = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        if (value == NULL)
            return llvm_array_error_out(node, ctx,
                "LLVM ArrayPush could not lower value expression", out);
        if (LLVMTypeOf(value) != entry->elem_type) {
            if ((entry->elem_type == ctx->type_i32 || entry->elem_type == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = llvm_build_checked_fptosi(ctx, value, entry->elem_type, llvm_tmp_name(ctx));
            else if ((entry->elem_type == ctx->type_f32 || entry->elem_type == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, entry->elem_type, llvm_tmp_name(ctx));
        }
        if (LLVMTypeOf(value) != entry->elem_type)
            return llvm_array_error_out(node, ctx,
                "LLVM ArrayPush value does not match Array<T> element type",
                out);

        if (use_raw_nominal)
            return llvm_array_emit_raw_nominal_push(ctx, node, callee_name,
                arr_alloca, entry, value, out);

        if (op == LLVM_ARRAY_BUILTIN_PUSH_OWNED_STRING &&
            (suffix == NULL || strcmp(suffix, "String") != 0))
            return llvm_array_error_out(node, ctx,
                "LLVM ArrayPushOwnedString requires Array<String>", out);

        LLVMFuncEntry *fn = llvm_array_required_suffix_runtime(ctx, node,
            callee_name,
            op == LLVM_ARRAY_BUILTIN_PUSH_OWNED_STRING
                ? "pgy_array_push_owned" : "pgy_array_push", suffix,
            "LLVM ArrayPush requires registered runtime function", out);
        if (fn == NULL)
            return true;
        LLVMValueRef args[] = { arr_alloca, value };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        *out = llvm_void_expression_placeholder(ctx, node, callee_name);
        return true;
    }

    if (op == LLVM_ARRAY_BUILTIN_SET) {
        ASTNode *arr_arg = ast_call_argument(node, 0);
        LLVMArrayVarEntry *entry = NULL;
        LLVMValueRef arr_alloca = llvm_array_required_receiver_binding(
            ctx, node, arr_arg, callee_name, &entry);
        if (arr_alloca == NULL)
            return llvm_array_error_out(node, ctx,
                "LLVM ArraySet requires registered Array<T> receiver", out);
        const char *suffix = llvm_type_to_suffix(ctx, entry->elem_type);
        bool use_raw_nominal = llvm_array_entry_uses_raw_nominal(ctx, entry);
        if ((suffix == NULL || strcmp(suffix, "Unknown") == 0)
            && !use_raw_nominal) {
            llvm_array_required_elem_suffix(ctx, node, entry, callee_name);
            return true;
        }

        LLVMValueRef idx = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        LLVMValueRef value = llvm_emit_expression(ast_call_argument(node, 2), ctx);
        if (idx == NULL || value == NULL)
            return llvm_array_error_out(node, ctx,
                "LLVM ArraySet could not lower index or value expression", out);

        if (LLVMTypeOf(value) != entry->elem_type) {
            if ((entry->elem_type == ctx->type_i32 || entry->elem_type == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = llvm_build_checked_fptosi(ctx, value, entry->elem_type, llvm_tmp_name(ctx));
            else if ((entry->elem_type == ctx->type_f32 || entry->elem_type == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, entry->elem_type, llvm_tmp_name(ctx));
        }
        if (LLVMTypeOf(value) != entry->elem_type)
            return llvm_array_error_out(node, ctx,
                "LLVM ArraySet value does not match Array<T> element type",
                out);
        LLVMValueRef index64 = idx;
        if (LLVMTypeOf(index64) != ctx->type_i64)
            index64 = LLVMBuildSExtOrBitCast(ctx->builder, index64,
                ctx->type_i64, llvm_tmp_name(ctx));
        if (use_raw_nominal)
            return llvm_array_emit_raw_nominal_set(ctx, node, callee_name,
                arr_alloca, entry, index64, value, out);
        /* Contract: checked ArraySet lowers to pgy_array_set_<suffix>. */
        LLVMFuncEntry *fn = llvm_array_required_suffix_runtime(ctx, node,
            callee_name, "pgy_array_set", suffix,
            "LLVM ArraySet requires registered runtime function", out);
        if (fn == NULL)
            return true;
        LLVMValueRef args[] = { arr_alloca, index64, value };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        *out = llvm_void_expression_placeholder(ctx, node, callee_name);
        return true;
    }

    if (op == LLVM_ARRAY_BUILTIN_SORT || op == LLVM_ARRAY_BUILTIN_REVERSE) {
        ASTNode *arr_arg = ast_call_argument(node, 0);
        LLVMArrayVarEntry *entry = NULL;
        LLVMValueRef arr_alloca = llvm_array_required_receiver_binding(
            ctx, node, arr_arg, callee_name, &entry);
        if (arr_alloca == NULL)
            return llvm_array_error_out(node, ctx,
                "LLVM array transform requires registered Array<T> receiver", out);
        const char *suffix = llvm_array_required_elem_suffix(
            ctx, node, entry, callee_name);
        if (suffix == NULL)
            return llvm_array_error_out(node, ctx,
                "LLVM array transform requires concrete Array<T> element metadata",
                out);

        LLVMFuncEntry *fn = llvm_array_required_suffix_runtime(ctx, node,
            callee_name, op == LLVM_ARRAY_BUILTIN_SORT
                ? "pgy_array_sort" : "pgy_array_reverse", suffix,
            "LLVM array transform requires registered runtime function", out);
        if (fn == NULL)
            return true;

        /* Extract data pointer (field 0) and length (field 1) from the
         * array struct, call sort, then return the original array value. */
        LLVMTypeRef arr_struct_ty = llvm_array_struct_type(ctx, suffix);
        if (arr_struct_ty == NULL)
            return llvm_array_error_out(node, ctx,
                "LLVM array transform cannot resolve Array<T> struct type", out);
        LLVMValueRef data_gep = LLVMBuildStructGEP2(ctx->builder,
            arr_struct_ty, arr_alloca, 0, llvm_tmp_name(ctx));
        LLVMValueRef data_ptr = LLVMBuildLoad2(ctx->builder,
            LLVMPointerType(entry->elem_type, 0), data_gep,
            llvm_tmp_name(ctx));
        LLVMValueRef len_gep = LLVMBuildStructGEP2(ctx->builder,
            arr_struct_ty, arr_alloca, 1, llvm_tmp_name(ctx));
        LLVMValueRef len = LLVMBuildLoad2(ctx->builder, ctx->type_i64,
            len_gep, llvm_tmp_name(ctx));
        LLVMValueRef args[] = { data_ptr, len };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        /* Return the (now sorted) array struct value. */
        *out = LLVMBuildLoad2(ctx->builder, arr_struct_ty, arr_alloca,
            llvm_tmp_name(ctx));
        return true;
    }

    if (op == LLVM_ARRAY_BUILTIN_POP) {
        ASTNode *arr_arg = ast_call_argument(node, 0);
        LLVMArrayVarEntry *entry = NULL;
        LLVMValueRef arr_alloca = llvm_array_required_receiver_binding(
            ctx, node, arr_arg, callee_name, &entry);
        if (arr_alloca == NULL)
            return llvm_array_error_out(node, ctx,
                "LLVM ArrayPop requires registered Array<T> receiver", out);
        const char *suffix = llvm_type_to_suffix(ctx, entry->elem_type);
        bool use_raw_nominal = llvm_array_entry_uses_raw_nominal(ctx, entry);
        if (use_raw_nominal)
            return llvm_array_emit_raw_nominal_pop(ctx, node, callee_name,
                arr_alloca, entry, out);
        if (suffix == NULL || strcmp(suffix, "Unknown") == 0) {
            llvm_array_required_elem_suffix(ctx, node, entry, callee_name);
            return llvm_array_error_out(node, ctx,
                "LLVM ArrayPop requires concrete Array<T> element metadata", out);
        }

        LLVMFuncEntry *fn = llvm_array_required_suffix_runtime(ctx, node,
            callee_name, "pgy_array_pop", suffix,
            "LLVM ArrayPop requires registered runtime function", out);
        if (fn == NULL)
            return true;
        LLVMValueRef args[] = { arr_alloca };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
        *out = llvm_void_expression_placeholder(ctx, node, callee_name);
        return true;
    }

    return false;
}

#endif
