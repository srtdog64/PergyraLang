#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"

/* =================================================================
 * Statement emission
 * ================================================================= */

static const char *
llvm_simple_expr_type_name(LLVMGenCtx *ctx, ASTNode *expr)
{
    if (expr == NULL)
        return "Int";

    switch (expr->type) {
    case AST_NUMBER: return "Int";
    case AST_STRING: return "String";
    case AST_BOOLEAN: return "Bool";
    case AST_IDENTIFIER: {
        LLVMVarEntry *entry = llvm_scope_lookup(ctx, expr->data.identifier.name);
        if (entry != NULL)
            return llvm_type_to_suffix(ctx, entry->type);
        return "Int";
    }
    default:
        return "Int";
    }
}

static bool
llvm_is_option_destructor(ASTNode *pat, const char **kind, const char **binding)
{
    *kind = NULL;
    *binding = NULL;

    if (pat == NULL)
        return false;

    if (pat->type == AST_IDENTIFIER) {
        const char *name = pat->data.identifier.name;
        if (name != NULL && strcmp(name, "None") == 0) {
            *kind = "None";
            return true;
        }
        return false;
    }

    if (pat->type != AST_CALL
        || pat->data.call.callee == NULL
        || pat->data.call.callee->type != AST_IDENTIFIER) {
        return false;
    }

    const char *name = pat->data.call.callee->data.identifier.name;
    if (name == NULL)
        return false;

    if (strcmp(name, "None") == 0 && pat->data.call.arg_count == 0) {
        *kind = "None";
        return true;
    }
    if (strcmp(name, "Some") == 0 && pat->data.call.arg_count == 1) {
        *kind = "Some";
        if (pat->data.call.arguments[0] != NULL
            && pat->data.call.arguments[0]->type == AST_IDENTIFIER) {
            *binding = pat->data.call.arguments[0]->data.identifier.name;
        }
        return true;
    }

    return false;
}

static void
llvm_defer_scope_push(LLVMGenCtx *ctx)
{
    if (ctx->defer_scope_depth >= MAX_SCOPE_DEPTH)
        return;
    ctx->defer_body_counts[ctx->defer_scope_depth++] = 0;
}

static void
llvm_defer_scope_pop(LLVMGenCtx *ctx)
{
    if (ctx->defer_scope_depth <= 0)
        return;
    ctx->defer_scope_depth--;
    ctx->defer_body_counts[ctx->defer_scope_depth] = 0;
}

static void
llvm_register_defer(ASTNode *body, LLVMGenCtx *ctx)
{
    if (body == NULL || ctx->defer_scope_depth <= 0)
        return;
    int scope = ctx->defer_scope_depth - 1;
    int count = ctx->defer_body_counts[scope];
    if (count >= MAX_DEFER_PER_SCOPE)
        return;
    ctx->defer_bodies[scope][count] = body;
    ctx->defer_body_counts[scope]++;
}

static void
llvm_emit_defers_from(LLVMGenCtx *ctx, int start_depth)
{
    if (start_depth < 0)
        start_depth = 0;
    for (int depth = ctx->defer_scope_depth - 1; depth >= start_depth; depth--) {
        for (int i = ctx->defer_body_counts[depth] - 1; i >= 0; i--) {
            ASTNode *body = ctx->defer_bodies[depth][i];
            if (body != NULL)
                llvm_emit_statement(body, ctx);
        }
    }
}

static const char *
llvm_infer_spawn_future_inner(LLVMGenCtx *ctx, ASTNode *spawn_expr)
{
    ASTNode *target = spawn_expr != NULL ? spawn_expr->data.spawn_expr.function : NULL;
    ASTNode *call = NULL;
    ASTNode *callee = target;
    const char *callee_name = NULL;
    static char buf[128];

    if (target != NULL && target->type == AST_CALL) {
        call = target;
        callee = target->data.call.callee;
    }
    if (callee != NULL && callee->type == AST_IDENTIFIER)
        callee_name = callee->data.identifier.name;
    if (callee_name == NULL || ctx->hir == NULL)
        return "Int";

    ASTNode *decl = NULL;
    for (size_t i = 0; i < ctx->hir->function_count; i++) {
        ASTNode *fn = ctx->hir->functions[i];
        if (fn != NULL && fn->type == AST_FUNC_DECL
            && fn->data.func_decl.name != NULL
            && strcmp(fn->data.func_decl.name, callee_name) == 0) {
            decl = fn;
            break;
        }
    }
    if (decl == NULL || decl->data.func_decl.return_type == NULL
        || decl->data.func_decl.return_type->type != AST_TYPE
        || decl->data.func_decl.return_type->data.type.name == NULL) {
        return "Int";
    }

    const char *ret_name = decl->data.func_decl.return_type->data.type.name;
    if (!(ret_name[0] >= 'A' && ret_name[0] <= 'Z' && ret_name[1] == '\0'))
        return ret_name;

    if (call == NULL)
        return "Int";

    for (size_t i = 0; i < decl->data.func_decl.param_count && i < call->data.call.arg_count; i++) {
        FuncParam *param = decl->data.func_decl.params[i];
        if (param == NULL || param->type == NULL || param->type->type != AST_TYPE
            || param->type->data.type.name == NULL)
            continue;
        if (strcmp(param->type->data.type.name, ret_name) == 0) {
            snprintf(buf, sizeof(buf), "%s",
                llvm_simple_expr_type_name(ctx, call->data.call.arguments[i]));
            return buf;
        }
    }

    return "Int";
}

