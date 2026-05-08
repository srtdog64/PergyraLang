#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"

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

bool
llvm_stmt_emit_collection_like_let(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *name;
    ASTNode *type_ann;
    ASTNode *init;

    if (node == NULL || node->type != AST_LET_DECL || ctx == NULL)
        return false;

    name = node->data.let_decl.name;
    type_ann = node->data.let_decl.type;
    init = node->data.let_decl.initializer;

    if (type_ann != NULL
        && type_ann->type == AST_TYPE
        && type_ann->data.type.name != NULL
        && init != NULL
        && init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER
        && strcmp(init->data.call.callee->data.identifier.name, "ToObject") == 0
        && init->data.call.arg_count >= 2
        && init->data.call.arguments[1] != NULL
        && init->data.call.arguments[1]->type == AST_IDENTIFIER) {
        LLVMClassTypeEntry *target_cls = llvm_lookup_class(ctx, type_ann->data.type.name);
        if (target_cls != NULL
            && target_cls->is_immutable
            && !target_cls->is_boundary_transfer_contract) {
            const char *source_name = init->data.call.arguments[1]->data.identifier.name;
            llvm_register_var_class(ctx, name, type_ann->data.type.name);
            llvm_register_projection_borrow(ctx, name, type_ann->data.type.name, source_name);
            return true;
        }
    }

    if (type_ann != NULL
        && type_ann->type == AST_TYPE
        && type_ann->data.type.name != NULL
        && init != NULL
        && init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER) {
        const char *ann_name = type_ann->data.type.name;
        const char *callee = init->data.call.callee->data.identifier.name;
        char *inner = NULL;

        if (type_ann->data.type.generic_args != NULL
            && type_ann->data.type.generic_args->count > 0
            && type_ann->data.type.generic_args->params[0] != NULL) {
            inner = llvm_stmt_render_type_arg(type_ann->data.type.generic_args->params[0]);
        }

        if (strcmp(ann_name, "List") == 0 && strcmp(callee, "ListNew") == 0) {
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
            LLVMFuncEntry *new_fn = llvm_lookup_function(ctx, "pgy_list_new_raw_export");
            if (new_fn == NULL) {
                free(inner);
                return llvm_stmt_diag_collection(ctx, node,
                    LLVM_STMT_COLLECTION_DIAG_RUNTIME_FN, name,
                    "List", 0, "pgy_list_new_raw_export");
            }
            {
                LLVMValueRef args[] = {
                    LLVMBuildBitCast(ctx->builder, alloca_val, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                    llvm_sizeof_type_i64(ctx, elem_ty)
                };
                LLVMBuildCall2(ctx->builder, new_fn->fn_type, new_fn->fn, args, 2, "");
            }
            llvm_scope_declare(ctx, name, alloca_val, list_ty);
            llvm_register_list_var(ctx, name, inner);
            free(inner);
            return true;
        }

        if (strcmp(ann_name, "Set") == 0 && strcmp(callee, "SetNew") == 0) {
            if (inner == NULL || inner[0] == '\0') {
                bool ok = llvm_stmt_diag_collection(ctx, node,
                    LLVM_STMT_COLLECTION_DIAG_TYPE_ARG, name, ann_name, 0, NULL);
                free(inner);
                return ok;
            }
            LLVMTypeRef set_ty = ast_type_to_llvm(ctx, type_ann);
            LLVMTypeRef elem_ty = pergyra_type_to_llvm(ctx, inner);
            if (ctx->has_error || set_ty == NULL || elem_ty == NULL) {
                free(inner);
                return true;
            }
            LLVMValueRef alloca_val = llvm_create_entry_alloca(ctx, set_ty, name);
            LLVMFuncEntry *new_fn = llvm_lookup_function(ctx, "pgy_set_new_raw_export");
            if (new_fn == NULL) {
                free(inner);
                return llvm_stmt_diag_collection(ctx, node,
                    LLVM_STMT_COLLECTION_DIAG_RUNTIME_FN, name,
                    "Set", 0, "pgy_set_new_raw_export");
            }
            {
                LLVMValueRef args[] = {
                    LLVMBuildBitCast(ctx->builder, alloca_val, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                    llvm_sizeof_type_i64(ctx, elem_ty)
                };
                LLVMBuildCall2(ctx->builder, new_fn->fn_type, new_fn->fn, args, 2, "");
            }
            llvm_scope_declare(ctx, name, alloca_val, set_ty);
            llvm_register_set_var(ctx, name, inner);
            free(inner);
            return true;
        }

        if (strcmp(ann_name, "Queue") == 0 && strcmp(callee, "QueueNew") == 0) {
            if (inner == NULL || inner[0] == '\0') {
                bool ok = llvm_stmt_diag_collection(ctx, node,
                    LLVM_STMT_COLLECTION_DIAG_TYPE_ARG, name, ann_name, 0, NULL);
                free(inner);
                return ok;
            }
            LLVMTypeRef queue_ty = ast_type_to_llvm(ctx, type_ann);
            LLVMTypeRef elem_ty = pergyra_type_to_llvm(ctx, inner);
            if (ctx->has_error || queue_ty == NULL || elem_ty == NULL) {
                free(inner);
                return true;
            }
            LLVMValueRef alloca_val = llvm_create_entry_alloca(ctx, queue_ty, name);
            LLVMFuncEntry *new_fn = llvm_lookup_function(ctx, "pgy_queue_new_raw_export");
            if (new_fn == NULL) {
                free(inner);
                return llvm_stmt_diag_collection(ctx, node,
                    LLVM_STMT_COLLECTION_DIAG_RUNTIME_FN, name,
                    "Queue", 0, "pgy_queue_new_raw_export");
            }
            {
                LLVMValueRef args[] = {
                    LLVMBuildBitCast(ctx->builder, alloca_val, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                    llvm_sizeof_type_i64(ctx, elem_ty)
                };
                LLVMBuildCall2(ctx->builder, new_fn->fn_type, new_fn->fn, args, 2, "");
            }
            llvm_scope_declare(ctx, name, alloca_val, queue_ty);
            llvm_register_queue_var(ctx, name, inner);
            free(inner);
            return true;
        }

        if (strcmp(ann_name, "HashMap") == 0 && strcmp(callee, "MapNew") == 0) {
            char *value_type = NULL;
            char *key_type = NULL;
            LLVMTypeRef map_ty = ast_type_to_llvm(ctx, type_ann);
            LLVMTypeRef value_ty;
            LLVMValueRef alloca_val;
            LLVMFuncEntry *new_fn;

            if (type_ann->data.type.generic_args != NULL
                && type_ann->data.type.generic_args->count > 0
                && type_ann->data.type.generic_args->params[0] != NULL) {
                key_type = llvm_stmt_render_type_arg(
                    type_ann->data.type.generic_args->params[0]);
            }
            if (type_ann->data.type.generic_args != NULL
                && type_ann->data.type.generic_args->count > 1
                && type_ann->data.type.generic_args->params[1] != NULL) {
                value_type = llvm_stmt_render_type_arg(
                    type_ann->data.type.generic_args->params[1]);
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
            new_fn = llvm_lookup_function(ctx, "pgy_map_new_raw_export");
            if (new_fn == NULL) {
                bool ok = llvm_stmt_diag_collection(ctx, node,
                    LLVM_STMT_COLLECTION_DIAG_RUNTIME_FN, name,
                    "HashMap", 0, "pgy_map_new_raw_export");
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
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER
        && strcmp(init->data.call.callee->data.identifier.name, "Channel") == 0) {
        char *channel_inner = NULL;
        char init_fn_name[128];
        LLVMTypeRef ch_type = LLVMArrayType(
            LLVMInt8TypeInContext(ctx->context), 256);
        LLVMValueRef alloca_val = llvm_create_entry_alloca(ctx, ch_type, name);

        if (type_ann == NULL || type_ann->type != AST_TYPE
            || type_ann->data.type.generic_args == NULL
            || type_ann->data.type.generic_args->count == 0
            || type_ann->data.type.generic_args->params[0] == NULL) {
            return llvm_stmt_diag_collection(ctx, node,
                LLVM_STMT_COLLECTION_DIAG_TYPE_ARG, name, "Channel", 0, NULL);
        }

        channel_inner = llvm_stmt_render_type_arg(
            type_ann->data.type.generic_args->params[0]);
        if (channel_inner == NULL || channel_inner[0] == '\0') {
            return llvm_stmt_diag_collection(ctx, node,
                LLVM_STMT_COLLECTION_DIAG_TYPE_ARG, name, "Channel", 0, NULL);
        }

        snprintf(init_fn_name, sizeof(init_fn_name),
            "pgy_channel_init_%s", channel_inner);
        LLVMFuncEntry *init_fn = llvm_lookup_function(ctx, init_fn_name);
        if (init_fn == NULL) {
            bool ok = llvm_stmt_diag_collection(ctx, node,
                LLVM_STMT_COLLECTION_DIAG_RUNTIME_FN, name,
                "Channel", 0, init_fn_name);
            free(channel_inner);
            return ok;
        }
        LLVMValueRef cap = LLVMConstInt(ctx->type_i64, 16, 0);
        if (init->data.call.arg_count > 0)
            cap = LLVMBuildZExt(ctx->builder,
                llvm_emit_expression(init->data.call.arguments[0], ctx),
                ctx->type_i64, llvm_tmp_name(ctx));
        LLVMValueRef args[] = { alloca_val, cap };
        LLVMBuildCall2(ctx->builder, init_fn->fn_type,
                       init_fn->fn, args, 2, "");
        llvm_scope_declare(ctx, name, alloca_val, ch_type);
        llvm_register_channel_var(ctx, name, channel_inner);
        return true;
    }

    if (init != NULL && init->type == AST_ARRAY_LITERAL) {
        size_t count = init->data.array_literal.count;
        LLVMTypeRef elem_type = ctx->type_i32;
        const char *inner_name = NULL;

        if (type_ann != NULL && type_ann->type == AST_TYPE
            && type_ann->data.type.name != NULL
            && (strcmp(type_ann->data.type.name, "Array") == 0
                || strcmp(type_ann->data.type.name, "Slice") == 0)
            && type_ann->data.type.generic_args != NULL
            && type_ann->data.type.generic_args->count > 0) {
            inner_name = type_ann->data.type.generic_args->params[0]->name;
            elem_type = pergyra_type_to_llvm(
                ctx, inner_name);
        } else if (count > 0) {
            LLVMValueRef first = llvm_emit_expression(
                init->data.array_literal.elements[0], ctx);
            if (first != NULL) {
                elem_type = LLVMTypeOf(first);
                const char *suffix = llvm_type_to_suffix(ctx, elem_type);
                if (suffix != NULL && strcmp(suffix, "Unknown") != 0)
                    inner_name = suffix;
            }
        }
        if (inner_name == NULL || inner_name[0] == '\0') {
            return llvm_stmt_diag_collection(ctx, node,
                LLVM_STMT_COLLECTION_DIAG_TYPE_ARG, name, "Array", 0, NULL);
        }

        LLVMTypeRef array_type = llvm_array_struct_type(ctx, inner_name);
        if (ctx->has_error || array_type == NULL)
            return true;
        LLVMValueRef var_alloca = llvm_create_entry_alloca(ctx, array_type, name);
        char new_fn_name[64];
        char push_fn_name[64];
        LLVMFuncEntry *new_fn;
        LLVMFuncEntry *push_fn;

        snprintf(new_fn_name, sizeof(new_fn_name), "pgy_array_new_%s", inner_name);
        new_fn = llvm_lookup_function(ctx, new_fn_name);
        if (new_fn == NULL) {
            return llvm_stmt_diag_collection(ctx, node,
                LLVM_STMT_COLLECTION_DIAG_RUNTIME_FN, name,
                "Array", 0, new_fn_name);
        }
        LLVMValueRef args[] = {
            LLVMConstInt(ctx->type_i64, (unsigned long long)count, 0)
        };
        LLVMValueRef arr_val = LLVMBuildCall2(ctx->builder, new_fn->fn_type,
            new_fn->fn, args, 1, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, arr_val, var_alloca);

        snprintf(push_fn_name, sizeof(push_fn_name), "pgy_array_push_%s", inner_name);
        push_fn = llvm_lookup_function(ctx, push_fn_name);
        if (push_fn == NULL) {
            return llvm_stmt_diag_collection(ctx, node,
                LLVM_STMT_COLLECTION_DIAG_RUNTIME_FN, name,
                "Array", 0, push_fn_name);
        }

        for (size_t i = 0; i < count; i++) {
            LLVMValueRef element = llvm_emit_expression(
                init->data.array_literal.elements[i], ctx);
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
                    element = LLVMBuildFPToSI(ctx->builder, element, elem_type,
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
        llvm_register_array_var(ctx, name, elem_type, (int64_t)count);
        return true;
    }

    return false;
}

#endif /* PGY_LLVM_ENABLED */
