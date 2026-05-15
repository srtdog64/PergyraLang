#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_collection_base_calls.h"

#include <stdlib.h>
#include <string.h>

#include "llvm_expr_call_collections_extended.h"
#include "llvm_internal_api.h"
#include "parser/ast_api.h"

static bool
llvm_collection_base_error_out(LLVMGenCtx *ctx, ASTNode *node,
                               LLVMValueRef *out, LLVMValueRef recovery,
                               const char *message)
{
    (void)recovery;

    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "%s",
            message != NULL ? message
                : "LLVM collection builtin could not be lowered");
    }
    if (out != NULL)
        *out = NULL;
    return true;
}

typedef enum {
    LLVM_COLLECTION_BASE_OP_NONE = 0,
    LLVM_COLLECTION_BASE_OP_LIST_NEW,
    LLVM_COLLECTION_BASE_OP_SET_ADD,
    LLVM_COLLECTION_BASE_OP_SET_HAS,
    LLVM_COLLECTION_BASE_OP_SET_NEW,
    LLVM_COLLECTION_BASE_OP_SET_REMOVE,
    LLVM_COLLECTION_BASE_OP_SET_SIZE,
} LLVMCollectionBaseOp;

typedef struct {
    const char *name;
    size_t argc;
    LLVMCollectionBaseOp op;
} LLVMCollectionBaseSpec;

static const LLVMCollectionBaseSpec kLLVMCollectionBaseSpecs[] = {
    {"ListNew", 0, LLVM_COLLECTION_BASE_OP_LIST_NEW},
    {"SetAdd", 2, LLVM_COLLECTION_BASE_OP_SET_ADD},
    {"SetHas", 2, LLVM_COLLECTION_BASE_OP_SET_HAS},
    {"SetNew", 0, LLVM_COLLECTION_BASE_OP_SET_NEW},
    {"SetRemove", 2, LLVM_COLLECTION_BASE_OP_SET_REMOVE},
    {"SetSize", 1, LLVM_COLLECTION_BASE_OP_SET_SIZE},
};

static int
llvm_collection_base_spec_compare(const void *key, const void *entry)
{
    const char *name = (const char *)key;
    const LLVMCollectionBaseSpec *spec = (const LLVMCollectionBaseSpec *)entry;
    return strcmp(name, spec->name);
}

static LLVMCollectionBaseOp
llvm_collection_base_lookup(const char *callee_name, size_t argc)
{
    const LLVMCollectionBaseSpec *spec;

    if (callee_name == NULL)
        return LLVM_COLLECTION_BASE_OP_NONE;
    spec = (const LLVMCollectionBaseSpec *)bsearch(
        callee_name,
        kLLVMCollectionBaseSpecs,
        sizeof(kLLVMCollectionBaseSpecs) / sizeof(kLLVMCollectionBaseSpecs[0]),
        sizeof(kLLVMCollectionBaseSpecs[0]),
        llvm_collection_base_spec_compare);
    if (spec == NULL)
        return LLVM_COLLECTION_BASE_OP_NONE;
    if (spec->argc != argc)
        return LLVM_COLLECTION_BASE_OP_NONE;
    return spec->op;
}