static void
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
            /* Resolve inner type from type annotation */
            const char *inner = "Int";
            bool is_secure = (strcmp(callee, "ClaimSecureSlot") == 0);
            if (type_ann != NULL && type_ann->type == AST_TYPE) {
                /* Check for generic args: Slot<Int> */
                if (type_ann->data.type.generic_args != NULL
                    && type_ann->data.type.generic_args->count > 0)
                    inner = type_ann->data.type.generic_args->params[0]->name;
                else if (type_ann->data.type.name != NULL) {
                    /* Try to extract inner from type name like "Slot_Int" */
                    const char *tn = type_ann->data.type.name;
                    if (strncmp(tn, "Slot", 4) == 0)
                        inner = "Int"; /* default */
                }
            }

            LLVMTypeRef slot_ty = is_secure
                ? llvm_secure_slot_struct_type(ctx, inner)
                : llvm_slot_struct_type(ctx, inner);
            LLVMValueRef alloca_val = llvm_create_entry_alloca(ctx, slot_ty, name);

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
                LLVMValueRef token_alloca = llvm_create_entry_alloca(ctx, token_ty, token_name);
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
            const char *inner = "Int";
            if (type_ann != NULL && type_ann->type == AST_TYPE
                && type_ann->data.type.generic_args != NULL
                && type_ann->data.type.generic_args->count > 0) {
                inner = type_ann->data.type.generic_args->params[0]->name;
            }

            LLVMTypeRef slot_ty = llvm_slot_struct_type(ctx, inner);
            LLVMValueRef alloca_val = llvm_create_entry_alloca(ctx, slot_ty, name);

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
                inner = type_ann->data.type.generic_args->params[0]->name;

            LLVMVarEntry *source = llvm_scope_lookup(ctx, source_name);
            if (source == NULL)
                return;

            bool is_move = (strcmp(callee, "Move") == 0);

            if (is_move) {
                /* Move: structural copy — new alloca owns the data,
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
                /* ReadView / WriteView: non-owning alias —
                 * share the source slot's alloca directly.
                 * No separate storage; reads/writes go through
                 * the same address as the owning slot. */
                llvm_scope_declare(ctx, name, source->alloca, source->type);
            }
            llvm_register_view_var(ctx, name, source_name, inner, is_move);
            return;
        }
    }

    /* Slot sugar: let x: Slot<Int> = 42 → auto Claim + Write */
    if (type_ann != NULL && type_ann->type == AST_TYPE
        && type_ann->data.type.name != NULL) {
        const char *ann_name = type_ann->data.type.name;
        bool is_slot_sugar = (strcmp(ann_name, "Slot") == 0
                           || strncmp(ann_name, "Slot<", 5) == 0);
        if (is_slot_sugar) {
            const char *inner = "Int";
            if (type_ann->data.type.generic_args != NULL
                && type_ann->data.type.generic_args->count > 0)
                inner = type_ann->data.type.generic_args->params[0]->name;

            if (init != NULL && init->type == AST_IDENTIFIER) {
                LLVMViewVarEntry *move_entry = llvm_lookup_view_var(ctx,
                    init->data.identifier.name);
                if (move_entry != NULL && move_entry->is_move_token) {
                    LLVMTypeRef slot_ty = llvm_slot_struct_type(ctx, inner);
                    LLVMValueRef alloca_val = llvm_create_entry_alloca(ctx, slot_ty, name);
                    LLVMVarEntry *source = llvm_scope_lookup(ctx, init->data.identifier.name);
                    if (source == NULL)
                        return;
                    LLVMValueRef moved = LLVMBuildLoad2(ctx->builder, source->type, source->alloca,
                        llvm_tmp_name(ctx));
                    LLVMBuildStore(ctx->builder, moved, alloca_val);
                    llvm_scope_declare(ctx, name, alloca_val, slot_ty);
                    llvm_register_slot_var(ctx, name, inner,
                        llvm_lookup_slot_is_secure(ctx, move_entry->source_slot));
                    return;
                }
            }

            LLVMTypeRef slot_ty = llvm_slot_struct_type(ctx, inner);
            LLVMValueRef alloca_val = llvm_create_entry_alloca(ctx, slot_ty, name);

            /* Inline Claim: zero-init + set claimed=true */
            LLVMBuildStore(ctx->builder, LLVMConstNull(slot_ty), alloca_val);
            LLVMValueRef claimed_ptr = LLVMBuildStructGEP2(ctx->builder,
                slot_ty, alloca_val, 1, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
                claimed_ptr);

            llvm_scope_declare(ctx, name, alloca_val, slot_ty);
            llvm_register_slot_var(ctx, name, inner, false);

            /* Auto Write the initializer value */
            if (init != NULL) {
                LLVMValueRef val = llvm_emit_expression(init, ctx);
                if (val != NULL) {
                    char fn_name[64];
                    snprintf(fn_name, sizeof(fn_name), "pgy_write_%s", inner);
                    LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
                    if (fn != NULL) {
                        LLVMValueRef args[] = { alloca_val, val };
                        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
                    } else {
                        LLVMValueRef value_ptr = LLVMBuildStructGEP2(ctx->builder,
                            slot_ty, alloca_val, 0, llvm_tmp_name(ctx));
                        LLVMValueRef occ_ptr = LLVMBuildStructGEP2(ctx->builder,
                            slot_ty, alloca_val, 1, llvm_tmp_name(ctx));
                        LLVMBuildStore(ctx->builder, val, value_ptr);
                        LLVMBuildStore(ctx->builder,
                            LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
                            occ_ptr);
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
            /* Allocate struct on stack */
            LLVMValueRef alloca_val = llvm_create_entry_alloca(
                ctx, cls->struct_type, name);

            /* Store each argument into corresponding field */
            size_t argc = init->data.call.arg_count;
            for (size_t i = 0; i < argc && (int)i < cls->field_count; i++) {
                LLVMValueRef arg = llvm_emit_expression(
                    init->data.call.arguments[i], ctx);
                LLVMValueRef gep = LLVMBuildStructGEP2(
                    ctx->builder, cls->struct_type, alloca_val,
                    (unsigned)cls->fields[i].index, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, arg, gep);
            }

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
    }

    /* Create alloca at function entry */
    LLVMValueRef alloca = llvm_create_entry_alloca(ctx, var_type, name);

    /* Store initializer if present */
    if (init != NULL) {
        LLVMValueRef val = llvm_emit_expression(init, ctx);
        if (val != NULL) {
            LLVMTypeRef val_type = LLVMTypeOf(val);

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
        }
    }

    llvm_scope_declare(ctx, name, alloca, var_type);

    if (type_ann != NULL && type_ann->type == AST_TYPE
        && type_ann->data.type.name != NULL
        && (strcmp(type_ann->data.type.name, "Array") == 0
            || strcmp(type_ann->data.type.name, "Slice") == 0)
        && type_ann->data.type.generic_args != NULL
        && type_ann->data.type.generic_args->count > 0) {
        LLVMTypeRef elem_type = pergyra_type_to_llvm(
            ctx, type_ann->data.type.generic_args->params[0]->name);
        llvm_register_array_var(ctx, name, elem_type, -1);
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
    }
}

static void
llvm_emit_return_stmt(ASTNode *node, LLVMGenCtx *ctx)
{
    llvm_emit_defers_from(ctx, 0);

    if (node->data.return_stmt.value != NULL) {
        LLVMValueRef val = llvm_emit_expression(node->data.return_stmt.value,
                                                 ctx);
        if (val != NULL) {
            /* Coerce to expected return type */
            LLVMTypeRef val_type = LLVMTypeOf(val);
            LLVMTypeRef ret_type = ctx->current_ret_type;
            if (ret_type != val_type && ret_type != ctx->type_void) {
                bool ret_is_int = (ret_type == ctx->type_i32 || ret_type == ctx->type_i64);
                bool ret_is_fp  = (ret_type == ctx->type_f32 || ret_type == ctx->type_f64);
                bool val_is_int = (val_type == ctx->type_i32 || val_type == ctx->type_i64);
                bool val_is_fp  = (val_type == ctx->type_f32 || val_type == ctx->type_f64);

                if (ret_is_int && val_is_fp)
                    val = LLVMBuildFPToSI(ctx->builder, val, ret_type,
                                           llvm_tmp_name(ctx));
                else if (ret_is_fp && val_is_int)
                    val = LLVMBuildSIToFP(ctx->builder, val, ret_type,
                                           llvm_tmp_name(ctx));
                else if (ret_is_int && val_is_int)
                    val = (LLVMGetIntTypeWidth(ret_type) > LLVMGetIntTypeWidth(val_type))
                        ? LLVMBuildSExt(ctx->builder, val, ret_type, llvm_tmp_name(ctx))
                        : LLVMBuildTrunc(ctx->builder, val, ret_type, llvm_tmp_name(ctx));
                else if (ret_is_fp && val_is_fp)
                    val = (ret_type == ctx->type_f64)
                        ? LLVMBuildFPExt(ctx->builder, val, ret_type, llvm_tmp_name(ctx))
                        : LLVMBuildFPTrunc(ctx->builder, val, ret_type, llvm_tmp_name(ctx));
            }
            LLVMBuildRet(ctx->builder, val);
        } else {
            LLVMBuildRet(ctx->builder, LLVMConstInt(ctx->type_i32, 0, 0));
        }
    } else {
        if (ctx->current_ret_type == ctx->type_void)
            LLVMBuildRetVoid(ctx->builder);
        else
            LLVMBuildRet(ctx->builder,
                          LLVMConstInt(ctx->current_ret_type, 0, 0));
    }
}

static void
llvm_emit_if_stmt(ASTNode *node, LLVMGenCtx *ctx)
{
    LLVMValueRef cond = llvm_emit_expression(node->data.if_stmt.condition, ctx);
    if (cond == NULL)
        return;

    /* Ensure cond is i1 */
    if (LLVMTypeOf(cond) != ctx->type_i1)
        cond = LLVMBuildICmp(ctx->builder, LLVMIntNE, cond,
                              LLVMConstInt(LLVMTypeOf(cond), 0, 0),
                              llvm_tmp_name(ctx));

    LLVMValueRef fn = ctx->current_function;
    LLVMBasicBlockRef then_bb  = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "then");
    LLVMBasicBlockRef else_bb  = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "else");
    LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "ifcont");

    LLVMBuildCondBr(ctx->builder, cond, then_bb, else_bb);

    /* Then block */
    LLVMPositionBuilderAtEnd(ctx->builder, then_bb);
    if (node->data.if_stmt.then_branch != NULL)
        llvm_emit_statement(node->data.if_stmt.then_branch, ctx);
    /* Only branch to merge if no terminator (return) was emitted */
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, merge_bb);

    /* Else block */
    LLVMPositionBuilderAtEnd(ctx->builder, else_bb);
    if (node->data.if_stmt.else_branch != NULL)
        llvm_emit_statement(node->data.if_stmt.else_branch, ctx);
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, merge_bb);

    /* Merge */
    LLVMPositionBuilderAtEnd(ctx->builder, merge_bb);
}

