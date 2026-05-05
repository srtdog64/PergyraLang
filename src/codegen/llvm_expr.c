/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend — expression emission
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

LLVMValueRef llvm_emit_expression(ASTNode *node, LLVMGenCtx *ctx);
#include "llvm_expr_member_lvalue.h"
#include "llvm_expr_boundary_projection_helpers.h"
#include "llvm_expr_host_spawn_literal_helpers.h"
#include "llvm_expr_spawn_call_helpers.h"
#include "llvm_expr_banner_string_helpers.h"
#include "llvm_expr_string_coerce.h"
#include "llvm_expr_identifier_slot_helpers.h"
#include "llvm_expr_assignment_member_projection.h"
#include "llvm_expr_member_access.h"
#include "llvm_expr_scalar_core.h"
#include "llvm_expr_call_projection_sync.h"
#include "llvm_expr_call_methods_domain_slice.h"
#include "llvm_member_call_emit.h"
#include "llvm_expr_call_owners.h"

LLVMValueRef
llvm_emit_expression(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node == NULL || ctx->has_error)
        return NULL;

    switch (node->type) {
    case AST_NUMBER:        return llvm_emit_number(node, ctx);
    case AST_STRING:        return llvm_emit_string(node, ctx);
    case AST_BOOLEAN:       return llvm_emit_boolean(node, ctx);
    case AST_IDENTIFIER:    return llvm_emit_identifier(node, ctx);
    case AST_BINARY:        return llvm_emit_binary(node, ctx);
    case AST_UNARY:         return llvm_emit_unary(node, ctx);
    case AST_CALL:          return llvm_emit_call(node, ctx);
    case AST_ASSIGNMENT:    return llvm_emit_assignment(node, ctx);
    case AST_MEMBER_ACCESS: return llvm_emit_member_access(node, ctx);
    case AST_TUPLE_LITERAL: {
        size_t n = node->data.tuple_literal.count;
        if (n == 0)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        /* Tuple element values + type refs — LLVMStructTypeInContext copies
         * the type array, and BuildInsertValue consumes values immediately.
         * Buffers are ctx-scratch-safe. */
        LLVMValueRef *vals = pgy_arena_calloc(&ctx->scratch,
            n * sizeof(LLVMValueRef));
        LLVMTypeRef  *tys  = pgy_arena_calloc(&ctx->scratch,
            n * sizeof(LLVMTypeRef));
        if (vals == NULL || tys == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        for (size_t i = 0; i < n; i++) {
            vals[i] = llvm_emit_expression(node->data.tuple_literal.elements[i], ctx);
            if (vals[i] == NULL)
                vals[i] = LLVMConstInt(ctx->type_i32, 0, 0);
            tys[i] = LLVMTypeOf(vals[i]);
        }
        LLVMTypeRef tup_ty = LLVMStructTypeInContext(ctx->context, tys,
            (unsigned)n, 0);
        LLVMValueRef agg = LLVMGetUndef(tup_ty);
        for (size_t i = 0; i < n; i++)
            agg = LLVMBuildInsertValue(ctx->builder, agg, vals[i],
                (unsigned)i, llvm_tmp_name(ctx));
        /* vals / tys are ctx->scratch-owned. */
        return agg;
    }

    case AST_ARRAY_LITERAL: {
        size_t count = node->data.array_literal.count;
        const char *inner_name = NULL;
        LLVMTypeRef elem_type = ctx->type_i32;
        if (count > 0) {
            LLVMValueRef first = llvm_emit_expression(node->data.array_literal.elements[0], ctx);
            if (first != NULL) {
                elem_type = LLVMTypeOf(first);
                const char *suffix = llvm_type_to_suffix(ctx, elem_type);
                if (suffix != NULL && strcmp(suffix, "Unknown") != 0)
                    inner_name = suffix;
            }
        } else if (ctx->expected_type_name != NULL
                   && strncmp(ctx->expected_type_name, "Array<", 6) == 0) {
            inner_name = llvm_constructed_arg_name_at(ctx->expected_type_name, 0);
        }
        if (inner_name == NULL || inner_name[0] == '\0') {
            llvm_expr_set_missing_type_error(ctx, node,
                "array literal expression");
            return LLVMConstInt(ctx->type_i32, 0, 0);
        }

        LLVMTypeRef array_type = llvm_array_struct_type(ctx, inner_name);
        LLVMValueRef tmp = llvm_create_entry_alloca(ctx, array_type, llvm_tmp_name(ctx));
        char push_fn_name[64];
        snprintf(push_fn_name, sizeof(push_fn_name), "pgy_array_push_%s", inner_name);
        LLVMFuncEntry *push_fn = llvm_lookup_function(ctx, push_fn_name);
        if (push_fn == NULL && count > 0) {
            llvm_required_runtime_function(ctx, node,
                "array literal expression", "ArrayPush", push_fn_name);
            return LLVMConstInt(ctx->type_i32, 0, 0);
        }
        LLVMBuildStore(ctx->builder, LLVMConstNull(array_type), tmp);
        for (size_t i = 0; i < count; i++) {
            LLVMValueRef elem = llvm_emit_expression(node->data.array_literal.elements[i], ctx);
            if (elem != NULL) {
                LLVMValueRef args[] = { tmp, elem };
                LLVMBuildCall2(ctx->builder, push_fn->fn_type, push_fn->fn, args, 2, "");
            }
        }
        return LLVMBuildLoad2(ctx->builder, array_type, tmp, llvm_tmp_name(ctx));
    }

    case AST_ARRAY_ACCESS: {
        ASTNode *array_node = node->data.array_access.array;
        LLVMValueRef arr = llvm_emit_expression(array_node, ctx);
        LLVMValueRef idx = llvm_emit_expression(
            node->data.array_access.index, ctx);
        if (arr == NULL || idx == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        if (array_node != NULL && array_node->type == AST_IDENTIFIER) {
            const char *name = array_node->data.identifier.name;
            LLVMVarEntry *arr_var = llvm_scope_lookup(ctx, name);
            LLVMArrayVarEntry *entry = llvm_lookup_array_var(ctx, name);
            if (arr_var != NULL && entry != NULL) {
                const char *suffix = llvm_type_to_suffix(ctx, entry->elem_type);
                if (suffix != NULL && strcmp(suffix, "Unknown") != 0) {
                    const char *struct_name = LLVMGetStructName(arr_var->type);
                    const char *fn_prefix = "pgy_array_get_";
                    char fn_name[64];
                    if (struct_name != NULL
                        && strncmp(struct_name, "PgySlice_", 9) == 0) {
                        fn_prefix = "pgy_slice_get_";
                    }
                    snprintf(fn_name, sizeof(fn_name), "%s%s", fn_prefix, suffix);
                    LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
                    if (fn != NULL) {
                        LLVMValueRef index64 = idx;
                        if (LLVMTypeOf(index64) != ctx->type_i64)
                            index64 = LLVMBuildSExtOrBitCast(ctx->builder, index64,
                                ctx->type_i64, llvm_tmp_name(ctx));
                        LLVMValueRef args[] = { arr_var->alloca, index64 };
                        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                            args, 2, llvm_tmp_name(ctx));
                    }
                    llvm_required_runtime_function(ctx, node,
                        "indexed collection access",
                        struct_name != NULL
                            && strncmp(struct_name, "PgySlice_", 9) == 0
                            ? "SliceGet" : "ArrayGet",
                        fn_name);
                    return LLVMConstInt(ctx->type_i32, 0, 0);
                }
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
                return LLVMConstInt(ctx->type_i32, 0, 0);

            LLVMValueRef data_ptr = llvm_array_data_ptr(ctx, arr);
            LLVMTypeRef elem_ty = llvm_stmt_resolve_array_elem_type(
                ctx, array_node, data_ptr);
            if (elem_ty == NULL)
                elem_ty = ctx->type_i32;
            LLVMValueRef gep = LLVMBuildGEP2(ctx->builder,
                elem_ty, data_ptr, &idx, 1, llvm_tmp_name(ctx));
            return LLVMBuildLoad2(ctx->builder, elem_ty,
                gep, llvm_tmp_name(ctx));
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    case AST_CONTEXT_ACCESS: {
        /* context.GetRole("slotName") → load role slot from self (i8*)
         * self is in scope as the party/roster method's first param */
        LLVMVarEntry *self_var = llvm_scope_lookup(ctx, "self");
        if (self_var == NULL)
            return LLVMConstNull(ctx->type_i8ptr);

        /* For now: return the self pointer cast — the role slot is
         * accessed through the party struct, which self points to */
        LLVMValueRef self_val = LLVMBuildLoad2(ctx->builder,
            ctx->type_i8ptr, self_var->alloca, llvm_tmp_name(ctx));
        return self_val;
    }

    case AST_PARTY_INSTANCE: {
        /* PartyType { slot1: val1, slot2: val2 }
         * → alloca struct, store fields, return value */
        const char *pty = node->data.party_instance.party_type;
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, pty);
        if (cls == NULL)
            return LLVMConstNull(ctx->type_i8ptr);

        LLVMValueRef alloca = llvm_create_entry_alloca(ctx,
            cls->struct_type, llvm_tmp_name(ctx));

        /* Zero-initialize */
        LLVMValueRef zero = LLVMConstNull(cls->struct_type);
        LLVMBuildStore(ctx->builder, zero, alloca);

        /* Store each assignment */
        for (size_t i = 0; i < node->data.party_instance.assignment_count; i++) {
            const char *slot_name = node->data.party_instance.assignments[i].slot_name;
            ASTNode *val_node = node->data.party_instance.assignments[i].value;

            /* Find field index */
            for (int f = 0; f < cls->field_count; f++) {
                if (strcmp(cls->fields[f].field_name, slot_name) == 0) {
                    LLVMValueRef field_ptr = LLVMBuildStructGEP2(
                        ctx->builder, cls->struct_type, alloca,
                        (unsigned)cls->fields[f].index,
                        llvm_tmp_name(ctx));
                    LLVMValueRef val = llvm_emit_expression(val_node, ctx);
                    if (val != NULL)
                        LLVMBuildStore(ctx->builder, val, field_ptr);
                    break;
                }
            }
        }

        return LLVMBuildLoad2(ctx->builder, cls->struct_type,
            alloca, llvm_tmp_name(ctx));
    }

    case AST_TASK_GROUP: {
        /* TaskGroup { tasks... } → emit tasks sequentially (MVP) */
        for (size_t i = 0; i < node->data.task_group.task_count; i++) {
            if (node->data.task_group.tasks[i] != NULL)
                llvm_emit_expression(node->data.task_group.tasks[i], ctx);
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    case AST_CHANNEL_SEND: {
        /* ch <- value → pgy_channel_send_T(&ch, value) */
        return llvm_emit_channel_send_expr(node, ctx);
    }

    case AST_CHANNEL_RECV: {
        /* <- ch → pgy_channel_recv_val_T(&ch) */
        return llvm_emit_channel_recv_expr(node, ctx);
    }

    case AST_SPAWN_EXPR:
        return llvm_emit_spawn_expr(node, ctx);

    case AST_AWAIT_EXPR:
        if (node->data.await_expr.expression != NULL) {
            ASTNode *inner_expr = node->data.await_expr.expression;
            const char *inner = NULL;
            bool is_remote = false;
            if (inner_expr->type == AST_IDENTIFIER)
                inner = llvm_lookup_future_inner(ctx, inner_expr->data.identifier.name);
            if (inner_expr->type == AST_IDENTIFIER)
                is_remote = llvm_lookup_future_is_remote(ctx, inner_expr->data.identifier.name);
            if (inner != NULL) {
                LLVMValueRef task = llvm_emit_expression(inner_expr, ctx);
                return llvm_await_task_handle(ctx, node, task, inner, is_remote);
            }
            return llvm_emit_expression(inner_expr, ctx);
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    case AST_LAMBDA_EXPR: {
        /* Generate a static LLVM function and return its pointer */
        int lid = ctx->lambda_counter++;
        int pc = (int)node->data.lambda_expr.param_count;

        /* Determine return type */
        LLVMTypeRef ret_type = ctx->type_i32;
        if (node->data.lambda_expr.return_type != NULL)
            ret_type = ast_type_to_llvm(ctx, node->data.lambda_expr.return_type);
        else if (node->data.lambda_expr.body != NULL
                 && node->data.lambda_expr.body->type == AST_BLOCK)
            ret_type = ctx->type_void;

        /* Parameter types (default i32) */
        LLVMTypeRef lparams[8];
        for (int j = 0; j < pc && j < 8; j++) {
            ASTNode *p = node->data.lambda_expr.params[j];
            if (p->type == AST_LET_DECL && p->data.let_decl.type != NULL)
                lparams[j] = ast_type_to_llvm(ctx, p->data.let_decl.type);
            else
                lparams[j] = ctx->type_i32;
        }

        char lname[128];
        snprintf(lname, sizeof(lname), "pgy_lambda_%d", lid);
        LLVMTypeRef lft = LLVMFunctionType(ret_type,
            lparams, (unsigned)pc, 0);
        LLVMValueRef lfn = LLVMAddFunction(ctx->module, lname, lft);
        llvm_register_function(ctx, LLVMGetValueName(lfn),
            lfn, lft, ret_type);

        /* Save current builder state */
        LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(ctx->builder);
        LLVMValueRef saved_fn = ctx->current_function;
        LLVMTypeRef saved_ret = ctx->current_ret_type;

        ctx->current_function = lfn;
        ctx->current_ret_type = ret_type;

        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(
            ctx->context, lfn, "entry");
        LLVMPositionBuilderAtEnd(ctx->builder, entry);

        llvm_scope_push(ctx);
        for (int j = 0; j < pc; j++) {
            ASTNode *p = node->data.lambda_expr.params[j];
            const char *pname = (p->type == AST_IDENTIFIER)
                ? p->data.identifier.name : p->data.let_decl.name;
            LLVMValueRef alloca = LLVMBuildAlloca(ctx->builder,
                lparams[j], pname);
            LLVMBuildStore(ctx->builder, LLVMGetParam(lfn, (unsigned)j),
                alloca);
            llvm_scope_declare(ctx, pname, alloca, lparams[j]);
        }

        if (node->data.lambda_expr.body != NULL) {
            if (node->data.lambda_expr.body->type == AST_BLOCK) {
                llvm_emit_block(node->data.lambda_expr.body, ctx);
            } else {
                LLVMValueRef val = llvm_emit_expression(
                    node->data.lambda_expr.body, ctx);
                if (ret_type != ctx->type_void)
                    LLVMBuildRet(ctx->builder, val);
                else
                    LLVMBuildRetVoid(ctx->builder);
            }
        }

        /* Ensure terminator exists */
        if (LLVMGetBasicBlockTerminator(
                LLVMGetInsertBlock(ctx->builder)) == NULL) {
            if (ret_type == ctx->type_void)
                LLVMBuildRetVoid(ctx->builder);
            else
                LLVMBuildRet(ctx->builder,
                    LLVMConstInt(ret_type, 0, 0));
        }

        llvm_scope_pop(ctx);

        /* Restore builder state */
        ctx->current_function = saved_fn;
        ctx->current_ret_type = saved_ret;
        LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);

        return lfn;
    }

    case AST_EVENT_SUBSCRIBE: {
        /* event += handler → EventName_SUBSCRIBE(&event, handler) */
        ASTNode *evt = node->data.event_op.event;
        ASTNode *handler = node->data.event_op.handler;

        const char *evt_name = NULL;
        if (evt != NULL && evt->type == AST_IDENTIFIER)
            evt_name = evt->data.identifier.name;

        if (evt_name != NULL) {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_SUBSCRIBE", evt_name);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
            LLVMVarEntry *ev = llvm_scope_lookup(ctx, evt_name);
            LLVMValueRef ev_ptr = (ev != NULL) ? ev->alloca
                : LLVMGetNamedGlobal(ctx->module, evt_name);
            LLVMValueRef hval = llvm_emit_expression(handler, ctx);

            if (fn == NULL || ev_ptr == NULL) {
                llvm_set_error_at_with_hints(ctx, node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "LLVM event subscribe requires generated event function '%s' and event storage '%s'",
                    fname, evt_name);
                return LLVMConstInt(ctx->type_i32, 0, 0);
            }
            LLVMValueRef args[] = { ev_ptr, hval };
            LLVMBuildCall2(ctx->builder, fn->fn_type,
                fn->fn, args, 2, "");
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    case AST_EVENT_UNSUBSCRIBE: {
        /* event -= handler → EventName_UNSUBSCRIBE(&event, handler) */
        ASTNode *evt = node->data.event_op.event;
        ASTNode *handler = node->data.event_op.handler;

        const char *evt_name = NULL;
        if (evt != NULL && evt->type == AST_IDENTIFIER)
            evt_name = evt->data.identifier.name;

        if (evt_name != NULL) {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_UNSUBSCRIBE", evt_name);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
            LLVMVarEntry *ev = llvm_scope_lookup(ctx, evt_name);
            LLVMValueRef ev_ptr = (ev != NULL) ? ev->alloca
                : LLVMGetNamedGlobal(ctx->module, evt_name);
            LLVMValueRef hval = llvm_emit_expression(handler, ctx);

            if (fn == NULL || ev_ptr == NULL) {
                llvm_set_error_at_with_hints(ctx, node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "LLVM event unsubscribe requires generated event function '%s' and event storage '%s'",
                    fname, evt_name);
                return LLVMConstInt(ctx->type_i32, 0, 0);
            }
            LLVMValueRef args[] = { ev_ptr, hval };
            LLVMBuildCall2(ctx->builder, fn->fn_type,
                fn->fn, args, 2, "");
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    case AST_EVENT_INVOKE: {
        /* Emit(event, args...) → EventName_INVOKE(&event, args...) */
        ASTNode *evt = node->data.event_invoke.event;
        const char *evt_name = NULL;
        if (evt != NULL && evt->type == AST_IDENTIFIER)
            evt_name = evt->data.identifier.name;

        if (evt_name != NULL) {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_INVOKE", evt_name);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
            LLVMVarEntry *ev = llvm_scope_lookup(ctx, evt_name);
            LLVMValueRef ev_ptr = (ev != NULL) ? ev->alloca
                : LLVMGetNamedGlobal(ctx->module, evt_name);

            if (fn == NULL || ev_ptr == NULL) {
                llvm_set_error_at_with_hints(ctx, node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "LLVM event invoke requires generated event function '%s' and event storage '%s'",
                    fname, evt_name);
                return LLVMConstInt(ctx->type_i32, 0, 0);
            }
            size_t ac = node->data.event_invoke.arg_count;
            LLVMValueRef *args = pgy_arena_calloc(&ctx->scratch,
                (ac + 1) * sizeof(LLVMValueRef));
            args[0] = ev_ptr;
            for (size_t j = 0; j < ac; j++)
                args[j + 1] = llvm_emit_expression(
                    node->data.event_invoke.arguments[j], ctx);
            LLVMBuildCall2(ctx->builder, fn->fn_type,
                fn->fn, args, (unsigned)(ac + 1), "");
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    case AST_WORLD_ACTIVATE:
    case AST_WORLD_DEACTIVATE:
    case AST_WORLD_MAINTAIN:
    case AST_WORLD_STATE:
    case AST_ZONE_APPLY:
    case AST_ZONE_LINK:
    case AST_ZONE_DETACH:
    case AST_ZONE_UNLINK:
    case AST_ZONE_REFRESH:
    case AST_ZONE_AUTHORITY:
    case AST_ZONE_STATE:
        /* Category 2 (decl sub-metadata) safety-net terminus.
         * See docs/95_ast_dispatch_partition.md for the partition model.
         *
         * These domain verbs should only be consumed by llvm_domain.c
         * handlers that index world_decl / zone_decl child arrays.  If
         * one arrives here it means a parser or dispatcher change
         * started routing them through expression emission — raise an
         * explicit diagnostic so the regression surfaces immediately. */
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM domain AST node %d reached expression emission; domain operations must lower through MIR/domain emitters, not silent expression fallback",
            (int)node->type);
        return LLVMConstInt(ctx->type_i32, 0, 0);

    default:
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM expression emitter has no lowering for AST node type %d; add an explicit lowering or route this declaration metadata through the domain/MIR emitter",
            (int)node->type);
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }
}

#endif /* PGY_LLVM_ENABLED */
