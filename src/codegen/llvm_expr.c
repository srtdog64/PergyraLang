/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend — expression emission
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

LLVMValueRef llvm_emit_expression(ASTNode *node, LLVMGenCtx *ctx);
#include "llvm_expr_emit_support.h"
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
#include "llvm_expr_event_calls.h"
#include "llvm_member_call_emit.h"
#include "llvm_expr_call_owners.h"

LLVMValueRef
llvm_emit_expression(ASTNode *node, LLVMGenCtx *ctx)
{
    if (ctx == NULL || node == NULL || ctx->has_error)
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
            return llvm_expression_error(ctx, node,
                "LLVM tuple literal allocation failed");
        for (size_t i = 0; i < n; i++) {
            vals[i] = llvm_emit_expression(node->data.tuple_literal.elements[i], ctx);
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
        /* vals / tys are ctx->scratch-owned. */
        return agg;
    }

    case AST_ARRAY_LITERAL: {
        size_t count = node->data.array_literal.count;
        const char *inner_name = NULL;
        char inner_name_buf[256];
        LLVMTypeRef elem_type = ctx->type_i32;
        LLVMValueRef first_value = NULL;
        if (count > 0) {
            first_value = llvm_emit_expression(node->data.array_literal.elements[0], ctx);
            if (first_value == NULL)
                return llvm_expression_error(ctx, node,
                    "LLVM array literal could not lower element 0");
            {
                elem_type = LLVMTypeOf(first_value);
                const char *suffix = llvm_type_to_suffix(ctx, elem_type);
                if (suffix != NULL && strcmp(suffix, "Unknown") != 0)
                    inner_name = suffix;
            }
        } else if (ctx->expected_type_name != NULL
                   && strncmp(ctx->expected_type_name, "Array<", 6) == 0) {
            if (llvm_constructed_arg_name_copy(ctx->expected_type_name, 0,
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
        LLVMValueRef tmp = llvm_create_entry_alloca(ctx, array_type, llvm_tmp_name(ctx));
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
                : llvm_emit_expression(node->data.array_literal.elements[i], ctx);
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

    case AST_ARRAY_ACCESS: {
        ASTNode *array_node = node->data.array_access.array;
        LLVMValueRef arr = llvm_emit_expression(array_node, ctx);
        LLVMValueRef idx = llvm_emit_expression(
            node->data.array_access.index, ctx);
        if (arr == NULL || idx == NULL)
            return llvm_expression_error(ctx, node,
                "LLVM array access could not lower receiver or index expression");

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
                    if (!llvm_expr_runtime_name(ctx, node, fn_name,
                            sizeof(fn_name), fn_prefix, suffix))
                        return NULL;
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

    case AST_CONTEXT_ACCESS: {
        /* context.GetRole("slotName") → load role slot from self (i8*)
         * self is in scope as the party/roster method's first param */
        LLVMVarEntry *self_var = llvm_scope_lookup(ctx, "self");
        if (self_var == NULL)
            return llvm_expression_error(ctx, node,
                "LLVM context access requires a registered self parameter");

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
            return llvm_expression_error(ctx, node,
                "LLVM party instance requires registered class metadata");

        LLVMValueRef alloca = llvm_create_entry_alloca(ctx,
            cls->struct_type, llvm_tmp_name(ctx));
        if (alloca == NULL)
            return llvm_expression_error(ctx, node,
                "LLVM party instance allocation failed");

        /* Zero-initialize */
        LLVMValueRef zero = LLVMConstNull(cls->struct_type);
        LLVMBuildStore(ctx->builder, zero, alloca);

        /* Store each assignment */
        for (size_t i = 0; i < node->data.party_instance.assignment_count; i++) {
            const char *slot_name = node->data.party_instance.assignments[i].slot_name;
            ASTNode *val_node = node->data.party_instance.assignments[i].value;
            bool found_field = false;

            /* Find field index */
            for (int f = 0; f < cls->field_count; f++) {
                if (strcmp(cls->fields[f].field_name, slot_name) == 0) {
                    found_field = true;
                    LLVMValueRef field_ptr = LLVMBuildStructGEP2(
                        ctx->builder, cls->struct_type, alloca,
                        (unsigned)cls->fields[f].index,
                        llvm_tmp_name(ctx));
                    LLVMValueRef val = llvm_emit_expression(val_node, ctx);
                    if (val == NULL)
                        return llvm_expression_error(ctx, node,
                            "LLVM party instance could not lower assigned value");
                    LLVMBuildStore(ctx->builder, val, field_ptr);
                    break;
                }
            }
            if (!found_field) {
                llvm_set_error_at_with_hints(ctx, node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "LLVM party instance field '%s' is not present in class metadata",
                    slot_name != NULL ? slot_name : "<unnamed>");
                return NULL;
            }
        }

        return LLVMBuildLoad2(ctx->builder, cls->struct_type,
            alloca, llvm_tmp_name(ctx));
    }

    case AST_TASK_GROUP: {
        /* TaskGroup must be consumed by the AIR/RIR/MIR boundary path. */
        return llvm_expression_error(ctx, node,
            "LLVM TaskGroup expression must lower through AIR/RIR/MIR task-group boundary, not expression fallback");
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
        return llvm_expression_error(ctx, node,
            "LLVM await expression requires an operand expression");
    case AST_LAMBDA_EXPR: {
        /* Generate a static LLVM function and return its pointer */
        int lid = ctx->lambda_counter++;
        int pc = (int)node->data.lambda_expr.param_count;
        if (pc > 8)
            return llvm_expression_error(ctx, node,
                "LLVM lambda expression supports at most 8 parameters");

        /* Determine return type */
        LLVMTypeRef ret_type = ctx->type_i32;
        if (node->data.lambda_expr.return_type != NULL) {
            ret_type = ast_type_to_llvm(ctx, node->data.lambda_expr.return_type);
            if (ctx->has_error || ret_type == NULL)
                return llvm_expression_error(ctx, node,
                    "LLVM lambda expression could not lower return type");
        } else if (node->data.lambda_expr.body != NULL
                 && node->data.lambda_expr.body->type == AST_BLOCK)
            ret_type = ctx->type_void;

        /* Parameter types (default i32) */
        LLVMTypeRef lparams[8];
        for (int j = 0; j < pc && j < 8; j++) {
            ASTNode *p = node->data.lambda_expr.params[j];
            if (p == NULL)
                return llvm_expression_error(ctx, node,
                    "LLVM lambda expression has a missing parameter");
            if (p->type == AST_LET_DECL && p->data.let_decl.type != NULL) {
                lparams[j] = ast_type_to_llvm(ctx, p->data.let_decl.type);
                if (ctx->has_error || lparams[j] == NULL)
                    return llvm_expression_error(ctx, node,
                        "LLVM lambda expression could not lower parameter type");
            } else {
                lparams[j] = ctx->type_i32;
            }
        }

        char lname[128];
        if (!llvm_expr_lambda_name(ctx, node, lname, sizeof(lname), lid))
            return NULL;
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
            const char *pname = NULL;
            if (p != NULL && p->type == AST_IDENTIFIER)
                pname = p->data.identifier.name;
            else if (p != NULL && p->type == AST_LET_DECL)
                pname = p->data.let_decl.name;
            if (pname == NULL || pname[0] == '\0') {
                llvm_expression_error(ctx, node,
                    "LLVM lambda expression requires named parameters");
                break;
            }
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
                if (ret_type != ctx->type_void && val != NULL)
                    LLVMBuildRet(ctx->builder, val);
                else if (ret_type == ctx->type_void)
                    LLVMBuildRetVoid(ctx->builder);
                else {
                    llvm_expression_error(ctx, node,
                        "LLVM lambda expression could not lower body expression");
                    LLVMValueRef zero = llvm_zero_value_for_type(ctx,
                        ret_type);
                    if (zero != NULL)
                        LLVMBuildRet(ctx->builder, zero);
                }
            }
        }

        /* Ensure terminator exists */
        if (LLVMGetBasicBlockTerminator(
                LLVMGetInsertBlock(ctx->builder)) == NULL) {
            if (ret_type == ctx->type_void)
                LLVMBuildRetVoid(ctx->builder);
            else {
                LLVMValueRef zero = llvm_zero_value_for_type(ctx,
                    ret_type);
                if (zero != NULL)
                    LLVMBuildRet(ctx->builder, zero);
            }
        }

        llvm_scope_pop(ctx);

        /* Restore builder state */
        ctx->current_function = saved_fn;
        ctx->current_ret_type = saved_ret;
        if (saved_bb != NULL)
            LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);

        return lfn;
    }

    case AST_EVENT_SUBSCRIBE: {
        /* event += handler → EventName_SUBSCRIBE(&event, handler) */
        return llvm_emit_event_subscribe_expr(node, ctx);
    }

    case AST_EVENT_UNSUBSCRIBE: {
        /* event -= handler → EventName_UNSUBSCRIBE(&event, handler) */
        return llvm_emit_event_unsubscribe_expr(node, ctx);
    }

    case AST_EVENT_INVOKE: {
        /* Emit(event, args...) → EventName_INVOKE(&event, args...) */
        return llvm_emit_event_invoke_expr(node, ctx);
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
        return NULL;

    default:
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM expression emitter has no lowering for AST node type %d; add an explicit lowering or route this declaration metadata through the domain/MIR emitter",
            (int)node->type);
        return NULL;
    }
}

#endif /* PGY_LLVM_ENABLED */