static void
llvm_emit_while_loop(ASTNode *node, LLVMGenCtx *ctx)
{
    LLVMValueRef fn = ctx->current_function;
    LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "while.cond");
    LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "while.body");
    LLVMBasicBlockRef exit_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "while.exit");

    LLVMBuildBr(ctx->builder, cond_bb);

    /* Condition */
    LLVMPositionBuilderAtEnd(ctx->builder, cond_bb);
    LLVMValueRef cond = llvm_emit_expression(node->data.while_loop.condition,
                                              ctx);
    if (cond != NULL && LLVMTypeOf(cond) != ctx->type_i1)
        cond = LLVMBuildICmp(ctx->builder, LLVMIntNE, cond,
                              LLVMConstInt(LLVMTypeOf(cond), 0, 0),
                              llvm_tmp_name(ctx));
    if (cond == NULL)
        cond = LLVMConstInt(ctx->type_i1, 0, 0);

    LLVMBuildCondBr(ctx->builder, cond, body_bb, exit_bb);

    /* Body */
    LLVMPositionBuilderAtEnd(ctx->builder, body_bb);
    if (ctx->loop_depth < MAX_SCOPE_DEPTH) {
        ctx->loop_continue_blocks[ctx->loop_depth] = cond_bb;
        ctx->loop_break_blocks[ctx->loop_depth] = exit_bb;
        ctx->loop_defer_base_depth[ctx->loop_depth] = ctx->defer_scope_depth;
        ctx->loop_depth++;
    }
    if (node->data.while_loop.body != NULL)
        llvm_emit_statement(node->data.while_loop.body, ctx);
    if (ctx->loop_depth > 0)
        ctx->loop_depth--;
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, cond_bb);

    /* Exit */
    LLVMPositionBuilderAtEnd(ctx->builder, exit_bb);
}

static void
llvm_emit_for_loop(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *var_name = node->data.for_loop.variable;

    llvm_scope_push(ctx);

    /* Create loop variable */
    LLVMValueRef var_alloca = llvm_create_entry_alloca(ctx, ctx->type_i32,
                                                        var_name);
    LLVMValueRef start = llvm_emit_expression(node->data.for_loop.range_start,
                                               ctx);
    if (start == NULL)
        start = LLVMConstInt(ctx->type_i32, 0, 0);
    LLVMBuildStore(ctx->builder, start, var_alloca);
    llvm_scope_declare(ctx, var_name, var_alloca, ctx->type_i32);

    LLVMValueRef fn = ctx->current_function;
    LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "for.cond");
    LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "for.body");
    LLVMBasicBlockRef incr_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "for.incr");
    LLVMBasicBlockRef exit_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "for.exit");

    LLVMBuildBr(ctx->builder, cond_bb);

    /* Condition: i < end */
    LLVMPositionBuilderAtEnd(ctx->builder, cond_bb);
    LLVMValueRef current = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                                           var_alloca, llvm_tmp_name(ctx));
    LLVMValueRef end = llvm_emit_expression(node->data.for_loop.range_end, ctx);
    if (end == NULL)
        end = LLVMConstInt(ctx->type_i32, 0, 0);
    LLVMValueRef cond = LLVMBuildICmp(ctx->builder, LLVMIntSLT, current, end,
                                       llvm_tmp_name(ctx));
    LLVMBuildCondBr(ctx->builder, cond, body_bb, exit_bb);

    /* Body */
    LLVMPositionBuilderAtEnd(ctx->builder, body_bb);
    if (ctx->loop_depth < MAX_SCOPE_DEPTH) {
        ctx->loop_continue_blocks[ctx->loop_depth] = incr_bb;
        ctx->loop_break_blocks[ctx->loop_depth] = exit_bb;
        ctx->loop_defer_base_depth[ctx->loop_depth] = ctx->defer_scope_depth;
        ctx->loop_depth++;
    }
    if (node->data.for_loop.body != NULL)
        llvm_emit_statement(node->data.for_loop.body, ctx);
    if (ctx->loop_depth > 0)
        ctx->loop_depth--;
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, incr_bb);

    /* Increment: i = i + 1 */
    LLVMPositionBuilderAtEnd(ctx->builder, incr_bb);
    LLVMValueRef cur2 = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                                        var_alloca, llvm_tmp_name(ctx));
    LLVMValueRef next = LLVMBuildAdd(ctx->builder, cur2,
                                      LLVMConstInt(ctx->type_i32, 1, 0),
                                      llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, next, var_alloca);
    LLVMBuildBr(ctx->builder, cond_bb);

    /* Exit */
    LLVMPositionBuilderAtEnd(ctx->builder, exit_bb);

    llvm_scope_pop(ctx);
}

