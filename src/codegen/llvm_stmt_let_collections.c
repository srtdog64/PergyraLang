#ifdef PGY_LLVM_ENABLED
#include "codegen_channel_runtime_abi.h"
#include "llvm_internal.h"
#include "llvm_stmt_let_collection_policy.h"
#include "parser/ast_api.h"
#include "../common/string_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum
{
    LLVM_STMT_COLLECTION_DIAG_TYPE_ARG,
    LLVM_STMT_COLLECTION_DIAG_RUNTIME_FN
} LLVMStmtCollectionDiagKind;

static bool
llvm_stmt_diag_collection(LLVMGenCtx *ctx,
                          ASTNode *node,
                          LLVMStmtCollectionDiagKind kind,
                          const char *binding_name,
                          const char *container_name,
                          size_t arg_index,
                          const char *fn_name)
{
    if (ctx == NULL)
        return false;
    if (ctx->has_error)
        return false;

    if (kind == LLVM_STMT_COLLECTION_DIAG_TYPE_ARG) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM %s binding '%s' requires explicit concrete type argument %zu",
            container_name != NULL ? container_name : "collection",
            binding_name != NULL ? binding_name : "<binding>",
            arg_index + 1);
        return false;
    }

    llvm_set_error_at_with_hints(ctx, node,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_INSPECT_MIR_INVENTORY,
        "LLVM %s binding '%s' requires runtime export '%s'; not registered",
        container_name != NULL ? container_name : "collection",
        binding_name != NULL ? binding_name : "<binding>",
        fn_name != NULL ? fn_name : "<unknown>");
    return false;
}

static bool
llvm_stmt_collection_runtime_name(LLVMGenCtx *ctx,
                                  ASTNode *node,
                                  char *out,
                                  size_t out_size,
                                  const char *prefix,
                                  const char *type_name)
{
    int written;

    if (out == NULL || out_size == 0)
        return false;

    written = snprintf(out, out_size, "%s%s",
        prefix != NULL ? prefix : "",
        type_name != NULL ? type_name : "");
    if (written >= 0 && (size_t)written < out_size)
        return true;

    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_SPEC_LIMIT,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_REFACTOR_OR_RAISE_LIMIT,
            "LLVM collection runtime symbol is too long for type '%s'",
            type_name != NULL ? type_name : "<type>");
    }
    return false;
}

static LLVMStmtLetCallOp
llvm_stmt_let_call_op(ASTNode *init)
{
    ASTNode *callee;

    if (init == NULL || init->type != AST_CALL)
        return LLVM_STMT_LET_CALL_NONE;

    callee = ast_call_callee(init);
    if (callee == NULL || callee->type != AST_IDENTIFIER)
        return LLVM_STMT_LET_CALL_NONE;

    return llvm_stmt_let_call_lookup(ast_identifier_name(callee));
}

static void
llvm_stmt_register_collection_var(LLVMGenCtx *ctx,
                                  const char *name,
                                  const char *inner,
                                  const LLVMStmtCollectionCtorSpec *spec)
{
    if (spec == NULL)
        return;

    switch (spec->op) {
    case LLVM_STMT_COLLECTION_CTOR_LIST:
        llvm_register_list_var(ctx, name, inner);
        break;
    case LLVM_STMT_COLLECTION_CTOR_QUEUE:
        llvm_register_queue_var(ctx, name, inner);
        break;
    case LLVM_STMT_COLLECTION_CTOR_SET:
        llvm_register_set_var(ctx, name, inner);
        break;
    default:
        break;
    }
}

