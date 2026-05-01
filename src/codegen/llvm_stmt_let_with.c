#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "codegen_slot_type_policy.h"

static bool
llvm_stmt_require_let_type_arg(LLVMGenCtx *ctx, ASTNode *node,
                               const char *binding_name,
                               const char *container_name)
{
    if (ctx == NULL)
        return false;
    if (!ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM %s let-binding for '%s' requires an explicit concrete %s<T> annotation",
            container_name != NULL ? container_name : "typed",
            binding_name != NULL ? binding_name : "<binding>",
            container_name != NULL ? container_name : "type");
    }
    return false;
}

void
llvm_emit_let_decl(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *name = node->data.let_decl.name;
    ASTNode *type_ann = node->data.let_decl.type;
    ASTNode *init     = node->data.let_decl.initializer;
    const char *spawn_future_inner = NULL;

    if (llvm_stmt_emit_claim_slot_let(node, ctx))
        return;

    if (type_ann != NULL && type_ann->type == AST_TYPE
        && type_ann->data.type.name != NULL
        && init != NULL && init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER
        && init->data.call.arg_count >= 1
        && init->data.call.arguments[0] != NULL
        && init->data.call.arguments[0]->type == AST_IDENTIFIER) {
        const char *ann_name = type_ann->data.type.name;
        const char *callee = init->data.call.callee->data.identifier.name;
        const char *source_name = init->data.call.arguments[0]->data.identifier.name;
        bool alias_decl =
            (pgy_codegen_type_name_is_read_view(ann_name)
             && pgy_codegen_call_name_is_view_read(callee))
            || (pgy_codegen_type_name_is_write_view(ann_name)
                && pgy_codegen_call_name_is_view_write(callee))
            || ((strcmp(ann_name, "MoveToken") == 0 || strncmp(ann_name, "MoveToken<", 10) == 0)
                && strcmp(callee, "Move") == 0);
        if (alias_decl) {
            char *inner = NULL;
            if (type_ann->data.type.generic_args != NULL
                && type_ann->data.type.generic_args->count > 0
                && type_ann->data.type.generic_args->params[0] != NULL)
                inner = llvm_stmt_render_type_arg(type_ann->data.type.generic_args->params[0]);
            if (inner == NULL || inner[0] == '\0') {
                llvm_stmt_require_let_type_arg(ctx, node, name, ann_name);
                return;
            }

            LLVMVarEntry *source = llvm_scope_lookup(ctx, source_name);
            if (source == NULL)
                return;

            bool is_move = (strcmp(callee, "Move") == 0);

            if (is_move) {
                /* Move: structural copy; the new alloca owns the data,
                 * source is invalidated */
                LLVMTypeRef slot_ty = llvm_slot_struct_type(ctx, inner);
                LLVMValueRef alloca_val = llvm_create_entry_alloca(
                    ctx, slot_ty, name);
                LLVMValueRef moved = LLVMBuildLoad2(ctx->builder,
                    source->type, source->alloca, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, moved, alloca_val);
                llvm_scope_declare(ctx, name, alloca_val, slot_ty);
                /* Mark source as consumed */
                for (int i = 0; i < ctx->slot_var_count; i++) {
                    if (strcmp(ctx->slot_vars[i].var_name, source_name) == 0) {
                        ctx->slot_vars[i].released = true;
                        break;
                    }
                }
            } else {
                /* ReadView / WriteView: non-owning alias; share the source
                 * slot's alloca directly.
                 * No separate storage; reads/writes go through
                 * the same address as the owning slot. */
                llvm_scope_declare(ctx, name, source->alloca, source->type);
            }
            llvm_register_view_var(ctx, name, source_name, inner, is_move);
            return;
        }
    }

    if (llvm_stmt_emit_collection_like_let(node, ctx))
        return;

    /* Slot sugar: let x: Slot<Int> = 42; auto Claim + Write. */
    if (type_ann != NULL && type_ann->type == AST_TYPE
        && type_ann->data.type.name != NULL) {
        const char *ann_name = type_ann->data.type.name;
        bool is_slot_sugar = (strcmp(ann_name, "Slot") == 0
                           || strncmp(ann_name, "Slot<", 5) == 0);
        bool is_secure_slot_sugar = (strcmp(ann_name, "SecureSlot") == 0
                                  || strncmp(ann_name, "SecureSlot<", 11) == 0);
        if (is_slot_sugar || is_secure_slot_sugar) {
            char *inner = NULL;
            bool is_secure = is_secure_slot_sugar;
            if (type_ann->data.type.generic_args != NULL
                && type_ann->data.type.generic_args->count > 0
                && type_ann->data.type.generic_args->params[0] != NULL)
                inner = llvm_stmt_render_type_arg(
                    type_ann->data.type.generic_args->params[0]);
            if (inner == NULL || inner[0] == '\0') {
                llvm_stmt_require_let_type_arg(ctx, node, name, ann_name);
                return;
            }

            if (init != NULL && init->type == AST_IDENTIFIER) {
                LLVMViewVarEntry *move_entry = llvm_lookup_view_var(ctx,
                    init->data.identifier.name);
                if (move_entry != NULL && move_entry->is_move_token) {
                    LLVMTypeRef slot_ty = is_secure
                        ? llvm_secure_slot_struct_type(ctx, inner)
                        : llvm_slot_struct_type(ctx, inner);
                    LLVMValueRef alloca_val = llvm_stmt_create_slot_alloca(ctx, slot_ty, name);
                    LLVMVarEntry *source = llvm_scope_lookup(ctx, init->data.identifier.name);
                    if (source == NULL)
                        return;
                    LLVMValueRef moved = LLVMBuildLoad2(ctx->builder, source->type, source->alloca,
                        llvm_tmp_name(ctx));
                    LLVMBuildStore(ctx->builder, moved, alloca_val);
                    llvm_scope_declare(ctx, name, alloca_val, slot_ty);
                    llvm_register_slot_var(ctx, name, inner, is_secure);
                    if (is_secure) {
                        LLVMTypeRef token_ty = llvm_secure_token_type(ctx, inner);
                        char token_name[256];
                        snprintf(token_name, sizeof(token_name), "%s_token", name);
                        LLVMValueRef token_alloca = llvm_stmt_create_slot_alloca(ctx, token_ty, token_name);
                        LLVMBuildStore(ctx->builder, LLVMConstNull(token_ty), token_alloca);
                        llvm_scope_declare(ctx, pergyra_strdup(token_name), token_alloca, token_ty);
                    }
                    return;
                }
            }

            LLVMTypeRef slot_ty = is_secure
                ? llvm_secure_slot_struct_type(ctx, inner)
                : llvm_slot_struct_type(ctx, inner);
            LLVMValueRef alloca_val = llvm_stmt_create_slot_alloca(ctx, slot_ty, name);

            /* Inline Claim: zero-init + set claimed=true */
            LLVMBuildStore(ctx->builder, LLVMConstNull(slot_ty), alloca_val);
            LLVMValueRef claimed_ptr = LLVMBuildStructGEP2(ctx->builder,
                slot_ty, alloca_val, 1, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
                claimed_ptr);

            if (is_secure) {
                LLVMTypeRef token_ty = llvm_secure_token_type(ctx, inner);
                char token_name[256];
                LLVMValueRef token_alloca;
                LLVMValueRef slot_ptr_i64;
                LLVMValueRef token_id;
                LLVMValueRef slot_token_ptr;
                LLVMValueRef token_id_ptr;
                LLVMValueRef token_write_ptr;
                LLVMValueRef token_read_ptr;

                snprintf(token_name, sizeof(token_name), "%s_token", name);
                token_alloca = llvm_stmt_create_slot_alloca(ctx, token_ty, token_name);
                LLVMBuildStore(ctx->builder, LLVMConstNull(token_ty), token_alloca);

                slot_ptr_i64 = LLVMBuildPtrToInt(ctx->builder,
                    alloca_val, ctx->type_i64, llvm_tmp_name(ctx));
                token_id = LLVMBuildXor(ctx->builder, slot_ptr_i64,
                    LLVMConstInt(ctx->type_i64, 0xDEADBEEFCAFEBABEULL, 0),
                    llvm_tmp_name(ctx));

                slot_token_ptr = LLVMBuildStructGEP2(ctx->builder,
                    slot_ty, alloca_val, 2, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, token_id, slot_token_ptr);

                token_id_ptr = LLVMBuildStructGEP2(ctx->builder,
                    token_ty, token_alloca, 0, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, token_id, token_id_ptr);

                token_write_ptr = LLVMBuildStructGEP2(ctx->builder,
                    token_ty, token_alloca, 1, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
                    token_write_ptr);

                token_read_ptr = LLVMBuildStructGEP2(ctx->builder,
                    token_ty, token_alloca, 2, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
                    token_read_ptr);

                llvm_scope_declare(ctx, pergyra_strdup(token_name), token_alloca, token_ty);
            }

            llvm_scope_declare(ctx, name, alloca_val, slot_ty);
            llvm_register_slot_var(ctx, name, inner, is_secure);

            /* Auto Write the initializer value */
            if (init != NULL) {
                LLVMValueRef val = llvm_emit_expression(init, ctx);
                if (val != NULL) {
                    char fn_name[64];
                    snprintf(fn_name, sizeof(fn_name),
                        is_secure ? "pgy_secure_write_%s" : "pgy_write_%s", inner);
                    LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
                    if (fn != NULL) {
                        if (is_secure) {
                            char token_name[256];
                            LLVMVarEntry *token_var;
                            snprintf(token_name, sizeof(token_name), "%s_token", name);
                            token_var = llvm_scope_lookup(ctx, token_name);
                            if (token_var != NULL) {
                                LLVMValueRef args[] = { alloca_val, val, token_var->alloca };
                                LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
                            }
                        } else {
                            LLVMValueRef args[] = { alloca_val, val };
                            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
                        }
                    } else {
                        LLVMValueRef value_ptr = LLVMBuildStructGEP2(ctx->builder,
                            slot_ty, alloca_val, 0, llvm_tmp_name(ctx));
                        LLVMValueRef occ_ptr = LLVMBuildStructGEP2(ctx->builder,
                            slot_ty, alloca_val, 1, llvm_tmp_name(ctx));
                        LLVMBuildStore(ctx->builder, val, value_ptr);
                        LLVMBuildStore(ctx->builder,
                            LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
                            occ_ptr);
                        if (is_secure) {
                            LLVMValueRef token_ptr = LLVMBuildStructGEP2(ctx->builder,
                                slot_ty, alloca_val, 2, llvm_tmp_name(ctx));
                            LLVMBuildStore(ctx->builder,
                                LLVMBuildLoad2(ctx->builder, ctx->type_i64, token_ptr,
                                    llvm_tmp_name(ctx)),
                                token_ptr);
                        }
                    }
                }
            }
            return;
        }
    }

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
    if (type_ann != NULL)
        var_type = ast_type_to_llvm(ctx, type_ann);
    else if (init != NULL && init->type == AST_SPAWN_EXPR) {
        var_type = ctx->type_task_handle;
        spawn_future_inner = llvm_infer_spawn_future_inner(ctx, init);
    } else if (init != NULL) {
        var_type = llvm_stmt_infer_expr_type(ctx, init);
    }
    if (init != NULL && init->type == AST_LAMBDA_EXPR) {
        LLVMTypeRef lambda_type = llvm_stmt_lambda_signature_type(ctx, init);
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
                return;
            }
            llvm_register_future_var(ctx, name, future_inner,
                strcmp(ann_name, "RemoteFuture") == 0);
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