static void
llvm_emit_match_stmt(ASTNode *node, LLVMGenCtx *ctx)
{
    LLVMValueRef subject = llvm_emit_expression(node->data.match_stmt.subject,
                                                 ctx);
    if (subject == NULL)
        return;

    LLVMValueRef fn = ctx->current_function;
    LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "match.end");

    for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
        ASTNode *mc = node->data.match_stmt.cases[i];
        if (mc == NULL || mc->type != AST_MATCH_CASE)
            continue;

        const char *option_kind = NULL;
        const char *option_binding = NULL;
        LLVMValueRef cmp = NULL;

        if (llvm_is_option_destructor(mc->data.match_case.pattern,
                                      &option_kind, &option_binding)) {
            LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, subject, 0,
                llvm_tmp_name(ctx));
            cmp = LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
                LLVMConstInt(ctx->type_i32,
                    strcmp(option_kind, "Some") == 0 ? 0 : 1, 0),
                llvm_tmp_name(ctx));
        } else {
            LLVMValueRef pattern = llvm_emit_expression(mc->data.match_case.pattern,
                                                         ctx);
            if (pattern == NULL)
                continue;
            cmp = LLVMBuildICmp(ctx->builder, LLVMIntEQ,
                                subject, pattern,
                                llvm_tmp_name(ctx));
        }

        LLVMBasicBlockRef case_bb = LLVMAppendBasicBlockInContext(
            ctx->context, fn, "match.case");
        LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(
            ctx->context, fn, "match.next");

        LLVMBuildCondBr(ctx->builder, cmp, case_bb, next_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, case_bb);
        llvm_scope_push(ctx);
        if (option_binding != NULL) {
            LLVMValueRef payload = LLVMBuildExtractValue(ctx->builder, subject, 1,
                llvm_tmp_name(ctx));
            LLVMTypeRef payload_ty = LLVMTypeOf(payload);
            LLVMValueRef payload_alloca = llvm_create_entry_alloca(ctx, payload_ty,
                option_binding);
            LLVMBuildStore(ctx->builder, payload, payload_alloca);
            llvm_scope_declare(ctx, pergyra_strdup(option_binding),
                payload_alloca, payload_ty);
        }
        if (mc->data.match_case.body != NULL)
            llvm_emit_statement(mc->data.match_case.body, ctx);
        llvm_scope_pop(ctx);
        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
            LLVMBuildBr(ctx->builder, merge_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
    }

    /* Default case */
    if (node->data.match_stmt.default_body != NULL) {
        llvm_emit_statement(node->data.match_stmt.default_body, ctx);
    }
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, merge_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, merge_bb);
}

static void
llvm_emit_with_stmt(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *alias = node->data.with_stmt.alias;
    bool is_secure    = node->data.with_stmt.is_secure;

    const char *inner = "Int";
    if (node->data.with_stmt.slot_type != NULL
        && node->data.with_stmt.slot_type->type == AST_TYPE
        && node->data.with_stmt.slot_type->data.type.name != NULL)
        inner = node->data.with_stmt.slot_type->data.type.name;

    LLVMTypeRef slot_ty = is_secure
        ? llvm_secure_slot_struct_type(ctx, inner)
        : llvm_slot_struct_type(ctx, inner);
    LLVMValueRef alloca_val = llvm_create_entry_alloca(ctx, slot_ty, alias);

    /* Inline claim: zero-init + set claimed=true (avoids ABI mismatch) */
    char fn_name[64];
    LLVMBuildStore(ctx->builder, LLVMConstNull(slot_ty), alloca_val);
    LLVMValueRef claimed_ptr = LLVMBuildStructGEP2(ctx->builder,
        slot_ty, alloca_val, 1, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder,
        LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
        claimed_ptr);

    /* Push scope, register slot variable */
    llvm_scope_push(ctx);
    if (is_secure) {
        LLVMTypeRef token_ty = llvm_secure_token_type(ctx, inner);
        char token_name[256];
        snprintf(token_name, sizeof(token_name), "%s_token", alias);
        LLVMValueRef token_alloca = llvm_create_entry_alloca(ctx, token_ty, token_name);
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
    llvm_scope_declare(ctx, alias, alloca_val, slot_ty);
    llvm_register_slot_var(ctx, alias, inner, is_secure);

    /* Emit body */
    if (node->data.with_stmt.body != NULL)
        llvm_emit_block(node->data.with_stmt.body, ctx);

    /* Auto-release */
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL) {
        snprintf(fn_name, sizeof(fn_name), is_secure ? "pgy_secure_release_%s" : "pgy_release_%s", inner);
        LLVMFuncEntry *release_fn = llvm_lookup_function(ctx, fn_name);
        if (release_fn != NULL) {
            if (is_secure) {
                LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, alias);
                if (token_var != NULL) {
                    LLVMValueRef args[] = { alloca_val, token_var->alloca };
                    LLVMBuildCall2(ctx->builder, release_fn->fn_type,
                                   release_fn->fn, args, 2, "");
                }
            } else {
                LLVMValueRef args[] = { alloca_val };
                LLVMBuildCall2(ctx->builder, release_fn->fn_type,
                               release_fn->fn, args, 1, "");
            }
        }
    }

    llvm_scope_pop(ctx);
}

