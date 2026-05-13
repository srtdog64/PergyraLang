#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "codegen_slot_type_policy.h"
#include "llvm_stmt_let_names.h"

void
llvm_emit_let_decl(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *name = node->data.let_decl.name;
    ASTNode *type_ann = node->data.let_decl.type;
    ASTNode *init     = node->data.let_decl.initializer;
    const char *spawn_future_inner = NULL;

    if (llvm_stmt_emit_claim_slot_let(node, ctx))
        return;

    if (llvm_stmt_emit_view_or_move_let(node, ctx))
        return;

    if (llvm_stmt_emit_collection_like_let(node, ctx))
        return;

    if (llvm_stmt_emit_slot_sugar_let(node, ctx))
        return;

    /* Detect class constructor: let v = ClassName(args...) */
    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER) {
        const char *callee = init->data.call.callee->data.identifier.name;
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, callee);
        if (cls != NULL) {
            LLVMValueRef alloca_val = llvm_create_entry_alloca(
                ctx, cls->struct_type, name);
            LLVMValueRef init_val = llvm_emit_expression(init, ctx);
            if (init_val != NULL)
                LLVMBuildStore(ctx->builder, init_val, alloca_val);

            llvm_scope_declare(ctx, name, alloca_val, cls->struct_type);
            llvm_register_var_class(ctx, name, callee);
            return;
        }
    }

    /* Determine type from annotation or initializer */
    LLVMTypeRef var_type = ctx->type_i32; /* default */
    if (type_ann != NULL) {
        var_type = ast_type_to_llvm(ctx, type_ann);
        if (ctx->has_error || var_type == NULL)
            return;
    } else if (init != NULL && init->type == AST_SPAWN_EXPR) {
        var_type = ctx->type_task_handle;
        spawn_future_inner = llvm_infer_spawn_future_inner(ctx, init);
    } else if (init != NULL) {
        var_type = llvm_stmt_infer_expr_type(ctx, init);
        if (ctx->has_error || var_type == NULL)
            return;
    }
    if (init != NULL && init->type == AST_LAMBDA_EXPR) {
        LLVMTypeRef lambda_type = llvm_stmt_lambda_signature_type(ctx, init);
        if (ctx->has_error || lambda_type == NULL)
            return;
        if (lambda_type != NULL)
            var_type = lambda_type;
    }

    /* Create alloca at function entry */
    LLVMValueRef alloca = llvm_create_entry_alloca(ctx, var_type, name);

    /* Store initializer if present */
    if (init != NULL) {
        if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
            fprintf(stderr, "[llvm let] name=%s phase=before-init type-kind=%d\n",
                name != NULL ? name : "-", (int)LLVMGetTypeKind(var_type));
        LLVMTypeRef saved_expected_type = ctx->current_ret_type;
        ctx->current_ret_type = var_type;
        /* Propagate the source-level type annotation so Result<T,E>
         * construction inside the initializer can recover T/E from the
         * let binding context (parity with C backend's expected_type). */
        const char *saved_expected_name = ctx->expected_type_name;
        if (type_ann != NULL && type_ann->type == AST_TYPE
            && type_ann->data.type.name != NULL)
            ctx->expected_type_name =
                llvm_stmt_render_type_annotation_static(type_ann);
        LLVMValueRef val = llvm_emit_expression(init, ctx);
        if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
            fprintf(stderr, "[llvm let] name=%s phase=after-init val=%p\n",
                name != NULL ? name : "-", (void *)val);
        ctx->expected_type_name = saved_expected_name;
        ctx->current_ret_type = saved_expected_type;
        if (val != NULL) {
            LLVMTypeRef val_type = LLVMTypeOf(val);
            if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
                fprintf(stderr,
                    "[llvm let] name=%s phase=before-store var-kind=%d val-kind=%d\n",
                    name != NULL ? name : "-",
                    (int)LLVMGetTypeKind(var_type),
                    (int)LLVMGetTypeKind(val_type));

            /* Type coercion between numeric types */
            if (var_type != val_type) {
                bool var_is_int = (var_type == ctx->type_i32 || var_type == ctx->type_i64);
                bool var_is_fp  = (var_type == ctx->type_f32 || var_type == ctx->type_f64);
                bool val_is_int = (val_type == ctx->type_i32 || val_type == ctx->type_i64);
                bool val_is_fp  = (val_type == ctx->type_f32 || val_type == ctx->type_f64);

                if (var_is_int && val_is_fp)
                    val = LLVMBuildFPToSI(ctx->builder, val, var_type,
                                           llvm_tmp_name(ctx));
                else if (var_is_fp && val_is_int)
                    val = LLVMBuildSIToFP(ctx->builder, val, var_type,
                                           llvm_tmp_name(ctx));
                else if (var_is_int && val_is_int)
                    val = (LLVMGetIntTypeWidth(var_type) > LLVMGetIntTypeWidth(val_type))
                        ? LLVMBuildSExt(ctx->builder, val, var_type, llvm_tmp_name(ctx))
                        : LLVMBuildTrunc(ctx->builder, val, var_type, llvm_tmp_name(ctx));
                else if (var_is_fp && val_is_fp)
                    val = (var_type == ctx->type_f64)
                        ? LLVMBuildFPExt(ctx->builder, val, var_type, llvm_tmp_name(ctx))
                        : LLVMBuildFPTrunc(ctx->builder, val, var_type, llvm_tmp_name(ctx));
            }

            LLVMBuildStore(ctx->builder, val, alloca);
            if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
                fprintf(stderr, "[llvm let] name=%s phase=after-store\n",
                    name != NULL ? name : "-");
        }
    }

    llvm_scope_declare(ctx, name, alloca, var_type);
    if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
        fprintf(stderr, "[llvm let] name=%s phase=after-scope-declare\n",
            name != NULL ? name : "-");

    if (!llvm_stmt_register_callable_let_binding(node, ctx))
        return;

    {
        LLVMClassTypeEntry *value_cls = llvm_stmt_lookup_class_by_type(ctx, var_type);
        if (value_cls != NULL)
            llvm_register_var_class(ctx, name, value_cls->class_name);
    }

    if (type_ann != NULL && type_ann->type == AST_TYPE
        && type_ann->data.type.name != NULL) {
        LLVMClassTypeEntry *ann_cls = llvm_lookup_class(ctx, type_ann->data.type.name);
        if (ann_cls != NULL)
            llvm_register_var_class(ctx, name, type_ann->data.type.name);
    } else if (init != NULL) {
        const char *inferred_nominal = llvm_stmt_infer_nominal_name_from_init(ctx, init);
        LLVMClassTypeEntry *inferred_cls = inferred_nominal != NULL
            ? llvm_lookup_class(ctx, inferred_nominal) : NULL;
        if (inferred_cls != NULL)
            llvm_register_var_class(ctx, name, inferred_nominal);
    }

    if (type_ann != NULL && type_ann->type == AST_TYPE
        && type_ann->data.type.name != NULL
        && (strcmp(type_ann->data.type.name, "Array") == 0
            || strcmp(type_ann->data.type.name, "Slice") == 0)
        && type_ann->data.type.generic_args != NULL
        && type_ann->data.type.generic_args->count > 0) {
        char *elem_name = llvm_stmt_render_type_arg_scratch(
            type_ann->data.type.generic_args->params[0],
            &ctx->scratch);
        if (elem_name == NULL || elem_name[0] == '\0') {
            llvm_stmt_require_let_type_arg(ctx, node, name,
                type_ann->data.type.name);
            return;
        }
        LLVMTypeRef elem_type = pergyra_type_to_llvm(ctx, elem_name);
        llvm_register_array_var(ctx, name, elem_type, -1);
    } else if (init != NULL
        && init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_MEMBER_ACCESS
        && init->data.call.callee->data.member.name != NULL
        && strcmp(init->data.call.callee->data.member.name, "Slice") == 0
        && init->data.call.callee->data.member.object != NULL) {
        LLVMTypeRef elem_type = llvm_stmt_resolve_array_elem_type(
            ctx, init->data.call.callee->data.member.object, NULL);
        llvm_register_array_var(ctx, name, elem_type, -1);
        if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
            fprintf(stderr, "[llvm let] name=%s phase=after-slice-register\n",
                name != NULL ? name : "-");
    }

    if (type_ann != NULL && type_ann->type == AST_TYPE
        && type_ann->data.type.name != NULL) {
        const char *ann_name = type_ann->data.type.name;
        if (strcmp(ann_name, "List") == 0
            && type_ann->data.type.generic_args != NULL
            && type_ann->data.type.generic_args->count > 0
            && type_ann->data.type.generic_args->params[0] != NULL) {
            char *inner_name = llvm_stmt_render_type_arg(
                type_ann->data.type.generic_args->params[0]);
            if (inner_name == NULL || inner_name[0] == '\0') {
                llvm_stmt_require_let_type_arg(ctx, node, name, ann_name);
                free(inner_name);
                return;
            }
            llvm_register_list_var(ctx, name, inner_name);
            free(inner_name);
        } else if (strcmp(ann_name, "Queue") == 0
            && type_ann->data.type.generic_args != NULL
            && type_ann->data.type.generic_args->count > 0
            && type_ann->data.type.generic_args->params[0] != NULL) {
            char *inner_name = llvm_stmt_render_type_arg(
                type_ann->data.type.generic_args->params[0]);
            if (inner_name == NULL || inner_name[0] == '\0') {
                llvm_stmt_require_let_type_arg(ctx, node, name, ann_name);
                free(inner_name);
                return;
            }
            llvm_register_queue_var(ctx, name, inner_name);
            free(inner_name);
        } else if (strcmp(ann_name, "HashMap") == 0
            && type_ann->data.type.generic_args != NULL
            && type_ann->data.type.generic_args->count > 1
            && type_ann->data.type.generic_args->params[0] != NULL
            && type_ann->data.type.generic_args->params[1] != NULL) {
            char *key_name = llvm_stmt_render_type_arg(
                type_ann->data.type.generic_args->params[0]);
            char *value_name = llvm_stmt_render_type_arg(
                type_ann->data.type.generic_args->params[1]);
            if (key_name == NULL || key_name[0] == '\0') {
                llvm_stmt_require_let_type_arg(ctx, node, name, ann_name);
                free(key_name);
                free(value_name);
                return;
            }
            if (value_name == NULL || value_name[0] == '\0') {
                llvm_stmt_require_let_type_arg(ctx, node, name, ann_name);
                free(key_name);
                free(value_name);
                return;
            }
            llvm_register_map_var(ctx, name, key_name, value_name);
            free(key_name);
            free(value_name);
        } else if (strcmp(ann_name, "Rc") == 0
            || strcmp(ann_name, "Weak") == 0
            || strncmp(ann_name, "Rc<", 3) == 0
            || strncmp(ann_name, "Weak<", 5) == 0) {
            llvm_register_typed_var(ctx, name, type_ann);
        }
    }

    if (type_ann != NULL && type_ann->type == AST_TYPE
        && type_ann->data.type.name != NULL
        && type_ann->data.type.generic_args != NULL
        && type_ann->data.type.generic_args->count > 0) {
        const char *ann_name = type_ann->data.type.name;
        if (strcmp(ann_name, "Future") == 0
            || strcmp(ann_name, "RemoteFuture") == 0) {
            char *future_inner = NULL;
            if (type_ann->data.type.generic_args->params != NULL
                && type_ann->data.type.generic_args->params[0] != NULL) {
                future_inner = llvm_stmt_render_type_arg(
                    type_ann->data.type.generic_args->params[0]);
            }
            if (future_inner == NULL || future_inner[0] == '\0') {
                llvm_stmt_require_let_type_arg(ctx, node, name, ann_name);
                free(future_inner);
                return;
            }
            llvm_register_future_var(ctx, name, future_inner,
                strcmp(ann_name, "RemoteFuture") == 0);
            free(future_inner);
        }
    } else if (init != NULL && init->type == AST_CALL
               && init->data.call.callee != NULL
               && init->data.call.callee->type == AST_IDENTIFIER
               && strcmp(init->data.call.callee->data.identifier.name,
                         "SubmitDeviceRead") == 0) {
        const char *inner = NULL;
        ASTNode *slot_arg = init->data.call.arguments[0];
        if (slot_arg != NULL && slot_arg->type == AST_IDENTIFIER) {
            const char *tracked = llvm_lookup_device_slot_inner(
                ctx, slot_arg->data.identifier.name);
            if (tracked != NULL)
                inner = tracked;
        }
        if (inner == NULL || inner[0] == '\0') {
            if (!ctx->has_error) {
                llvm_set_error_at_with_hints(ctx, init,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "LLVM SubmitDeviceRead future binding '%s' requires concrete DeviceSlot<T> metadata",
                    name != NULL ? name : "<binding>");
            }
            return;
        }
        llvm_register_future_var(ctx, name, inner, true);
    } else if (init != NULL && init->type == AST_SPAWN_EXPR) {
        if (spawn_future_inner == NULL || spawn_future_inner[0] == '\0') {
            if (!ctx->has_error) {
                llvm_set_error_at_with_hints(ctx, init,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "LLVM spawn binding '%s' requires an explicit Future<T> annotation or an inferable spawned function return type",
                    name != NULL ? name : "<binding>");
            }
            return;
        }
        llvm_register_future_var(ctx, name, spawn_future_inner, false);
    }

    /* Track class type for member access */
    if (type_ann != NULL && type_ann->type == AST_TYPE
        && type_ann->data.type.name != NULL) {
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx,
            type_ann->data.type.name);
        if (cls != NULL)
            llvm_register_var_class(ctx, name, type_ann->data.type.name);
    } else if (init != NULL) {
        const char *nominal_name = llvm_stmt_infer_nominal_name_from_init(ctx, init);
        LLVMClassTypeEntry *nominal_cls = nominal_name != NULL
            ? llvm_lookup_class(ctx, nominal_name) : NULL;
        if (nominal_cls != NULL) {
            llvm_register_var_class(ctx, name, nominal_name);
        }
    }
}


#endif /* PGY_LLVM_ENABLED */
