#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
void
llvm_emit_let_decl(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *name = node->data.let_decl.name;
    ASTNode *type_ann = node->data.let_decl.type;
    ASTNode *init     = node->data.let_decl.initializer;
    const char *spawn_future_inner = NULL;

    /* Detect ClaimSlot / ClaimSecureSlot / ClaimDeviceSlot */
    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER) {
        const char *callee = init->data.call.callee->data.identifier.name;
        if (strcmp(callee, "ClaimSlot") == 0
            || strcmp(callee, "ClaimSecureSlot") == 0) {
            /* Resolve inner type from explicit type annotation. */
            const char *inner = NULL;
            bool is_secure = (strcmp(callee, "ClaimSecureSlot") == 0);
            if (type_ann != NULL && type_ann->type == AST_TYPE) {
                if (type_ann->data.type.generic_args != NULL
                    && type_ann->data.type.generic_args->count > 0)
                    inner = type_ann->data.type.generic_args->params[0]->name;
            }
            if (inner == NULL) {
                llvm_set_error_at_with_hints(ctx, node, PGY_CODE_LLVM_TYPE_UNSUPPORTED, PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING, PGY_FIX_ANNOTATE_CONCRETE_TYPE, "LLVM %s let-binding for '%s' requires an explicit %s<T> annotation",
                    callee,
                    name != NULL ? name : "<slot>",
                    is_secure ? "SecureSlot" : "Slot");
                return;
            }

            LLVMTypeRef slot_ty = is_secure
                ? llvm_secure_slot_struct_type(ctx, inner)
                : llvm_slot_struct_type(ctx, inner);
            LLVMValueRef alloca_val = llvm_stmt_create_slot_alloca(ctx, slot_ty, name);

            /* Inline Claim: initialize the concrete slot storage directly.
             * SecureSlot also materializes a token bound to the owning alloca. */
            LLVMBuildStore(ctx->builder, LLVMConstNull(slot_ty), alloca_val);
            LLVMValueRef claimed_ptr = LLVMBuildStructGEP2(ctx->builder,
                slot_ty, alloca_val, 1, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
                claimed_ptr);

            if (is_secure) {
                LLVMTypeRef token_ty = llvm_secure_token_type(ctx, inner);
                char token_name[256];
                snprintf(token_name, sizeof(token_name), "%s_token", name);
                LLVMValueRef token_alloca = llvm_stmt_create_slot_alloca(ctx, token_ty, token_name);
                LLVMBuildStore(ctx->builder, LLVMConstNull(token_ty), token_alloca);

                LLVMValueRef slot_ptr_i64 = LLVMBuildPtrToInt(ctx->builder,
                    alloca_val, ctx->type_i64, llvm_tmp_name(ctx));
                LLVMValueRef token_id = LLVMBuildXor(ctx->builder, slot_ptr_i64,
                    LLVMConstInt(ctx->type_i64, 0xDEADBEEFCAFEBABEULL, 0),
                    llvm_tmp_name(ctx));

                LLVMValueRef slot_token_ptr = LLVMBuildStructGEP2(ctx->builder,
                    slot_ty, alloca_val, 2, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, token_id, slot_token_ptr);

                LLVMValueRef token_id_ptr = LLVMBuildStructGEP2(ctx->builder,
                    token_ty, token_alloca, 0, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, token_id, token_id_ptr);

                LLVMValueRef token_write_ptr = LLVMBuildStructGEP2(ctx->builder,
                    token_ty, token_alloca, 1, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
                    token_write_ptr);

                LLVMValueRef token_read_ptr = LLVMBuildStructGEP2(ctx->builder,
                    token_ty, token_alloca, 2, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
                    token_read_ptr);

                llvm_scope_declare(ctx, pergyra_strdup(token_name), token_alloca, token_ty);
            }

            llvm_scope_declare(ctx, name, alloca_val, slot_ty);
            llvm_register_slot_var(ctx, name, inner, is_secure);
            return;
        }
        if (strcmp(callee, "ClaimDeviceSlot") == 0) {
            const char *inner = NULL;
            if (type_ann != NULL && type_ann->type == AST_TYPE
                && type_ann->data.type.generic_args != NULL
                && type_ann->data.type.generic_args->count > 0) {
                inner = type_ann->data.type.generic_args->params[0]->name;
            }
            if (inner == NULL) {
                llvm_set_error_at_with_hints(ctx, node, PGY_CODE_LLVM_TYPE_UNSUPPORTED, PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING, PGY_FIX_ANNOTATE_CONCRETE_TYPE, "LLVM ClaimDeviceSlot let-binding for '%s' requires an explicit DeviceSlot<T> annotation",
                    name != NULL ? name : "<slot>");
                return;
            }

            LLVMTypeRef slot_ty = llvm_slot_struct_type(ctx, inner);
            LLVMValueRef alloca_val = llvm_stmt_create_slot_alloca(ctx, slot_ty, name);

            LLVMBuildStore(ctx->builder, LLVMConstNull(slot_ty), alloca_val);
            LLVMValueRef claimed_ptr = LLVMBuildStructGEP2(ctx->builder,
                slot_ty, alloca_val, 1, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
                claimed_ptr);

            llvm_scope_declare(ctx, name, alloca_val, slot_ty);
            llvm_register_device_slot_var(ctx, name, inner);
            return;
        }
    }

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
            ((strcmp(ann_name, "ReadView") == 0 || strncmp(ann_name, "ReadView<", 9) == 0)
             && strcmp(callee, "ViewRead") == 0)
            || ((strcmp(ann_name, "WriteView") == 0 || strncmp(ann_name, "WriteView<", 10) == 0)
                && strcmp(callee, "ViewWrite") == 0)
            || ((strcmp(ann_name, "MoveToken") == 0 || strncmp(ann_name, "MoveToken<", 10) == 0)
                && strcmp(callee, "Move") == 0);
        if (alias_decl) {
            const char *inner = "Int";
            if (type_ann->data.type.generic_args != NULL
                && type_ann->data.type.generic_args->count > 0)
                inner = llvm_stmt_render_type_arg(type_ann->data.type.generic_args->params[0]);

            LLVMVarEntry *source = llvm_scope_lookup(ctx, source_name);
            if (source == NULL)
                return;

            bool is_move = (strcmp(callee, "Move") == 0);

            if (is_move) {
                /* Move: structural copy ??new alloca owns the data,
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
                /* ReadView / WriteView: non-owning alias ??                 * share the source slot's alloca directly.
                 * No separate storage; reads/writes go through
                 * the same address as the owning slot. */
                llvm_scope_declare(ctx, name, source->alloca, source->type);
            }
            llvm_register_view_var(ctx, name, source_name, inner, is_move);
            return;
        }
    }

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
            return;
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
        const char *inner = "Int";

        if (type_ann->data.type.generic_args != NULL
            && type_ann->data.type.generic_args->count > 0
            && type_ann->data.type.generic_args->params[0] != NULL) {
            inner = llvm_stmt_render_type_arg(type_ann->data.type.generic_args->params[0]);
        }

        if (strcmp(ann_name, "List") == 0 && strcmp(callee, "ListNew") == 0) {
            LLVMTypeRef list_ty = ast_type_to_llvm(ctx, type_ann);
            LLVMTypeRef elem_ty = pergyra_type_to_llvm(ctx, inner);
            LLVMValueRef alloca_val = llvm_create_entry_alloca(ctx, list_ty, name);
            LLVMFuncEntry *new_fn = llvm_lookup_function(ctx, "pgy_list_new_raw_export");
            if (new_fn != NULL) {
                LLVMValueRef args[] = {
                    LLVMBuildBitCast(ctx->builder, alloca_val, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                    llvm_sizeof_type_i64(ctx, elem_ty)
                };
                LLVMBuildCall2(ctx->builder, new_fn->fn_type, new_fn->fn, args, 2, "");
            }
            llvm_scope_declare(ctx, name, alloca_val, list_ty);
            llvm_register_list_var(ctx, name, inner);
            return;
        }

        if (strcmp(ann_name, "Set") == 0 && strcmp(callee, "SetNew") == 0) {
            LLVMTypeRef set_ty = ast_type_to_llvm(ctx, type_ann);
            LLVMTypeRef elem_ty = pergyra_type_to_llvm(ctx, inner);
            LLVMValueRef alloca_val = llvm_create_entry_alloca(ctx, set_ty, name);
            LLVMFuncEntry *new_fn = llvm_lookup_function(ctx, "pgy_set_new_raw_export");
            if (new_fn != NULL) {
                LLVMValueRef args[] = {
                    LLVMBuildBitCast(ctx->builder, alloca_val, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                    llvm_sizeof_type_i64(ctx, elem_ty)
                };
                LLVMBuildCall2(ctx->builder, new_fn->fn_type, new_fn->fn, args, 2, "");
            }
            llvm_scope_declare(ctx, name, alloca_val, set_ty);
            llvm_register_set_var(ctx, name, inner);
            return;
        }

        if (strcmp(ann_name, "Queue") == 0 && strcmp(callee, "QueueNew") == 0) {
            LLVMTypeRef queue_ty = ast_type_to_llvm(ctx, type_ann);
            LLVMTypeRef elem_ty = pergyra_type_to_llvm(ctx, inner);
            LLVMValueRef alloca_val = llvm_create_entry_alloca(ctx, queue_ty, name);
            LLVMFuncEntry *new_fn = llvm_lookup_function(ctx, "pgy_queue_new_raw_export");
            if (new_fn != NULL) {
                LLVMValueRef args[] = {
                    LLVMBuildBitCast(ctx->builder, alloca_val, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                    llvm_sizeof_type_i64(ctx, elem_ty)
                };
                LLVMBuildCall2(ctx->builder, new_fn->fn_type, new_fn->fn, args, 2, "");
            }
            llvm_scope_declare(ctx, name, alloca_val, queue_ty);
            llvm_register_queue_var(ctx, name, inner);
            return;
        }

        if (strcmp(ann_name, "HashMap") == 0 && strcmp(callee, "MapNew") == 0) {
            const char *value_type = "Int";
            const char *key_type = "String";
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
            value_ty = pergyra_type_to_llvm(ctx, value_type);
            alloca_val = llvm_create_entry_alloca(ctx, map_ty, name);
            new_fn = llvm_lookup_function(ctx, "pgy_map_new_raw_export");
            if (new_fn != NULL) {
                LLVMValueRef args[] = {
                    LLVMBuildBitCast(ctx->builder, alloca_val, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                    llvm_sizeof_type_i64(ctx, value_ty)
                };
                LLVMBuildCall2(ctx->builder, new_fn->fn_type, new_fn->fn, args, 2, "");
            }
            llvm_scope_declare(ctx, name, alloca_val, map_ty);
            llvm_register_map_var(ctx, name, key_type, value_type);
            return;
        }
    }

    /* Slot sugar: let x: Slot<Int> = 42 ??auto Claim + Write */
    if (type_ann != NULL && type_ann->type == AST_TYPE
        && type_ann->data.type.name != NULL) {
        const char *ann_name = type_ann->data.type.name;
        bool is_slot_sugar = (strcmp(ann_name, "Slot") == 0
                           || strncmp(ann_name, "Slot<", 5) == 0);
        bool is_secure_slot_sugar = (strcmp(ann_name, "SecureSlot") == 0
                                  || strncmp(ann_name, "SecureSlot<", 11) == 0);
        if (is_slot_sugar || is_secure_slot_sugar) {
            const char *inner = "Int";
            bool is_secure = is_secure_slot_sugar;
            if (type_ann->data.type.generic_args != NULL
                && type_ann->data.type.generic_args->count > 0)
                inner = type_ann->data.type.generic_args->params[0]->name;

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

    /* Detect Channel constructor: let ch: Channel<Int> = Channel(capacity) */
    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER
        && strcmp(init->data.call.callee->data.identifier.name, "Channel") == 0) {
        /* Allocate opaque channel as a large-enough byte array.
         * PgyChannel_Int_RT on the runtime side is ~128 bytes;
         * we allocate 256 bytes for safety. */
        LLVMTypeRef ch_type = LLVMArrayType(
            LLVMInt8TypeInContext(ctx->context), 256);
        LLVMValueRef alloca_val = llvm_create_entry_alloca(ctx, ch_type, name);

        /* Call pgy_channel_init_Int(ptr, capacity) */
        LLVMFuncEntry *init_fn = llvm_lookup_function(ctx,
            "pgy_channel_init_Int");
        if (init_fn != NULL) {
            LLVMValueRef cap = LLVMConstInt(ctx->type_i64, 16, 0);
            if (init->data.call.arg_count > 0)
                cap = LLVMBuildZExt(ctx->builder,
                    llvm_emit_expression(init->data.call.arguments[0], ctx),
                    ctx->type_i64, llvm_tmp_name(ctx));
            LLVMValueRef args[] = { alloca_val, cap };
            LLVMBuildCall2(ctx->builder, init_fn->fn_type,
                           init_fn->fn, args, 2, "");
        }
        llvm_scope_declare(ctx, name, alloca_val, ch_type);
        if (type_ann != NULL && type_ann->type == AST_TYPE
            && type_ann->data.type.generic_args != NULL
            && type_ann->data.type.generic_args->count > 0) {
            llvm_register_channel_var(ctx, name,
                type_ann->data.type.generic_args->params[0]->name);
        } else {
            llvm_register_channel_var(ctx, name, "Int");
        }
        return;
    }

    /* Array literal: let values: Array<Int> = [1, 2, 3] */
    if (init != NULL && init->type == AST_ARRAY_LITERAL) {
        size_t count = init->data.array_literal.count;
        LLVMTypeRef elem_type = ctx->type_i32;
        const char *inner_name = "Int";

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

        LLVMTypeRef array_type = llvm_array_struct_type(ctx, inner_name);
        LLVMValueRef var_alloca = llvm_create_entry_alloca(ctx, array_type, name);
        char new_fn_name[64];
        char push_fn_name[64];
        LLVMFuncEntry *new_fn;
        LLVMFuncEntry *push_fn;

        snprintf(new_fn_name, sizeof(new_fn_name), "pgy_array_new_%s", inner_name);
        new_fn = llvm_lookup_function(ctx, new_fn_name);
        if (new_fn != NULL) {
            LLVMValueRef args[] = {
                LLVMConstInt(ctx->type_i64, (unsigned long long)count, 0)
            };
            LLVMValueRef arr_val = LLVMBuildCall2(ctx->builder, new_fn->fn_type,
                new_fn->fn, args, 1, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, arr_val, var_alloca);
        }

        snprintf(push_fn_name, sizeof(push_fn_name), "pgy_array_push_%s", inner_name);
        push_fn = llvm_lookup_function(ctx, push_fn_name);

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
            if (push_fn != NULL && element != NULL) {
                LLVMValueRef args[] = { var_alloca, element };
                LLVMBuildCall2(ctx->builder, push_fn->fn_type,
                    push_fn->fn, args, 2, "");
            }
        }

        llvm_scope_declare(ctx, name, var_alloca, array_type);
        llvm_register_array_var(ctx, name, elem_type, (int64_t)count);
        return;
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

    if (type_ann != NULL && type_ann->type == AST_EVENT_HANDLER_TYPE) {
        llvm_register_callable_var(ctx, name, type_ann);
    } else if (init != NULL && init->type == AST_LAMBDA_EXPR) {
        ASTNode **param_types = NULL;
        if (init->data.lambda_expr.param_count > 0) {
            param_types = pgy_arena_calloc(&ctx->scratch,
                init->data.lambda_expr.param_count * sizeof(ASTNode *));
            if (param_types == NULL) {
                llvm_set_error(ctx, "out of memory registering lambda callable");
                return;
            }
            for (size_t i = 0; i < init->data.lambda_expr.param_count; i++) {
                ASTNode *p = init->data.lambda_expr.params[i];
                param_types[i] = (p != NULL && p->type == AST_LET_DECL)
                    ? p->data.let_decl.type : NULL;
            }
        }
        llvm_register_callable_signature(ctx, name,
            init->data.lambda_expr.param_count,
            param_types,
            init->data.lambda_expr.return_type);
    } else if (init != NULL && init->type == AST_IDENTIFIER
               && init->data.identifier.name != NULL) {
        ASTNode *decl = llvm_stmt_find_function_decl_by_name(ctx,
            init->data.identifier.name);
        if (decl != NULL && decl->type == AST_FUNC_DECL) {
            ASTNode **param_types = NULL;
            if (decl->data.func_decl.param_count > 0) {
                param_types = pgy_arena_calloc(&ctx->scratch,
                    decl->data.func_decl.param_count * sizeof(ASTNode *));
                if (param_types == NULL) {
                    llvm_set_error(ctx,
                        "out of memory registering function callable");
                    return;
                }
                for (size_t i = 0; i < decl->data.func_decl.param_count; i++) {
                    FuncParam *p = decl->data.func_decl.params[i];
                    param_types[i] = p != NULL ? p->type : NULL;
                }
            }
            llvm_register_callable_signature(ctx, name,
                decl->data.func_decl.param_count,
                param_types,
                decl->data.func_decl.return_type);
        }
    } else if (init != NULL && init->type == AST_CALL
               && init->data.call.callee != NULL
               && init->data.call.callee->type == AST_IDENTIFIER
               && init->data.call.callee->data.identifier.name != NULL) {
        ASTNode *decl = llvm_stmt_find_function_decl_by_name(ctx,
            init->data.call.callee->data.identifier.name);
        if (decl != NULL && decl->type == AST_FUNC_DECL
            && decl->data.func_decl.return_type != NULL
            && decl->data.func_decl.return_type->type == AST_EVENT_HANDLER_TYPE) {
            llvm_register_callable_var(ctx, name, decl->data.func_decl.return_type);
        }
    }

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
            llvm_register_list_var(ctx, name, inner_name);
        } else if (strcmp(ann_name, "Queue") == 0
            && type_ann->data.type.generic_args != NULL
            && type_ann->data.type.generic_args->count > 0
            && type_ann->data.type.generic_args->params[0] != NULL) {
            char *inner_name = llvm_stmt_render_type_arg(
                type_ann->data.type.generic_args->params[0]);
            llvm_register_queue_var(ctx, name, inner_name);
        } else if (strcmp(ann_name, "HashMap") == 0
            && type_ann->data.type.generic_args != NULL
            && type_ann->data.type.generic_args->count > 1
            && type_ann->data.type.generic_args->params[0] != NULL
            && type_ann->data.type.generic_args->params[1] != NULL) {
            char *key_name = llvm_stmt_render_type_arg(
                type_ann->data.type.generic_args->params[0]);
            char *value_name = llvm_stmt_render_type_arg(
                type_ann->data.type.generic_args->params[1]);
            llvm_register_map_var(ctx, name, key_name, value_name);
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
            llvm_register_future_var(ctx, name,
                type_ann->data.type.generic_args->params[0]->name,
                strcmp(ann_name, "RemoteFuture") == 0);
        }
    } else if (init != NULL && init->type == AST_CALL
               && init->data.call.callee != NULL
               && init->data.call.callee->type == AST_IDENTIFIER
               && strcmp(init->data.call.callee->data.identifier.name,
                         "SubmitDeviceRead") == 0) {
        const char *inner = "Int";
        ASTNode *slot_arg = init->data.call.arguments[0];
        if (slot_arg != NULL && slot_arg->type == AST_IDENTIFIER) {
            const char *tracked = llvm_lookup_device_slot_inner(
                ctx, slot_arg->data.identifier.name);
            if (tracked != NULL)
                inner = tracked;
        }
        llvm_register_future_var(ctx, name, inner, true);
    } else if (init != NULL && init->type == AST_SPAWN_EXPR) {
        llvm_register_future_var(ctx, name,
            spawn_future_inner != NULL ? spawn_future_inner : "Int",
            false);
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