void
llvm_emit_block(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node == NULL || ctx->has_error)
        return;

    if (node->type != AST_BLOCK)
        return;

    int saved_slot_count = ctx->slot_var_count;
    llvm_defer_scope_push(ctx);
    llvm_scope_push(ctx);
    for (size_t i = 0; i < node->data.block.count; i++) {
        llvm_emit_statement(node->data.block.statements[i], ctx);
        /* Stop emitting after a terminator (return) */
        if (LLVMGetBasicBlockTerminator(
                LLVMGetInsertBlock(ctx->builder)) != NULL)
            break;
    }

    /* Slot sugar: auto-release slot vars declared in this scope (LIFO).
     * Skip slots already explicitly released by the user. */
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL) {
        llvm_emit_defers_from(ctx, ctx->defer_scope_depth - 1);
        for (int i = ctx->slot_var_count - 1; i >= saved_slot_count; i--) {
            if (ctx->slot_vars[i].released) continue;
            const char *inner = ctx->slot_vars[i].inner_type;
            const char *vname = ctx->slot_vars[i].var_name;
            char fn_name[64];
            bool is_secure = ctx->slot_vars[i].is_secure;
            snprintf(fn_name, sizeof(fn_name),
                is_secure ? "pgy_secure_release_%s" : "pgy_release_%s", inner);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
            LLVMVarEntry *var = llvm_scope_lookup(ctx, vname);
            if (fn != NULL && var != NULL) {
                if (is_secure) {
                    LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, vname);
                    if (token_var != NULL) {
                        LLVMValueRef args[] = { var->alloca, token_var->alloca };
                        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
                    }
                } else {
                    LLVMValueRef args[] = { var->alloca };
                    LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
                }
            } else if (!is_secure && var != NULL) {
                LLVMValueRef occ_ptr = LLVMBuildStructGEP2(ctx->builder,
                    var->type, var->alloca, 1, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 0, 0),
                    occ_ptr);
            }
        }
    }

    ctx->slot_var_count = saved_slot_count;
    llvm_scope_pop(ctx);
    llvm_defer_scope_pop(ctx);
}

/* =================================================================
 * Parallel block — real concurrency via thread pool
 *
 * For each task, generate an LLVM function `_pgy_par_N(i8*) -> i8*`
 * that contains the task body, then spawn all + await all.
 * ================================================================= */

static void
llvm_emit_parallel_block(ASTNode *node, LLVMGenCtx *ctx)
{
    size_t count = node->data.parallel.task_count;
    if (count == 0)
        return;

    /* -----------------------------------------------------------
     * 1) Collect all variables from the current scope stack.
     *    These will be captured into a context struct so that
     *    wrapper functions can access them.
     * ----------------------------------------------------------- */
    typedef struct { const char *name; LLVMValueRef alloca; LLVMTypeRef type; } CapturedVar;
    CapturedVar captured[MAX_SCOPE_VARS];
    int n_captured = 0;

    for (int i = 0; i < ctx->scope_depth; i++) {
        LLVMScopeFrame *frame = &ctx->scopes[i];
        for (int j = 0; j < frame->count && n_captured < MAX_SCOPE_VARS; j++) {
            captured[n_captured++] = (CapturedVar){
                frame->entries[j].name,
                frame->entries[j].alloca,
                frame->entries[j].type
            };
        }
    }

    /* -----------------------------------------------------------
     * 2) Build a context struct type: { ptr, ptr, ... }
     *    Each field is a pointer to the captured variable's alloca.
     *    In the wrapper, we GEP to get the pointer, then load/store
     *    through it — exactly like the C transpiler's approach.
     * ----------------------------------------------------------- */
    LLVMTypeRef *ctx_fields = calloc((size_t)n_captured, sizeof(LLVMTypeRef));
    for (int i = 0; i < n_captured; i++)
        ctx_fields[i] = ctx->type_i8ptr;   /* all fields are opaque ptr */

    char ctx_name[64];
    snprintf(ctx_name, sizeof(ctx_name), "_pgy_par_ctx_%d", ctx->parallel_counter);
    LLVMTypeRef ctx_struct_type = LLVMStructCreateNamed(ctx->context, ctx_name);
    LLVMStructSetBody(ctx_struct_type, ctx_fields, (unsigned)n_captured, 0);
    free(ctx_fields);

    /* -----------------------------------------------------------
     * 3) In the OUTER function: allocate + fill the context struct.
     * ----------------------------------------------------------- */
    LLVMValueRef ctx_alloca = LLVMBuildAlloca(ctx->builder, ctx_struct_type,
                                               "_pctx");
    for (int i = 0; i < n_captured; i++) {
        LLVMValueRef gep = LLVMBuildStructGEP2(ctx->builder, ctx_struct_type,
                                                 ctx_alloca, (unsigned)i,
                                                 llvm_tmp_name(ctx));
        /* Store the alloca address (pointer to the variable) */
        LLVMBuildStore(ctx->builder, captured[i].alloca, gep);
    }

    /* Cast context struct pointer to i8* for spawn argument */
    LLVMValueRef ctx_i8ptr = LLVMBuildBitCast(ctx->builder, ctx_alloca,
                                               ctx->type_i8ptr,
                                               llvm_tmp_name(ctx));

    /* -----------------------------------------------------------
     * 4) Generate wrapper functions for each parallel task.
     *    Each wrapper receives the context struct as i8* arg,
     *    casts it back, and GEPs to access captured variable pointers.
     * ----------------------------------------------------------- */
    LLVMValueRef    saved_fn  = ctx->current_function;
    LLVMTypeRef     saved_ret = ctx->current_ret_type;
    LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(ctx->builder);

    LLVMTypeRef wrapper_params[] = { ctx->type_i8ptr };
    LLVMTypeRef wrapper_type = LLVMFunctionType(ctx->type_i8ptr,
                                                 wrapper_params, 1, 0);

    LLVMValueRef *wrapper_fns = calloc(count, sizeof(LLVMValueRef));

    for (size_t i = 0; i < count; i++) {
        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name), "_pgy_par_%d_%zu",
                 ctx->parallel_counter, i);

        LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, wrapper_type);
        wrapper_fns[i] = fn;

        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(
            ctx->context, fn, "entry");
        LLVMPositionBuilderAtEnd(ctx->builder, entry);

        ctx->current_function = fn;
        ctx->current_ret_type = ctx->type_i8ptr;

        llvm_scope_push(ctx);

        /* Cast arg (i8*) back to context struct pointer */
        LLVMValueRef arg0 = LLVMGetParam(fn, 0);
        LLVMValueRef ctx_ptr = LLVMBuildBitCast(ctx->builder, arg0,
            LLVMPointerType(ctx_struct_type, 0), "_pctx");

        /* For each captured variable: GEP → load pointer → declare in scope.
         * The loaded pointer points to the original alloca, so
         * load/store through it accesses the outer variable. */
        for (int c = 0; c < n_captured; c++) {
            LLVMValueRef field_ptr = LLVMBuildStructGEP2(
                ctx->builder, ctx_struct_type, ctx_ptr, (unsigned)c,
                llvm_tmp_name(ctx));
            LLVMValueRef var_ptr = LLVMBuildLoad2(
                ctx->builder, ctx->type_i8ptr, field_ptr,
                llvm_tmp_name(ctx));
            /* Declare in wrapper scope — the "alloca" is actually the
             * loaded pointer to the outer function's alloca.  Since
             * llvm_emit_identifier does Load2(type, alloca, ...) and
             * store operations do Store(val, alloca), this transparent
             * pointer indirection works correctly. */
            llvm_scope_declare(ctx, captured[c].name, var_ptr, captured[c].type);
        }

        /* Emit the task body */
        llvm_emit_statement(node->data.parallel.tasks[i], ctx);

        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))
                == NULL)
            LLVMBuildRet(ctx->builder, LLVMConstNull(ctx->type_i8ptr));

        llvm_scope_pop(ctx);
    }

    ctx->parallel_counter++;

    /* Restore insertion point */
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret;
    LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);

    /* -----------------------------------------------------------
     * 5) Spawn all tasks, await all.
     * ----------------------------------------------------------- */
    LLVMFuncEntry *spawn_fn = llvm_lookup_function(ctx, "pgy_spawn_export");
    LLVMFuncEntry *await_fn = llvm_lookup_function(ctx, "pgy_await_export");

    if (spawn_fn == NULL || await_fn == NULL) {
        /* Fallback: emit sequentially */
        for (size_t i = 0; i < count; i++)
            llvm_emit_statement(node->data.parallel.tasks[i], ctx);
        free(wrapper_fns);
        return;
    }

    LLVMValueRef *handles = calloc(count, sizeof(LLVMValueRef));
    for (size_t i = 0; i < count; i++) {
        LLVMValueRef fn_ptr = LLVMBuildBitCast(
            ctx->builder, wrapper_fns[i], ctx->type_i8ptr,
            llvm_tmp_name(ctx));

        LLVMValueRef args[] = { fn_ptr, ctx_i8ptr };
        handles[i] = LLVMBuildCall2(ctx->builder, spawn_fn->fn_type,
                                     spawn_fn->fn, args, 2,
                                     llvm_tmp_name(ctx));
    }

    for (size_t i = 0; i < count; i++) {
        LLVMValueRef args[] = { handles[i] };
        LLVMBuildCall2(ctx->builder, await_fn->fn_type,
                       await_fn->fn, args, 1, "");
    }

    free(handles);
    free(wrapper_fns);
}