bool
llvm_stmt_emit_collection_like_let(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *name;
    ASTNode *type_ann;
    ASTNode *init;

    if (node == NULL || node->type != AST_LET_DECL || ctx == NULL)
        return false;

    name = ast_let_name(node);
    type_ann = ast_let_type(node);
    init = ast_let_initializer(node);

    if (type_ann != NULL
        && type_ann->type == AST_TYPE
        && ast_type_name(type_ann) != NULL
        && init != NULL
        && init->type == AST_CALL
        && ast_call_callee(init) != NULL
        && ast_call_callee(init)->type == AST_IDENTIFIER
        && ast_identifier_name(ast_call_callee(init)) != NULL
        && llvm_stmt_let_call_op(init) == LLVM_STMT_LET_CALL_TO_OBJECT
        && ast_call_arg_count(init) >= 2
        && ast_call_argument(init, 1) != NULL
        && ast_call_argument(init, 1)->type == AST_IDENTIFIER) {
        const char *type_name = ast_type_name(type_ann);
        LLVMClassTypeEntry *target_cls = llvm_lookup_class(ctx, type_name);
        if (target_cls != NULL
            && target_cls->is_immutable
            && !target_cls->is_boundary_transfer_contract) {
            const char *source_name =
                ast_identifier_name(ast_call_argument(init, 1));
            llvm_scope_declare(ctx, name, NULL, target_cls->struct_type);
            llvm_register_var_class(ctx, name, type_name);
            llvm_register_projection_borrow(ctx, name, type_name, source_name);
            return true;
        }
    }

    if (type_ann != NULL
        && type_ann->type == AST_TYPE
        && ast_type_name(type_ann) != NULL
        && init != NULL
        && init->type == AST_CALL
        && ast_call_callee(init) != NULL
        && ast_call_callee(init)->type == AST_IDENTIFIER) {
        const char *ann_name = ast_type_name(type_ann);
        GenericParams *generic_args = ast_type_generic_args(type_ann);
        const char *callee = ast_identifier_name(ast_call_callee(init));
        const LLVMStmtCollectionCtorSpec *ctor_spec =
            llvm_stmt_collection_ctor_lookup(callee);
        char *inner = NULL;

        GenericParam *inner_param = ast_generic_param_at(generic_args, 0);
        if (inner_param != NULL) {
            inner = llvm_stmt_render_type_arg(inner_param);
        }
        if (callee == NULL) {
            free(inner);
            return false;
        }

        if (ctor_spec != NULL
            && strcmp(ann_name, ctor_spec->annotation_name) == 0
            && ctor_spec->op != LLVM_STMT_COLLECTION_CTOR_HASH_MAP) {
            if (inner == NULL || inner[0] == '\0') {
                bool ok = llvm_stmt_diag_collection(ctx, node,
                    LLVM_STMT_COLLECTION_DIAG_TYPE_ARG, name, ann_name, 0, NULL);
                free(inner);
                return ok;
            }
            LLVMTypeRef list_ty = ast_type_to_llvm(ctx, type_ann);
            LLVMTypeRef elem_ty = pergyra_type_to_llvm(ctx, inner);
            if (ctx->has_error || list_ty == NULL || elem_ty == NULL) {
                free(inner);
                return true;
            }
            LLVMValueRef alloca_val = llvm_create_entry_alloca(ctx, list_ty, name);
            LLVMFuncEntry *new_fn = llvm_lookup_function(ctx, ctor_spec->runtime_fn);
            if (new_fn == NULL) {
                free(inner);
                return llvm_stmt_diag_collection(ctx, node,
                    LLVM_STMT_COLLECTION_DIAG_RUNTIME_FN, name,
                    ctor_spec->annotation_name, 0, ctor_spec->runtime_fn);
            }
            {
                LLVMValueRef args[] = {
                    LLVMBuildBitCast(ctx->builder, alloca_val, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                    llvm_sizeof_type_i64(ctx, elem_ty)
                };
                LLVMBuildCall2(ctx->builder, new_fn->fn_type, new_fn->fn, args, 2, "");
            }
            llvm_scope_declare(ctx, name, alloca_val, list_ty);
            llvm_stmt_register_collection_var(ctx, name, inner, ctor_spec);
            free(inner);
            return true;
        }

        if (ctor_spec != NULL
            && strcmp(ann_name, ctor_spec->annotation_name) == 0
            && ctor_spec->op == LLVM_STMT_COLLECTION_CTOR_HASH_MAP) {
            char *value_type = NULL;
            char *key_type = NULL;
            LLVMTypeRef map_ty = ast_type_to_llvm(ctx, type_ann);
            LLVMTypeRef value_ty;
            LLVMValueRef alloca_val;
            LLVMFuncEntry *new_fn;

            if (ast_generic_param_at(generic_args, 0) != NULL) {
                key_type = llvm_stmt_render_type_arg(
                    ast_generic_param_at(generic_args, 0));
            }
            if (ast_generic_param_at(generic_args, 1) != NULL) {
                value_type = llvm_stmt_render_type_arg(
                    ast_generic_param_at(generic_args, 1));
            }
            if (key_type == NULL || key_type[0] == '\0') {
                bool ok = llvm_stmt_diag_collection(ctx, node,
                    LLVM_STMT_COLLECTION_DIAG_TYPE_ARG, name, "HashMap", 0, NULL);
                free(inner);
                free(key_type);
                free(value_type);
                return ok;
            }
            if (value_type == NULL || value_type[0] == '\0') {
                bool ok = llvm_stmt_diag_collection(ctx, node,
                    LLVM_STMT_COLLECTION_DIAG_TYPE_ARG, name, "HashMap", 1, NULL);
                free(inner);
                free(key_type);
                free(value_type);
                return ok;
            }
            value_ty = pergyra_type_to_llvm(ctx, value_type);
            if (ctx->has_error || map_ty == NULL || value_ty == NULL) {
                free(inner);
                free(key_type);
                free(value_type);
                return true;
            }
            alloca_val = llvm_create_entry_alloca(ctx, map_ty, name);
            new_fn = llvm_lookup_function(ctx, ctor_spec->runtime_fn);
            if (new_fn == NULL) {
                bool ok = llvm_stmt_diag_collection(ctx, node,
                    LLVM_STMT_COLLECTION_DIAG_RUNTIME_FN, name,
                    "HashMap", 0, ctor_spec->runtime_fn);
                free(inner);
                free(key_type);
                free(value_type);
                return ok;
            }
            {
                LLVMValueRef args[] = {
                    LLVMBuildBitCast(ctx->builder, alloca_val, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                    llvm_sizeof_type_i64(ctx, value_ty)
                };
                LLVMBuildCall2(ctx->builder, new_fn->fn_type, new_fn->fn, args, 2, "");
            }
            llvm_scope_declare(ctx, name, alloca_val, map_ty);
            llvm_register_map_var(ctx, name, key_type, value_type);
            free(inner);
            free(key_type);
            free(value_type);
            return true;
        }
        free(inner);
    }

    if (init != NULL && init->type == AST_CALL
        && ast_call_callee(init) != NULL
        && ast_call_callee(init)->type == AST_IDENTIFIER
        && ast_identifier_name(ast_call_callee(init)) != NULL
        && llvm_stmt_let_call_op(init) == LLVM_STMT_LET_CALL_CHANNEL) {
        GenericParams *generic_args = ast_type_generic_args(type_ann);
        char *channel_inner = NULL;
        char init_fn_name[128];
        LLVMTypeRef ch_type = LLVMArrayType(
            LLVMInt8TypeInContext(ctx->context), 256);
        LLVMValueRef alloca_val = llvm_create_entry_alloca(ctx, ch_type, name);

        if (type_ann == NULL || type_ann->type != AST_TYPE
            || ast_generic_param_at(generic_args, 0) == NULL) {
            return llvm_stmt_diag_collection(ctx, node,
                LLVM_STMT_COLLECTION_DIAG_TYPE_ARG, name, "Channel", 0, NULL);
        }

        channel_inner = llvm_stmt_render_type_arg(
            ast_generic_param_at(generic_args, 0));
        if (channel_inner == NULL || channel_inner[0] == '\0') {
            free(channel_inner);
            return llvm_stmt_diag_collection(ctx, node,
                LLVM_STMT_COLLECTION_DIAG_TYPE_ARG, name, "Channel", 0, NULL);
        }

        if (!pgy_channel_runtime_name(init_fn_name, sizeof(init_fn_name),
                "init", channel_inner)) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_SPEC_LIMIT,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_REFACTOR_OR_RAISE_LIMIT,
                "LLVM Channel binding '%s' runtime symbol is too long for type '%s'",
                name != NULL ? name : "<binding>",
                channel_inner);
            free(channel_inner);
            return false;
        }
        LLVMFuncEntry *init_fn = llvm_lookup_function(ctx, init_fn_name);
        if (init_fn == NULL) {
            bool ok = llvm_stmt_diag_collection(ctx, node,
                LLVM_STMT_COLLECTION_DIAG_RUNTIME_FN, name,
                "Channel", 0, init_fn_name);
            free(channel_inner);
            return ok;
        }
        LLVMValueRef cap = LLVMConstInt(ctx->type_i64, 16, 0);
        if (ast_call_arg_count(init) > 0)
            cap = LLVMBuildZExt(ctx->builder,
                llvm_emit_expression(ast_call_argument(init, 0), ctx),
                ctx->type_i64, llvm_tmp_name(ctx));
        LLVMValueRef args[] = { alloca_val, cap };
        LLVMBuildCall2(ctx->builder, init_fn->fn_type,
                       init_fn->fn, args, 2, "");
        llvm_scope_declare(ctx, name, alloca_val, ch_type);
        llvm_register_channel_var(ctx, name, channel_inner);
        free(channel_inner);
        return true;
    }

    if (init != NULL && init->type == AST_ARRAY_LITERAL) {
        size_t count = ast_array_literal_count(init);
        LLVMTypeRef elem_type = NULL;
        char *owned_inner_name = NULL;
        const char *inner_name = NULL;
        const char *source_type_name = ctx->current_mir_routine != NULL
            ? mir_routine_source_local_type_name(ctx->current_mir_routine, name)
            : NULL;
        char source_inner_buf[128];

        if (source_type_name != NULL
            && pgy_classify_type(source_type_name) == PGY_TK_ARRAY
            && llvm_constructed_arg_name_copy(source_type_name, 0,
                source_inner_buf, sizeof(source_inner_buf))) {
            owned_inner_name = pergyra_strdup(source_inner_buf);
            inner_name = owned_inner_name;
            elem_type = pergyra_type_to_llvm(ctx, inner_name);
        } else if (type_ann != NULL && type_ann->type == AST_TYPE
            && ast_type_name(type_ann) != NULL
            && (strcmp(ast_type_name(type_ann), "Array") == 0
                || strcmp(ast_type_name(type_ann), "Slice") == 0)
            && ast_generic_param_at(ast_type_generic_args(type_ann), 0) != NULL) {
            GenericParams *generic_args = ast_type_generic_args(type_ann);
            owned_inner_name = llvm_stmt_render_type_arg(
                ast_generic_param_at(generic_args, 0));
            inner_name = owned_inner_name;
            elem_type = pergyra_type_to_llvm(ctx, inner_name);
        } else if (count > 0) {
            LLVMValueRef first = llvm_emit_expression(
                ast_array_literal_element(init, 0), ctx);
            if (first != NULL) {
                elem_type = LLVMTypeOf(first);
                const char *suffix = llvm_type_to_suffix(ctx, elem_type);
                if (suffix != NULL && strcmp(suffix, "Unknown") != 0)
                    inner_name = suffix;
            }
        }
        if (inner_name == NULL || inner_name[0] == '\0') {
            bool ok = llvm_stmt_diag_collection(ctx, node,
                LLVM_STMT_COLLECTION_DIAG_TYPE_ARG, name, "Array", 0, NULL);
            free(owned_inner_name);
            return ok;
        }
        if (elem_type == NULL) {
            bool ok = llvm_stmt_diag_collection(ctx, node,
                LLVM_STMT_COLLECTION_DIAG_TYPE_ARG, name, "Array", 0, NULL);
            free(owned_inner_name);
            return ok;
        }

        /* Canonicalize the element name to its runtime suffix so nested
         * Array<Array<Int>> keys on "Array_Int" (pgy_array_*_Array_Int) rather
         * than the un-mangled "Array<Int>". Identity for scalar suffixes. */
        {
            const char *canon = llvm_type_to_suffix(ctx, elem_type);
            if (canon != NULL && strcmp(canon, "Unknown") != 0
                && (inner_name == NULL || strcmp(inner_name, canon) != 0)) {
                char *owned_canon = pergyra_strdup(canon);
                if (owned_canon != NULL) {
                    free(owned_inner_name);
                    owned_inner_name = owned_canon;
                    inner_name = owned_inner_name;
                }
            }
        }

        LLVMTypeRef array_type = llvm_array_struct_type(ctx, inner_name);
        if (ctx->has_error || array_type == NULL) {
            free(owned_inner_name);
            return true;
        }
        LLVMValueRef var_alloca = llvm_create_entry_alloca(ctx, array_type, name);
        const char *primitive_suffix = llvm_type_to_suffix(ctx, elem_type);
        bool raw_nominal_array = primitive_suffix == NULL
            || strcmp(primitive_suffix, "Unknown") == 0;
        if (raw_nominal_array) {
            LLVMFuncEntry *raw_new_fn = llvm_lookup_function(ctx,
                "pgy_array_new_raw_export");
            LLVMFuncEntry *raw_push_fn = llvm_lookup_function(ctx,
                "pgy_array_push_raw_export");
            LLVMValueRef elem_size = LLVMSizeOf(elem_type);
            if (LLVMTypeOf(elem_size) != ctx->type_i64)
                elem_size = LLVMBuildZExtOrBitCast(ctx->builder, elem_size,
                    ctx->type_i64, llvm_tmp_name(ctx));
            if (raw_new_fn == NULL || raw_push_fn == NULL) {
                bool ok = llvm_stmt_diag_collection(ctx, node,
                    LLVM_STMT_COLLECTION_DIAG_RUNTIME_FN, name,
                    "Array", 0,
                    raw_new_fn == NULL ? "pgy_array_new_raw_export"
                                       : "pgy_array_push_raw_export");
                free(owned_inner_name);
                return ok;
            }
            LLVMValueRef raw_arr = LLVMBuildBitCast(ctx->builder, var_alloca,
                ctx->type_i8ptr, llvm_tmp_name(ctx));
            LLVMValueRef new_args[] = {
                raw_arr,
                LLVMConstInt(ctx->type_i64, (unsigned long long)count, 0),
                elem_size
            };
            LLVMBuildCall2(ctx->builder, raw_new_fn->fn_type,
                raw_new_fn->fn, new_args, 3, "");
            for (size_t i = 0; i < count; i++) {
                LLVMValueRef element = llvm_emit_expression(
                    ast_array_literal_element(init, i), ctx);
                if (element == NULL) {
                    free(owned_inner_name);
                    return true;
                }
                LLVMValueRef elem_alloca = llvm_create_entry_alloca(ctx,
                    elem_type, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, element, elem_alloca);
                LLVMValueRef raw_elem = LLVMBuildBitCast(ctx->builder,
                    elem_alloca, ctx->type_i8ptr, llvm_tmp_name(ctx));
                LLVMValueRef push_args[] = { raw_arr, raw_elem, elem_size };
                LLVMBuildCall2(ctx->builder, raw_push_fn->fn_type,
                    raw_push_fn->fn, push_args, 3, "");
            }
            llvm_scope_declare(ctx, name, var_alloca, array_type);
            llvm_register_array_var(ctx, name, elem_type, inner_name,
                (int64_t)count);
            free(owned_inner_name);
            return true;
        }
        char new_fn_name[64];
        char push_fn_name[64];
        LLVMFuncEntry *new_fn;
        LLVMFuncEntry *push_fn;

        if (!llvm_stmt_collection_runtime_name(ctx, node, new_fn_name,
                sizeof(new_fn_name), "pgy_array_new_", inner_name)) {
            free(owned_inner_name);
            return true;
        }
        new_fn = llvm_lookup_function(ctx, new_fn_name);
        if (new_fn == NULL) {
            bool ok = llvm_stmt_diag_collection(ctx, node,
                LLVM_STMT_COLLECTION_DIAG_RUNTIME_FN, name,
                "Array", 0, new_fn_name);
            free(owned_inner_name);
            return ok;
        }
        LLVMValueRef args[] = {
            LLVMConstInt(ctx->type_i64, (unsigned long long)count, 0)
        };
        LLVMValueRef arr_val = LLVMBuildCall2(ctx->builder, new_fn->fn_type,
            new_fn->fn, args, 1, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, arr_val, var_alloca);

        if (!llvm_stmt_collection_runtime_name(ctx, node, push_fn_name,
                sizeof(push_fn_name), "pgy_array_push_", inner_name)) {
            free(owned_inner_name);
            return true;
        }
        push_fn = llvm_lookup_function(ctx, push_fn_name);
        if (push_fn == NULL) {
            bool ok = llvm_stmt_diag_collection(ctx, node,
                LLVM_STMT_COLLECTION_DIAG_RUNTIME_FN, name,
                "Array", 0, push_fn_name);
            free(owned_inner_name);
            return ok;
        }

        for (size_t i = 0; i < count; i++) {
            LLVMValueRef element = llvm_emit_expression(
                ast_array_literal_element(init, i), ctx);
            if (element != NULL && LLVMTypeOf(element) != elem_type) {
                LLVMTypeRef element_type = LLVMTypeOf(element);
                bool target_is_int = (elem_type == ctx->type_i32
                                   || elem_type == ctx->type_i64);
                bool target_is_fp = (elem_type == ctx->type_f32
                                  || elem_type == ctx->type_f64);
                bool source_is_int = (element_type == ctx->type_i32
                                   || element_type == ctx->type_i64);
                bool source_is_fp = (element_type == ctx->type_f32
                                  || element_type == ctx->type_f64);

                if (target_is_int && source_is_fp)
                    element = llvm_build_checked_fptosi(ctx, element, elem_type,
                                              llvm_tmp_name(ctx));
                else if (target_is_fp && source_is_int)
                    element = LLVMBuildSIToFP(ctx->builder, element, elem_type,
                                              llvm_tmp_name(ctx));
            }
            if (element != NULL) {
                LLVMValueRef args[] = { var_alloca, element };
                LLVMBuildCall2(ctx->builder, push_fn->fn_type,
                    push_fn->fn, args, 2, "");
            }
        }

        llvm_scope_declare(ctx, name, var_alloca, array_type);
        llvm_register_array_var(ctx, name, elem_type, inner_name,
            (int64_t)count);
        free(owned_inner_name);
        return true;
    }

    return false;
}

#endif /* PGY_LLVM_ENABLED */