bool
llvm_emit_collection_base_call(ASTNode *node, LLVMGenCtx *ctx,
                               const char *callee_name, LLVMValueRef *out)
{
    LLVMCollectionBaseOp op;

    if (out == NULL)
        return false;

    op = llvm_collection_base_lookup(callee_name, ast_call_arg_count(node));

    if (op == LLVM_COLLECTION_BASE_OP_LIST_NEW) {
        LLVMTypeRef list_ty;
        LLVMTypeRef elem_ty;
        const char *inner_name = NULL;
        char inner_name_buf[256];
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (ctx->current_ret_type == NULL
            || LLVMGetTypeKind(ctx->current_ret_type) != LLVMStructTypeKind) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM ListNew() requires contextual List<T>; implicit i32 fallback is disabled");
            *out = NULL;
            return true;
        }
        if (ctx->expected_type_name != NULL
            && strncmp(ctx->expected_type_name, "List<", 5) == 0) {
            if (llvm_constructed_arg_name_copy(ctx->expected_type_name, 0,
                    inner_name_buf, sizeof(inner_name_buf))) {
                inner_name = inner_name_buf;
            }
        }
        if (inner_name == NULL || inner_name[0] == '\0'
            || strcmp(inner_name, "Unknown") == 0) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM ListNew() requires concrete List<T> type metadata");
            *out = NULL;
            return true;
        }
        list_ty = ctx->current_ret_type;
        elem_ty = pergyra_type_to_llvm(ctx, inner_name);
        if (elem_ty == NULL)
            return llvm_collection_base_error_out(ctx, node, out,
                LLVMConstInt(ctx->type_i32, 0, 0),
                "LLVM ListNew() requires concrete element LLVM type metadata");
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_list_new_raw_export");
        if (fn == NULL) {
            *out = NULL;
            return true;
        }
        tmp = llvm_create_entry_alloca(ctx, list_ty, llvm_tmp_name(ctx));
        if (tmp == NULL)
            return llvm_collection_base_error_out(ctx, node, out,
                LLVMConstInt(ctx->type_i32, 0, 0),
                "LLVM ListNew() could not allocate list temporary");
        LLVMBuildStore(ctx->builder, LLVMConstNull(list_ty), tmp);
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            llvm_sizeof_type_i64(ctx, elem_ty)
        };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        *out = LLVMBuildLoad2(ctx->builder, list_ty, tmp, llvm_tmp_name(ctx));
        return true;
    }

    if (op == LLVM_COLLECTION_BASE_OP_SET_NEW) {
        LLVMTypeRef set_ty;
        LLVMTypeRef elem_ty;
        const char *inner_name = NULL;
        char inner_name_buf[256];
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (ctx->current_ret_type == NULL
            || LLVMGetTypeKind(ctx->current_ret_type) != LLVMStructTypeKind) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM SetNew() requires contextual Set<T>; implicit i32 fallback is disabled");
            *out = NULL;
            return true;
        }
        if (ctx->expected_type_name != NULL
            && strncmp(ctx->expected_type_name, "Set<", 4) == 0) {
            if (llvm_constructed_arg_name_copy(ctx->expected_type_name, 0,
                    inner_name_buf, sizeof(inner_name_buf))) {
                inner_name = inner_name_buf;
            }
        }
        if (inner_name == NULL || inner_name[0] == '\0'
            || strcmp(inner_name, "Unknown") == 0) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM SetNew() requires concrete Set<T> type metadata");
            *out = NULL;
            return true;
        }
        set_ty = ctx->current_ret_type;
        elem_ty = pergyra_type_to_llvm(ctx, inner_name);
        if (elem_ty == NULL)
            return llvm_collection_base_error_out(ctx, node, out,
                LLVMConstInt(ctx->type_i32, 0, 0),
                "LLVM SetNew() requires concrete element LLVM type metadata");
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_set_new_raw_export");
        if (fn == NULL) {
            *out = NULL;
            return true;
        }
        tmp = llvm_create_entry_alloca(ctx, set_ty, llvm_tmp_name(ctx));
        if (tmp == NULL)
            return llvm_collection_base_error_out(ctx, node, out,
                LLVMConstInt(ctx->type_i32, 0, 0),
                "LLVM SetNew() could not allocate set temporary");
        LLVMBuildStore(ctx->builder, LLVMConstNull(set_ty), tmp);
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            llvm_sizeof_type_i64(ctx, elem_ty)
        };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        *out = LLVMBuildLoad2(ctx->builder, set_ty, tmp, llvm_tmp_name(ctx));
        return true;
    }

    if (op == LLVM_COLLECTION_BASE_OP_SET_ADD) {
        ASTNode *set_arg = ast_call_argument(node, 0);
        LLVMVarEntry *set_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        set_var = llvm_collection_required_receiver_var(ctx, node, set_arg,
            callee_name, "collection", LLVMConstInt(ctx->type_i32, 0, 0), out);
        if (set_var == NULL)
            return true;
        inner_name = llvm_lookup_set_inner(ctx, set_arg->data.identifier.name);
        elem_ty = llvm_collection_required_value_type(ctx, node, "Set",
            set_arg->data.identifier.name, inner_name, NULL);
        if (elem_ty == NULL) {
            *out = NULL;
            return true;
        }
        value = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        if (value == NULL)
            return llvm_collection_base_error_out(ctx, node, out,
                LLVMConstInt(ctx->type_i32, 0, 0),
                "LLVM SetAdd could not lower value expression");
        if (LLVMTypeOf(value) != elem_ty) {
            if ((elem_ty == ctx->type_i32 || elem_ty == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
            else if ((elem_ty == ctx->type_f32 || elem_ty == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
        }
        if (inner_name != NULL && strcmp(inner_name, "String") == 0) {
            fn = llvm_required_collection_function(ctx, node, callee_name,
                "pgy_set_add_string_raw_export");
            if (fn == NULL) {
                *out = NULL;
                return true;
            }
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, set_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                value
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        if (tmp == NULL)
            return llvm_collection_base_error_out(ctx, node, out,
                LLVMConstInt(ctx->type_i32, 0, 0),
                "LLVM SetAdd could not allocate element temporary");
        LLVMBuildStore(ctx->builder, value, tmp);
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_set_add_raw_export");
        if (fn == NULL) {
            *out = NULL;
            return true;
        }
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, set_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            llvm_sizeof_type_i64(ctx, elem_ty)
        };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        *out = LLVMConstInt(ctx->type_i32, 0, 0);
        return true;
    }

    if (op == LLVM_COLLECTION_BASE_OP_SET_HAS) {
        ASTNode *set_arg = ast_call_argument(node, 0);
        LLVMVarEntry *set_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        set_var = llvm_collection_required_receiver_var(ctx, node, set_arg,
            callee_name, "collection", LLVMConstInt(ctx->type_i1, 0, 0), out);
        if (set_var == NULL)
            return true;
        inner_name = llvm_lookup_set_inner(ctx, set_arg->data.identifier.name);
        elem_ty = llvm_collection_required_value_type(ctx, node, "Set",
            set_arg->data.identifier.name, inner_name, NULL);
        if (elem_ty == NULL) {
            *out = NULL;
            return true;
        }
        value = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        if (value == NULL)
            return llvm_collection_base_error_out(ctx, node, out,
                LLVMConstInt(ctx->type_i1, 0, 0),
                "LLVM SetHas could not lower value expression");
        if (LLVMTypeOf(value) != elem_ty) {
            if ((elem_ty == ctx->type_i32 || elem_ty == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
            else if ((elem_ty == ctx->type_f32 || elem_ty == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
        }
        if (inner_name != NULL && strcmp(inner_name, "String") == 0) {
            fn = llvm_required_collection_function(ctx, node, callee_name,
                "pgy_set_has_string_raw_export");
            if (fn == NULL) {
                *out = NULL;
                return true;
            }
            {
                LLVMValueRef args[] = {
                    LLVMBuildBitCast(ctx->builder, set_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                    value
                };
                *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2,
                                      llvm_tmp_name(ctx));
                return true;
            }
        }
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        if (tmp == NULL)
            return llvm_collection_base_error_out(ctx, node, out,
                LLVMConstInt(ctx->type_i1, 0, 0),
                "LLVM SetHas could not allocate element temporary");
        LLVMBuildStore(ctx->builder, value, tmp);
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_set_has_raw_export");
        if (fn == NULL) {
            *out = NULL;
            return true;
        }
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, set_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3,
                                  llvm_tmp_name(ctx));
            return true;
        }
    }

    if (op == LLVM_COLLECTION_BASE_OP_SET_REMOVE) {
        ASTNode *set_arg = ast_call_argument(node, 0);
        LLVMVarEntry *set_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        set_var = llvm_collection_required_receiver_var(ctx, node, set_arg,
            callee_name, "collection", LLVMConstInt(ctx->type_i32, 0, 0), out);
        if (set_var == NULL)
            return true;
        inner_name = llvm_lookup_set_inner(ctx, set_arg->data.identifier.name);
        elem_ty = llvm_collection_required_value_type(ctx, node, "Set",
            set_arg->data.identifier.name, inner_name, NULL);
        if (elem_ty == NULL) {
            *out = NULL;
            return true;
        }
        value = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        if (value == NULL)
            return llvm_collection_base_error_out(ctx, node, out,
                LLVMConstInt(ctx->type_i32, 0, 0),
                "LLVM SetRemove could not lower value expression");
        if (LLVMTypeOf(value) != elem_ty) {
            if ((elem_ty == ctx->type_i32 || elem_ty == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
            else if ((elem_ty == ctx->type_f32 || elem_ty == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
        }
        if (inner_name != NULL && strcmp(inner_name, "String") == 0) {
            fn = llvm_required_collection_function(ctx, node, callee_name,
                "pgy_set_remove_string_raw_export");
            if (fn == NULL) {
                *out = NULL;
                return true;
            }
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, set_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                value
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        if (tmp == NULL)
            return llvm_collection_base_error_out(ctx, node, out,
                LLVMConstInt(ctx->type_i32, 0, 0),
                "LLVM SetRemove could not allocate element temporary");
        LLVMBuildStore(ctx->builder, value, tmp);
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_set_remove_raw_export");
        if (fn == NULL) {
            *out = NULL;
            return true;
        }
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, set_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            llvm_sizeof_type_i64(ctx, elem_ty)
        };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        *out = LLVMConstInt(ctx->type_i32, 0, 0);
        return true;
    }

    if (op == LLVM_COLLECTION_BASE_OP_SET_SIZE) {
        ASTNode *set_arg = ast_call_argument(node, 0);
        LLVMVarEntry *set_var;
        LLVMFuncEntry *fn;
        set_var = llvm_collection_required_receiver_var(ctx, node, set_arg,
            callee_name, "collection", LLVMConstInt(ctx->type_i32, 0, 0), out);
        if (set_var == NULL)
            return true;
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_set_size_raw_export");
        if (fn == NULL) {
            *out = NULL;
            return true;
        }
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, set_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1,
                                  llvm_tmp_name(ctx));
            return true;
        }
    }

    return false;
}

#endif