static bool
llvm_select_case_parts(ASTNode *case_node, ASTNode **channel_out,
                       const char **bind_name_out, ASTNode **body_out)
{
    if (case_node == NULL || case_node->type != AST_BLOCK
        || case_node->data.block.count == 0)
        return false;

    ASTNode *first = case_node->data.block.statements[0];
    ASTNode *body = case_node->data.block.count >= 2
        ? case_node->data.block.statements[1] : NULL;

    if (first->type == AST_CHANNEL_RECV) {
        if (channel_out != NULL)
            *channel_out = first->data.channel_recv.channel;
        if (bind_name_out != NULL)
            *bind_name_out = NULL;
        if (body_out != NULL)
            *body_out = body;
        return true;
    }

    if (first->type == AST_ASSIGNMENT
        && first->data.assignment.target != NULL
        && first->data.assignment.target->type == AST_IDENTIFIER
        && first->data.assignment.value != NULL
        && first->data.assignment.value->type == AST_CHANNEL_RECV) {
        if (channel_out != NULL)
            *channel_out = first->data.assignment.value->data.channel_recv.channel;
        if (bind_name_out != NULL)
            *bind_name_out = first->data.assignment.target->data.identifier.name;
        if (body_out != NULL)
            *body_out = body;
        return true;
    }

    return false;
}

static void
llvm_emit_async_block(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode fake_block = {0};
    fake_block.type = AST_BLOCK;
    fake_block.data.block.statements = node->data.async_block.statements;
    fake_block.data.block.count = node->data.async_block.statement_count;

    LLVMValueRef saved_fn  = ctx->current_function;
    LLVMTypeRef saved_ret = ctx->current_ret_type;
    LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(ctx->builder);

    /* Reuse parallel wrapper generation, but spawn via async runtime and detach. */
    int saved_parallel_counter = ctx->parallel_counter;
    typedef struct { const char *name; LLVMValueRef alloca; LLVMTypeRef type; } CapturedVar;
    CapturedVar captured[MAX_SCOPE_VARS];
    int n_captured = 0;
    for (int i = 0; i < ctx->scope_depth; i++) {
        LLVMScopeFrame *frame = &ctx->scopes[i];
        for (int j = 0; j < frame->count && n_captured < MAX_SCOPE_VARS; j++) {
            captured[n_captured++] = (CapturedVar){
                frame->entries[j].name,
                frame->entries[j].alloca,
                frame->entries[j].type
            };
        }
    }
    bool has_captures = n_captured > 0;
    LLVMValueRef ctx_alloca = NULL;
    LLVMTypeRef ctx_struct_type = NULL;

    if (has_captures) {
        LLVMTypeRef *fields = calloc((size_t)n_captured, sizeof(LLVMTypeRef));
        for (int i = 0; i < n_captured; i++)
            fields[i] = ctx->type_i8ptr;
        ctx_struct_type = LLVMStructCreateNamed(ctx->context, llvm_tmp_name(ctx));
        LLVMStructSetBody(ctx_struct_type, fields, (unsigned)n_captured, 0);
        free(fields);

        ctx_alloca = LLVMBuildAlloca(ctx->builder, ctx_struct_type, "_actx");
        for (int i = 0; i < n_captured; i++) {
            LLVMValueRef gep = LLVMBuildStructGEP2(ctx->builder, ctx_struct_type,
                ctx_alloca, (unsigned)i, llvm_tmp_name(ctx));
            LLVMValueRef cast = LLVMBuildBitCast(ctx->builder, captured[i].alloca,
                ctx->type_i8ptr, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, cast, gep);
        }
    }

    LLVMValueRef ctx_i8ptr = has_captures
        ? LLVMBuildBitCast(ctx->builder, ctx_alloca, ctx->type_i8ptr, llvm_tmp_name(ctx))
        : LLVMConstNull(ctx->type_i8ptr);

    LLVMTypeRef wrapper_params[] = { ctx->type_i8ptr };
    LLVMTypeRef wrapper_type = LLVMFunctionType(ctx->type_i8ptr, wrapper_params, 1, 0);
    char fn_name[64];
    snprintf(fn_name, sizeof(fn_name), "_pgy_async_%d_0", ctx->parallel_counter++);
    LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, wrapper_type);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx->context, fn, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, entry);
    ctx->current_function = fn;
    ctx->current_ret_type = ctx->type_i8ptr;
    llvm_scope_push(ctx);
    if (has_captures) {
        LLVMValueRef arg0 = LLVMGetParam(fn, 0);
        LLVMValueRef ctx_ptr = LLVMBuildBitCast(ctx->builder, arg0,
            LLVMPointerType(ctx_struct_type, 0), "_actx");
        for (int i = 0; i < n_captured; i++) {
            LLVMValueRef field_ptr = LLVMBuildStructGEP2(ctx->builder, ctx_struct_type,
                ctx_ptr, (unsigned)i, llvm_tmp_name(ctx));
            LLVMValueRef var_ptr_i8 = LLVMBuildLoad2(ctx->builder, ctx->type_i8ptr,
                field_ptr, llvm_tmp_name(ctx));
            LLVMValueRef var_ptr = LLVMBuildBitCast(ctx->builder, var_ptr_i8,
                LLVMPointerType(captured[i].type, 0), llvm_tmp_name(ctx));
            llvm_scope_declare(ctx, captured[i].name, var_ptr, captured[i].type);
        }
    }
    llvm_emit_statement(&fake_block, ctx);
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildRet(ctx->builder, LLVMConstNull(ctx->type_i8ptr));
    llvm_scope_pop(ctx);

    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret;
    LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);

    LLVMFuncEntry *spawn_fn = llvm_lookup_function(ctx, "pgy_async_spawn_export");
    LLVMFuncEntry *detach_fn = llvm_lookup_function(ctx, "pgy_async_detach_export");
    if (spawn_fn == NULL || detach_fn == NULL) {
        ctx->parallel_counter = saved_parallel_counter;
        llvm_emit_statement(&fake_block, ctx);
        return;
    }

    LLVMValueRef fn_ptr = LLVMBuildBitCast(ctx->builder, fn, ctx->type_i8ptr, llvm_tmp_name(ctx));
    LLVMValueRef spawn_args[] = { fn_ptr, ctx_i8ptr };
    LLVMValueRef handle = LLVMBuildCall2(ctx->builder, spawn_fn->fn_type, spawn_fn->fn,
        spawn_args, 2, llvm_tmp_name(ctx));
    LLVMValueRef detach_args[] = { handle };
    LLVMBuildCall2(ctx->builder, detach_fn->fn_type, detach_fn->fn, detach_args, 1, "");
}

