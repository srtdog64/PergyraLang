/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend expression emission.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

LLVMValueRef llvm_emit_expression(ASTNode *node, LLVMGenCtx *ctx);
#include "llvm_expr_emit_support.h"
#include "llvm_expr_aggregate.h"
#include "llvm_expr_member_lvalue.h"
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
    case AST_TUPLE_LITERAL: return llvm_emit_tuple_literal_expr(node, ctx);
    case AST_ARRAY_LITERAL: return llvm_emit_array_literal_expr(node, ctx);
    case AST_MAP_LITERAL:   return llvm_emit_map_literal_expr(node, ctx);
    case AST_CAST:          return llvm_emit_cast_expr(node, ctx);
    case AST_ARRAY_ACCESS:  return llvm_emit_array_access_expr(node, ctx);

    case AST_CONTEXT_ACCESS: {
        LLVMVarEntry self_var;
        if (!llvm_scope_lookup_snapshot(ctx, "self", &self_var))
            return llvm_expression_error(ctx, node,
                "LLVM context access requires a registered self parameter");

        LLVMValueRef self_val = LLVMBuildLoad2(ctx->builder,
            ctx->type_i8ptr, self_var.alloca, llvm_tmp_name(ctx));
        return self_val;
    }

    case AST_PARTY_INSTANCE: {
        const char *pty = ast_party_instance_party_type(node);
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, pty);
        if (cls == NULL)
            return llvm_expression_error(ctx, node,
                "LLVM party instance requires registered class metadata");

        LLVMValueRef alloca = llvm_create_entry_alloca(ctx,
            cls->struct_type, llvm_tmp_name(ctx));
        if (alloca == NULL)
            return llvm_expression_error(ctx, node,
                "LLVM party instance allocation failed");

        LLVMValueRef zero = LLVMConstNull(cls->struct_type);
        LLVMBuildStore(ctx->builder, zero, alloca);

        for (size_t i = 0; i < ast_party_instance_assignment_count(node); i++) {
            const char *slot_name =
                ast_party_instance_assignment_slot_name(node, i);
            ASTNode *val_node =
                ast_party_instance_assignment_value(node, i);
            int field_index = llvm_class_field_index(cls, slot_name);

            if (field_index < 0) {
                llvm_set_error_at_with_hints(ctx, node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "LLVM party instance field '%s' is not present in class metadata",
                    slot_name != NULL ? slot_name : "<unnamed>");
                return NULL;
            }
            LLVMValueRef field_ptr = LLVMBuildStructGEP2(
                ctx->builder, cls->struct_type, alloca,
                (unsigned)field_index, llvm_tmp_name(ctx));
            LLVMValueRef val = llvm_emit_expression(val_node, ctx);
            if (val == NULL)
                return llvm_expression_error(ctx, node,
                    "LLVM party instance could not lower assigned value");
            LLVMBuildStore(ctx->builder, val, field_ptr);
        }

        return LLVMBuildLoad2(ctx->builder, cls->struct_type,
            alloca, llvm_tmp_name(ctx));
    }

    case AST_TASK_GROUP:
        return llvm_expression_error(ctx, node,
            "LLVM TaskGroup expression must lower through AIR/RIR/MIR task-group boundary, not expression fallback");

    case AST_CHANNEL_SEND:
        return llvm_emit_channel_send_expr(node, ctx);

    case AST_CHANNEL_RECV:
        return llvm_emit_channel_recv_expr(node, ctx);

    case AST_SPAWN_EXPR:
        return llvm_emit_spawn_expr(node, ctx);

    case AST_AWAIT_EXPR:
        if (ast_await_expression(node) != NULL) {
            ASTNode *inner_expr = ast_await_expression(node);
            const char *future_name = NULL;
            const char *inner = NULL;
            bool is_remote = false;

            if (inner_expr->type == AST_SPAWN_EXPR) {
                inner = llvm_infer_spawn_future_inner(ctx, inner_expr);
                if (inner == NULL || inner[0] == '\0') {
                    llvm_set_error_at_with_hints(ctx, node,
                        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                        "LLVM await spawn expression requires concrete Future<T> result metadata");
                    return NULL;
                }
                LLVMValueRef task = llvm_emit_spawn_expr(inner_expr, ctx);
                return llvm_await_task_handle(ctx, node, task, inner, false);
            }

            if (inner_expr->type != AST_IDENTIFIER) {
                llvm_set_error_at_with_hints(ctx, node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "LLVM await expression requires a named Future<T> operand");
                return NULL;
            }
            future_name = ast_identifier_name(inner_expr);
            inner = llvm_lookup_future_inner(ctx, future_name);
            if (inner == NULL || inner[0] == '\0') {
                llvm_set_error_at_with_hints(ctx, node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "LLVM await expression requires registered Future<T> result metadata");
                return NULL;
            }
            is_remote = llvm_lookup_future_is_remote(ctx, future_name);
            {
                LLVMValueRef task = llvm_emit_expression(inner_expr, ctx);
                return llvm_await_task_handle(ctx, node, task, inner, is_remote);
            }
        }
        return llvm_expression_error(ctx, node,
            "LLVM await expression requires an operand expression");

    case AST_LAMBDA_EXPR: {
        int lid = ctx->lambda_counter++;
        int pc = (int)ast_lambda_param_count(node);
        ASTNode *lambda_body = ast_lambda_body(node);
        if (pc > 8)
            return llvm_expression_error(ctx, node,
                "LLVM lambda expression supports at most 8 parameters");

        LLVMTypeRef ret_type = llvm_stmt_lambda_return_type(ctx, node);
        if (ctx->has_error || ret_type == NULL)
            return llvm_expression_error(ctx, node,
                "LLVM lambda expression could not lower return type");

        LLVMTypeRef lparams[8];
        for (int j = 0; j < pc && j < 8; j++) {
            ASTNode *p = ast_lambda_param(node, (size_t)j);
            if (p == NULL)
                return llvm_expression_error(ctx, node,
                    "LLVM lambda expression has a missing parameter");
            lparams[j] = llvm_stmt_lambda_param_type(ctx, node, p, (size_t)j);
            if (ctx->has_error || lparams[j] == NULL)
                return llvm_expression_error(ctx, node,
                    "LLVM lambda expression could not lower parameter type");
        }

        char lname[128];
        if (!llvm_expr_lambda_name(ctx, node, lname, sizeof(lname), lid))
            return NULL;
        LLVMTypeRef lft = LLVMFunctionType(ret_type,
            lparams, (unsigned)pc, 0);
        LLVMValueRef lfn = LLVMAddFunction(ctx->module, lname, lft);
        llvm_register_function(ctx, LLVMGetValueName(lfn),
            lfn, lft, ret_type);

        LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(ctx->builder);
        LLVMValueRef saved_fn = ctx->current_function;
        LLVMTypeRef saved_ret = ctx->current_ret_type;
        LLVMTypeRef saved_function_ret = ctx->current_function_ret_type;
        const char *saved_return_type_name = ctx->current_return_type_name;
        ASTNode *saved_return_callable_type =
            ctx->current_return_callable_type;
        LLVMLexicalRegistrySnapshot lexical_snapshot =
            llvm_lexical_registry_snapshot(ctx);

        ctx->current_function = lfn;
        ctx->current_ret_type = ret_type;
        ctx->current_function_ret_type = ret_type;
        {
            ASTNode *lambda_return_type = ast_lambda_return_type(node);
            ctx->current_return_type_name =
                lambda_return_type != NULL
                    ? llvm_stmt_render_type_annotation_copy(ctx,
                        lambda_return_type)
                    : NULL;
            ctx->current_return_callable_type =
                lambda_return_type != NULL
                && lambda_return_type->type == AST_EVENT_HANDLER_TYPE
                    ? lambda_return_type
                    : NULL;
        }

        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(
            ctx->context, lfn, "entry");
        LLVMPositionBuilderAtEnd(ctx->builder, entry);

        llvm_scope_push(ctx);
        for (int j = 0; j < pc; j++) {
            ASTNode *p = ast_lambda_param(node, (size_t)j);
            const char *pname = NULL;
            if (p != NULL && p->type == AST_IDENTIFIER)
                pname = ast_identifier_name(p);
            else if (p != NULL && p->type == AST_LET_DECL)
                pname = ast_let_name(p);
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

        if (lambda_body != NULL) {
            if (lambda_body->type == AST_BLOCK) {
                llvm_emit_block(lambda_body, ctx);
            } else {
                LLVMValueRef val = llvm_emit_expression(lambda_body, ctx);
                if (ret_type != ctx->type_void && val != NULL)
                    LLVMBuildRet(ctx->builder, val);
                else if (ret_type == ctx->type_void)
                    LLVMBuildRetVoid(ctx->builder);
                else {
                    llvm_expression_error(ctx, node,
                        "LLVM lambda expression could not lower body expression");
                    LLVMValueRef zero = llvm_zero_value_for_type(ctx, ret_type);
                    if (zero != NULL)
                        LLVMBuildRet(ctx->builder, zero);
                }
            }
        }

        if (LLVMGetBasicBlockTerminator(
                LLVMGetInsertBlock(ctx->builder)) == NULL) {
            if (ret_type == ctx->type_void)
                LLVMBuildRetVoid(ctx->builder);
            else {
                LLVMValueRef zero = llvm_zero_value_for_type(ctx, ret_type);
                if (zero != NULL)
                    LLVMBuildRet(ctx->builder, zero);
            }
        }

        llvm_scope_pop(ctx);
        llvm_lexical_registry_restore(ctx, lexical_snapshot);

        ctx->current_function = saved_fn;
        ctx->current_ret_type = saved_ret;
        ctx->current_function_ret_type = saved_function_ret;
        ctx->current_return_type_name = saved_return_type_name;
        ctx->current_return_callable_type = saved_return_callable_type;
        if (saved_bb != NULL)
            LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);

        return lfn;
    }

    case AST_EVENT_SUBSCRIBE:
        return llvm_emit_event_subscribe_expr(node, ctx);

    case AST_EVENT_UNSUBSCRIBE:
        return llvm_emit_event_unsubscribe_expr(node, ctx);

    case AST_EVENT_INVOKE:
        return llvm_emit_event_invoke_expr(node, ctx);

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