static void
llvm_emit_select_stmt(ASTNode *node, LLVMGenCtx *ctx)
{
    LLVMValueRef fn = ctx->current_function;
    LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "select.end");
    LLVMBasicBlockRef next_check_bb = NULL;

    for (size_t i = 0; i < node->data.select_stmt.case_count; i++) {
        ASTNode *case_node = node->data.select_stmt.cases[i];
        ASTNode *channel = NULL;
        ASTNode *body = NULL;
        const char *bind_name = NULL;
        bool valid_case = llvm_select_case_parts(case_node, &channel, &bind_name, &body);

        LLVMBasicBlockRef case_bb = LLVMAppendBasicBlockInContext(
            ctx->context, fn, "select.case");
        LLVMBasicBlockRef fail_bb = LLVMAppendBasicBlockInContext(
            ctx->context, fn, "select.next");

        if (next_check_bb != NULL)
            LLVMPositionBuilderAtEnd(ctx->builder, next_check_bb);

        if (valid_case && channel != NULL && channel->type == AST_IDENTIFIER) {
            const char *channel_name = channel->data.identifier.name;
            const char *inner = llvm_lookup_channel_inner(ctx, channel_name);
            LLVMVarEntry *ch_var = llvm_scope_lookup(ctx, channel_name);
            if (inner == NULL) inner = "Int";

            if (ch_var != NULL) {
                char fn_name[128];
                if (bind_name != NULL) {
                    LLVMTypeRef val_ty = pergyra_type_to_llvm(ctx, inner);
                    LLVMValueRef tmp = llvm_create_entry_alloca(ctx, val_ty, llvm_tmp_name(ctx));
                    snprintf(fn_name, sizeof(fn_name), "pgy_channel_try_recv_%s", inner);
                    LLVMFuncEntry *try_fn = llvm_lookup_function(ctx, fn_name);
                    if (try_fn != NULL) {
                        LLVMValueRef args[] = { ch_var->alloca, tmp };
                        LLVMValueRef ok = LLVMBuildCall2(ctx->builder, try_fn->fn_type,
                            try_fn->fn, args, 2, llvm_tmp_name(ctx));
                        LLVMBuildCondBr(ctx->builder, ok, case_bb, fail_bb);

                        LLVMPositionBuilderAtEnd(ctx->builder, case_bb);
                        llvm_scope_push(ctx);
                        {
                            LLVMValueRef bind_alloca =
                                llvm_create_entry_alloca(ctx, val_ty, bind_name);
                            LLVMValueRef received = LLVMBuildLoad2(ctx->builder, val_ty, tmp,
                                llvm_tmp_name(ctx));
                            LLVMBuildStore(ctx->builder, received, bind_alloca);
                            llvm_scope_declare(ctx, pergyra_strdup(bind_name),
                                               bind_alloca, val_ty);
                        }
                        if (body != NULL)
                            llvm_emit_statement(body, ctx);
                        llvm_scope_pop(ctx);
                        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
                            LLVMBuildBr(ctx->builder, merge_bb);
                        next_check_bb = fail_bb;
                        continue;
                    }
                } else {
                    snprintf(fn_name, sizeof(fn_name), "pgy_channel_ready_%s", inner);
                    LLVMFuncEntry *ready_fn = llvm_lookup_function(ctx, fn_name);
                    if (ready_fn != NULL) {
                        LLVMValueRef args[] = { ch_var->alloca };
                        LLVMValueRef ready = LLVMBuildCall2(ctx->builder, ready_fn->fn_type,
                            ready_fn->fn, args, 1, llvm_tmp_name(ctx));
                        LLVMBuildCondBr(ctx->builder, ready, case_bb, fail_bb);

                        LLVMPositionBuilderAtEnd(ctx->builder, case_bb);
                        {
                            char recv_name[128];
                            snprintf(recv_name, sizeof(recv_name), "pgy_channel_recv_val_%s", inner);
                            LLVMFuncEntry *recv_fn = llvm_lookup_function(ctx, recv_name);
                            if (recv_fn != NULL) {
                                LLVMValueRef recv_args[] = { ch_var->alloca };
                                (void)LLVMBuildCall2(ctx->builder, recv_fn->fn_type,
                                    recv_fn->fn, recv_args, 1, "");
                            }
                        }
                        if (body != NULL)
                            llvm_emit_statement(body, ctx);
                        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
                            LLVMBuildBr(ctx->builder, merge_bb);
                        next_check_bb = fail_bb;
                        continue;
                    }
                }
            }
        }

        LLVMBuildBr(ctx->builder, case_bb);
        LLVMPositionBuilderAtEnd(ctx->builder, case_bb);
        if (case_node != NULL)
            llvm_emit_statement(case_node, ctx);
        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
            LLVMBuildBr(ctx->builder, merge_bb);
        next_check_bb = fail_bb;
    }

    if (next_check_bb != NULL)
        LLVMPositionBuilderAtEnd(ctx->builder, next_check_bb);
    if (node->data.select_stmt.default_case != NULL)
        llvm_emit_statement(node->data.select_stmt.default_case, ctx);
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, merge_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, merge_bb);
}

void
llvm_emit_statement(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node == NULL || ctx->has_error)
        return;

    /* If current block already has a terminator, skip */
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) != NULL)
        return;

    switch (node->type) {
    case AST_LET_DECL:
        llvm_emit_let_decl(node, ctx);
        break;

    case AST_RETURN:
        llvm_emit_return_stmt(node, ctx);
        break;

    case AST_BREAK:
        if (ctx->loop_depth > 0) {
            llvm_emit_defers_from(ctx,
                ctx->loop_defer_base_depth[ctx->loop_depth - 1]);
            LLVMBuildBr(ctx->builder,
                ctx->loop_break_blocks[ctx->loop_depth - 1]);
        }
        break;
    case AST_ENUM_DECL:
        /* Enums are compile-time only — no IR needed */
        break;
    case AST_CONTINUE:
        if (ctx->loop_depth > 0) {
            llvm_emit_defers_from(ctx,
                ctx->loop_defer_base_depth[ctx->loop_depth - 1]);
            LLVMBuildBr(ctx->builder,
                ctx->loop_continue_blocks[ctx->loop_depth - 1]);
        }
        break;

    case AST_IF_STMT:
        llvm_emit_if_stmt(node, ctx);
        break;

    case AST_WHILE_LOOP:
        llvm_emit_while_loop(node, ctx);
        break;

    case AST_FOR_LOOP:
        llvm_emit_for_loop(node, ctx);
        break;

    case AST_MATCH_STMT:
        llvm_emit_match_stmt(node, ctx);
        break;

    case AST_WITH_STMT:
        llvm_emit_with_stmt(node, ctx);
        break;

    case AST_BLOCK:
        llvm_emit_block(node, ctx);
        break;

    case AST_ASYNC_BLOCK:
        llvm_emit_async_block(node, ctx);
        break;

    case AST_PARALLEL_BLOCK:
        llvm_emit_parallel_block(node, ctx);
        break;

    case AST_SELECT_STMT:
        llvm_emit_select_stmt(node, ctx);
        break;

    case AST_FUNC_DECL:
    case AST_CLASS_DECL:
    case AST_ACTOR_DECL:
    case AST_ABILITY_DECL:
    case AST_ROLE_DECL:
    case AST_PARTY_DECL:
    case AST_SYSTEMIC_DECL:
    case AST_WORLD_DECL:
    case AST_EVENT_DECL:
    case AST_IMPORT_DECL:
    case AST_NAMESPACE_DECL:
        /* Handled in program pass or declaration-only — skip here */
        break;

    case AST_EXTERN_BLOCK:
        /* extern "C" { func ...; } — handled in program pass (Pass 0) */
        break;

    case AST_UNSAFE_BLOCK:
        /* unsafe { ... } — emit body directly, no safety wrappers */
        if (node->data.unsafe_block.body != NULL)
            llvm_emit_block(node->data.unsafe_block.body, ctx);
        break;

    case AST_DEFER_STMT:
        if (node->data.defer_stmt.body != NULL)
            llvm_register_defer(node->data.defer_stmt.body, ctx);
        break;

    case AST_BIND_STMT: {
        /* bind party.slot = Role;
         * → party_var.slot_vtable = &Role_Ability_vtable_instance */
        const char *party_var  = node->data.bind_stmt.party_var;
        const char *slot_name  = node->data.bind_stmt.slot_name;
        const char *role_name  = node->data.bind_stmt.role_name;

        if (party_var == NULL || slot_name == NULL || role_name == NULL)
            break;

        /* Look up the party variable */
        LLVMVarEntry *pvar = llvm_scope_lookup(ctx, party_var);
        if (pvar == NULL) break;

        const char *party_class_name = llvm_lookup_var_class(ctx, party_var);
        LLVMClassTypeEntry *cls = party_class_name
            ? llvm_lookup_class(ctx, party_class_name) : NULL;
        if (cls == NULL) break;

        char vt_field[256];
        snprintf(vt_field, sizeof(vt_field), "%s_vtable", slot_name);
        int field_idx = -1;
        for (int fi = 0; fi < cls->field_count; fi++) {
            if (strcmp(cls->fields[fi].field_name, vt_field) == 0) {
                field_idx = cls->fields[fi].index;
                break;
            }
        }
        if (field_idx < 0) break;

        /* Find the Role's vtable global.
         * Convention: RoleName_AbilityName_vtable_instance */
        char global_prefix[256];
        snprintf(global_prefix, sizeof(global_prefix), "%s_", role_name);
        LLVMValueRef vt_global = NULL;
        LLVMValueRef g = LLVMGetFirstGlobal(ctx->module);
        while (g != NULL) {
            const char *gname = LLVMGetValueName(g);
            if (gname != NULL
                && strncmp(gname, global_prefix, strlen(global_prefix)) == 0
                && strstr(gname, "_vtable_instance") != NULL) {
                vt_global = g;
                break;
            }
            g = LLVMGetNextGlobal(g);
        }
        if (vt_global == NULL) break;

        /* GEP to vtable pointer field + store */
        LLVMValueRef party_alloca = pvar->alloca;
        LLVMValueRef field_ptr = LLVMBuildStructGEP2(ctx->builder,
            cls->struct_type, party_alloca, (unsigned)field_idx,
            llvm_tmp_name(ctx));
        LLVMValueRef vt_ptr = LLVMBuildBitCast(ctx->builder,
            vt_global, ctx->type_i8ptr, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, vt_ptr, field_ptr);
        break;
    }

    /* Expression statements */
    case AST_CALL:
    case AST_ASSIGNMENT:
    case AST_BINARY:
    case AST_UNARY:
    case AST_IDENTIFIER:
    case AST_MEMBER_ACCESS:
    case AST_NUMBER:
    case AST_STRING:
    case AST_BOOLEAN:
    case AST_CHANNEL_SEND:
    case AST_CHANNEL_RECV:
    case AST_SPAWN_EXPR:
    case AST_AWAIT_EXPR:
    case AST_ARRAY_ACCESS:
    case AST_PARTY_INSTANCE:
    case AST_CONTEXT_ACCESS:
    case AST_TASK_GROUP:
    case AST_EVENT_SUBSCRIBE:
    case AST_EVENT_UNSUBSCRIBE:
    case AST_EVENT_INVOKE:
        llvm_emit_expression(node, ctx);
        break;

    default:
        fprintf(stderr, "[llvm] warning: unhandled statement AST type %d\n",
                (int)node->type);
        break;
    }
}

#endif /* PGY_LLVM_ENABLED */
