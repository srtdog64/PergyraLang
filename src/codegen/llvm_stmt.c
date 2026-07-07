#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "llvm_domain_role_helpers.h"
#include "llvm_stmt_bind.h"
#include "llvm_stmt_emit_support.h"
#include "../compiler/mir_abi_layout.h"

/* =================================================================
 * Statement emission
 * ================================================================= */

ASTNode *
llvm_stmt_find_function_decl_by_name(LLVMGenCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;
    return llvm_find_function_decl(ctx, name);
}

static int
llvm_stmt_find_labeled_loop_depth(LLVMGenCtx *ctx, const char *label)
{
    if (ctx == NULL || label == NULL)
        return -1;

    for (int i = ctx->loop_depth - 1; i >= 0; i--) {
        if (ctx->loop_labels[i] != NULL
            && strcmp(ctx->loop_labels[i], label) == 0) {
            return i;
        }
    }

    return -1;
}

static void
llvm_emit_return_stmt(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *value = ast_return_value(node);

    llvm_emit_defers_from(ctx, 0);

    if (value != NULL) {
        LLVMTypeRef function_ret_type = ctx->current_function_ret_type;
        if (function_ret_type == NULL)
            function_ret_type = ctx->current_ret_type;
        if (function_ret_type == ctx->type_void) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM void function return must not carry a value expression");
            return;
        }
        if (!llvm_stmt_require_non_void_value(ctx, value,
                "LLVM return statement cannot consume a Void expression value")) {
            return;
        }
        const char *saved_expected_type_name = ctx->expected_type_name;
        ASTNode *saved_expected_callable_type = ctx->expected_callable_type;
        LLVMValueRef val;
        if (ctx->current_return_type_name != NULL
            || ctx->current_return_callable_type != NULL) {
            ctx->expected_type_name = ctx->current_return_type_name;
            ctx->expected_callable_type = ctx->current_return_callable_type;
        }
        val = llvm_emit_expression(value, ctx);
        ctx->expected_callable_type = saved_expected_callable_type;
        ctx->expected_type_name = saved_expected_type_name;
        if (val != NULL) {
            /* Coerce to expected return type */
            LLVMTypeRef val_type = LLVMTypeOf(val);
            LLVMTypeRef ret_type = function_ret_type;
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
            if (ctx != NULL && !ctx->has_error) {
                llvm_set_error_at_with_hints(ctx, value,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "LLVM return statement could not lower value expression");
            }
            if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
                LLVMBuildUnreachable(ctx->builder);
        }
    } else {
        LLVMTypeRef function_ret_type = ctx->current_function_ret_type;
        if (function_ret_type == NULL)
            function_ret_type = ctx->current_ret_type;
        if (function_ret_type == ctx->type_void)
            LLVMBuildRetVoid(ctx->builder);
        else {
            if (ctx != NULL && !ctx->has_error) {
                llvm_set_error_at_with_hints(ctx, node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_ADD_RETURN_ON_ALL_PATHS,
                    "LLVM non-Void return statement requires a value expression");
            }
            if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
                LLVMBuildUnreachable(ctx->builder);
        }
    }
}

static void
llvm_emit_if_stmt(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *condition = ast_if_condition(node);
    if (!llvm_stmt_require_non_void_value(ctx, condition,
            "LLVM if statement cannot consume a Void expression as condition")) {
        return;
    }
    LLVMValueRef cond = llvm_emit_expression(condition, ctx);
    if (cond == NULL) {
        if (ctx != NULL && !ctx->has_error) {
            llvm_set_error_at_with_hints(ctx, condition,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM if statement could not lower condition expression");
        }
        return;
    }

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
    if (ast_if_then_branch(node) != NULL)
        llvm_emit_statement(ast_if_then_branch(node), ctx);
    /* Only branch to merge if no terminator (return) was emitted */
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, merge_bb);

    /* Else block */
    LLVMPositionBuilderAtEnd(ctx->builder, else_bb);
    if (ast_if_else_branch(node) != NULL)
        llvm_emit_statement(ast_if_else_branch(node), ctx);
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, merge_bb);

    /* Merge */
    LLVMPositionBuilderAtEnd(ctx->builder, merge_bb);
}

void
llvm_emit_block(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node == NULL || ctx->has_error)
        return;

    if (node->type != AST_BLOCK)
        return;

    LLVMLexicalRegistrySnapshot lexical_snapshot =
        llvm_lexical_registry_snapshot(ctx);
    llvm_defer_scope_push(ctx);
    llvm_scope_push(ctx);
    for (size_t i = 0; i < ast_block_statement_count(node); i++) {
        llvm_emit_statement(ast_block_statement(node, i), ctx);
        /* Stop emitting after a terminator (return) */
        if (LLVMGetBasicBlockTerminator(
                LLVMGetInsertBlock(ctx->builder)) != NULL)
            break;
    }

    /* Slot sugar: auto-release slot vars declared in this scope (LIFO).
     * Skip slots already explicitly released by the user. */
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL) {
        llvm_emit_defers_from(ctx, ctx->defer_scope_depth - 1);
        for (int i = ctx->slot_var_count - 1;
             i >= lexical_snapshot.slot_var_count;
             i--) {
            if (ctx->slot_vars[i].released) continue;
            const char *inner = ctx->slot_vars[i].inner_type;
            const char *vname = ctx->slot_vars[i].var_name;
            bool is_secure = ctx->slot_vars[i].is_secure;
            const char *runtime_fn = mir_abi_resource_runtime_fn_by_kind(
                is_secure ? MIR_RESOURCE_ABI_SECURE_SLOT
                          : MIR_RESOURCE_ABI_SLOT,
                inner, "Release");
            LLVMFuncEntry *fn = runtime_fn != NULL
                ? llvm_lookup_function(ctx, runtime_fn)
                : NULL;
            LLVMVarEntry var;
            if (!llvm_scope_lookup_snapshot(ctx, vname, &var)
                || var.alloca != ctx->slot_vars[i].binding)
                continue;
            if (fn != NULL) {
                if (is_secure) {
                    LLVMVarEntry token_var;
                    if (llvm_lookup_secure_token_var(ctx, vname, &token_var)) {
                        LLVMValueRef args[] = { var.alloca, token_var.alloca };
                        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
                    } else {
                        llvm_set_error_at_with_hints(ctx, node,
                            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                            PGY_FIX_INSPECT_MIR_INVENTORY,
                            "LLVM secure slot auto-release requires paired token binding '%s_token'",
                            vname != NULL ? vname : "<slot>");
                        break;
                    }
                } else {
                    LLVMValueRef args[] = { var.alloca };
                    LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
                }
            } else if (pgy_classify_type(inner) != PGY_TK_UNKNOWN) {
                if (runtime_fn != NULL) {
                    llvm_required_runtime_function(ctx, node,
                        is_secure ? "secure slot" : "slot",
                        "auto-release", runtime_fn);
                } else {
                    llvm_set_error_at_with_hints(ctx, node,
                        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                        PGY_FIX_INSPECT_MIR_INVENTORY,
                        "LLVM auto-release requires MIR ABI runtime function row");
                }
                break;
            } else if (is_secure) {
                LLVMValueRef occ_ptr = LLVMBuildStructGEP2(ctx->builder,
                    var.type, var.alloca, 1, llvm_tmp_name(ctx));
                LLVMValueRef token_ptr = LLVMBuildStructGEP2(ctx->builder,
                    var.type, var.alloca, 2, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 0, 0),
                    occ_ptr);
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(ctx->type_i64, 0, 0), token_ptr);
            } else {
                LLVMValueRef occ_ptr = LLVMBuildStructGEP2(ctx->builder,
                    var.type, var.alloca, 1, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 0, 0),
                    occ_ptr);
            }
            ctx->slot_vars[i].released = true;
        }
    }

    llvm_scope_pop(ctx);
    llvm_lexical_registry_restore(ctx, lexical_snapshot);
    llvm_defer_scope_pop(ctx);
}

bool
llvm_emit_bind_statement_parts(LLVMGenCtx *ctx, const char *party_var,
                               const char *slot_name, const char *role_name,
                               ASTNode *diagnostic_node)
{
    LLVMVarEntry party_entry;
    const char *party_class_name;
    LLVMClassTypeEntry *cls;
    char vt_field[256];
    int field_idx;
    const char *ability_tag;
    LLVMValueRef vt_global;
    LLVMValueRef field_ptr;
    LLVMValueRef vt_ptr;

    if (ctx == NULL)
        return false;
    if (party_var == NULL || slot_name == NULL || role_name == NULL) {
        llvm_set_error_at_with_hints(ctx, diagnostic_node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ALIGN_ROLE_IMPL_WITH_ABILITY,
            "LLVM bind emission requires party variable, slot name, and role name");
        return false;
    }

    if (!llvm_scope_lookup_snapshot(ctx, party_var, &party_entry)) {
        llvm_set_error_at_with_hints(ctx, diagnostic_node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ALIGN_ROLE_IMPL_WITH_ABILITY,
            "LLVM bind emission cannot resolve party variable '%s'",
            party_var);
        return false;
    }

    party_class_name = llvm_lookup_var_class(ctx, party_var);
    cls = party_class_name ? llvm_lookup_class(ctx, party_class_name) : NULL;
    if (cls == NULL) {
        llvm_set_error_at_with_hints(ctx, diagnostic_node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ALIGN_ROLE_IMPL_WITH_ABILITY,
            "LLVM bind emission cannot resolve party type for '%s'",
            party_var);
        return false;
    }

    if (!llvm_stmt_format_bind_name(ctx, diagnostic_node, vt_field,
            sizeof(vt_field), slot_name, "_vtable", "vtable field")) {
        return false;
    }
    field_idx = llvm_class_field_index(cls, vt_field);
    if (field_idx < 0) {
        llvm_set_error_at_with_hints(ctx, diagnostic_node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ALIGN_ROLE_IMPL_WITH_ABILITY,
            "LLVM bind emission cannot resolve vtable field '%s'",
            vt_field);
        return false;
    }

    ability_tag = llvm_party_slot_first_ability_tag(ctx, party_class_name,
                                                    slot_name);
    if (ability_tag == NULL) {
        llvm_set_error_at_with_hints(ctx, diagnostic_node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ALIGN_ROLE_IMPL_WITH_ABILITY,
            "LLVM bind emission cannot resolve required ability for party slot '%s.%s'",
            party_class_name, slot_name);
        return false;
    }

    vt_global = llvm_lookup_role_vtable_global(ctx, role_name, ability_tag);
    if (vt_global == NULL) {
        llvm_set_error_at_with_hints(ctx, diagnostic_node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ALIGN_ROLE_IMPL_WITH_ABILITY,
            "LLVM bind emission cannot resolve role vtable global for '%s.%s'",
            role_name, ability_tag);
        return false;
    }

    field_ptr = LLVMBuildStructGEP2(ctx->builder, cls->struct_type,
        party_entry.alloca, (unsigned)field_idx, llvm_tmp_name(ctx));
    vt_ptr = LLVMBuildBitCast(ctx->builder, vt_global, ctx->type_i8ptr,
                              llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, vt_ptr, field_ptr);
    return true;
}

/* Parallel / async / select statement owners live in llvm_stmt_parallel_async.c. */

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

    case AST_LET_DESTRUCTURE:
        llvm_emit_let_destructure_stmt(node, ctx);
        break;

    case AST_RETURN:
        llvm_emit_return_stmt(node, ctx);
        break;

    case AST_BREAK:
        if (ctx->loop_depth > 0) {
            int target_depth = ctx->loop_depth - 1;
            if (ast_break_label(node) != NULL) {
                int found = llvm_stmt_find_labeled_loop_depth(
                    ctx, ast_break_label(node));
                if (found >= 0)
                    target_depth = found;
            }
            llvm_emit_defers_from(ctx,
                ctx->loop_defer_base_depth[target_depth]);
            LLVMBuildBr(ctx->builder,
                ctx->loop_break_blocks[target_depth]);
        }
        break;
    case AST_ENUM_DECL:
        /* Enums are compile-time only; no IR needed. */
        break;
    case AST_CONTINUE:
        if (ctx->loop_depth > 0) {
            int target_depth = ctx->loop_depth - 1;
            if (ast_continue_label(node) != NULL) {
                int found = llvm_stmt_find_labeled_loop_depth(
                    ctx, ast_continue_label(node));
                if (found >= 0)
                    target_depth = found;
            }
            llvm_emit_defers_from(ctx,
                ctx->loop_defer_base_depth[target_depth]);
            LLVMBuildBr(ctx->builder,
                ctx->loop_continue_blocks[target_depth]);
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
    case AST_ABILITY_DECL:
    case AST_ROLE_DECL:
    case AST_PARTY_DECL:
    case AST_ROSTER_DECL:
    case AST_WORLD_DECL:
    case AST_RELATION_DECL:
    case AST_EFFECT_DECL:
    case AST_ZONE_DECL:
    case AST_EVENT_DECL:
    case AST_INTENT_DECL:
    case AST_IMPORT_DECL:
    case AST_NAMESPACE_DECL:
    case AST_TYPE_ALIAS:
    case AST_LIFECYCLE_DECL:
    case AST_USE_DECL:
    case AST_INCLUDE_STMT:
    case AST_IMPL_ABILITY:
        /* Category 3 (top-level declarations) skip list.
         * See docs/95_ast_dispatch_partition.md for the partition model.
         *
         * These are consumed by the program pass (MIR-backed emitters in
         * llvm_pipeline.c / llvm_domain.c / llvm_intent.c / llvm_register.c).
         * Under current MIR-based emission they never reach this switch, but
         * the list absorbs any future dispatcher change that accidentally
         * routes a top-level decl into function-body statement context. */
        break;

    case AST_EXTERN_BLOCK:
        /* extern "C" { func ...; } is handled in program pass (Pass 0). */
        break;

    case AST_UNSAFE_BLOCK:
        /* unsafe { ... } is a lexical boundary; emit the body directly. */
        if (ast_unsafe_block_body(node) != NULL)
            llvm_emit_block(ast_unsafe_block_body(node), ctx);
        break;

    case AST_TRANSACTION_BLOCK: {
        /* Saga CFG: run the body; a `fail` stores the failed flag and branches
         * to the epilogue; the epilogue runs registered compensations in reverse
         * when the flag is set; control then continues. Same run -> on-fail ->
         * reverse-compensate -> exit shape as the intent saga. */
        LLVMValueRef fn = ctx->current_function;
        int txn_id = ctx->txn_counter++;
        char end_name[32], comp_name[32], cont_name[32];
        LLVMValueRef saved_flag = ctx->current_txn_failed_flag;
        LLVMBasicBlockRef saved_end = ctx->current_txn_end_bb;
        size_t comp_count = node->data.transaction_block.compensation_count;
        size_t i;

        snprintf(end_name, sizeof(end_name), "txn.end.%d", txn_id);
        snprintf(comp_name, sizeof(comp_name), "txn.comp.%d", txn_id);
        snprintf(cont_name, sizeof(cont_name), "txn.cont.%d", txn_id);

        LLVMValueRef failed_flag =
            LLVMBuildAlloca(ctx->builder, ctx->type_i1, "__txn_failed");
        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), failed_flag);

        LLVMBasicBlockRef end_bb =
            LLVMAppendBasicBlockInContext(ctx->context, fn, end_name);
        LLVMBasicBlockRef comp_bb =
            LLVMAppendBasicBlockInContext(ctx->context, fn, comp_name);
        LLVMBasicBlockRef cont_bb =
            LLVMAppendBasicBlockInContext(ctx->context, fn, cont_name);

        ctx->current_txn_failed_flag = failed_flag;
        ctx->current_txn_end_bb = end_bb;
        if (ast_transaction_block_body(node) != NULL)
            llvm_emit_block(ast_transaction_block_body(node), ctx);
        ctx->current_txn_failed_flag = saved_flag;
        ctx->current_txn_end_bb = saved_end;

        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
            LLVMBuildBr(ctx->builder, end_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, end_bb);
        LLVMValueRef f =
            LLVMBuildLoad2(ctx->builder, ctx->type_i1, failed_flag, "__txn_f");
        LLVMBuildCondBr(ctx->builder, f, comp_bb, cont_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, comp_bb);
        for (i = comp_count; i > 0; i--)
            (void)llvm_emit_expression(
                node->data.transaction_block.compensations[i - 1], ctx);
        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
            LLVMBuildBr(ctx->builder, cont_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, cont_bb);
        break;
    }

    case AST_FAIL_STMT:
        /* Roll back the innermost transaction: set its failed flag and branch to
         * its epilogue, then park the builder on a fresh (dead) block so any
         * trailing statements still have a valid insertion point. Outside a
         * transaction this is a no-op (semantic scope validation is a later step). */
        if (ctx->current_txn_failed_flag != NULL && ctx->current_txn_end_bb != NULL) {
            ASTNode *fail_reason = ast_fail_stmt_reason(node);
            if (fail_reason != NULL)
                (void)llvm_emit_expression(fail_reason, ctx);
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0),
                           ctx->current_txn_failed_flag);
            LLVMBuildBr(ctx->builder, ctx->current_txn_end_bb);
            LLVMPositionBuilderAtEnd(ctx->builder,
                LLVMAppendBasicBlockInContext(ctx->context,
                    ctx->current_function, "txn.afterfail"));
        }
        break;

    case AST_DEFER_STMT:
        if (ast_defer_body(node) != NULL)
            llvm_register_defer(ast_defer_body(node), ctx);
        break;

    case AST_BIND_STMT: {
        (void)llvm_emit_bind_statement_parts(ctx,
            ast_bind_statement_party_var(node),
            ast_bind_statement_slot_name(node),
            ast_bind_statement_role_name(node),
            node);
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
        /* Category 2 (decl sub-metadata) safety-net forward.
         * See docs/95_ast_dispatch_partition.md for the partition model.
         *
         * These domain runtime verbs are created only as sub-nodes of
         * world_decl / zone_decl in parser_domain.c, and llvm_domain.c
         * consumes those arrays directly with dedicated handlers.  Under
         * the current parser they never reach this switch, but the
         * verbs' identifier semantics ("activate", "apply", "link") make
         * future function-body use plausible, so the forward routes
         * any accidental arrival into llvm_emit_expression, where
         * PGY_CODE_LLVM_TYPE_UNSUPPORTED surfaces the breakage with an
         * actionable diagnostic instead of a silent no-op. */
        llvm_emit_expression(node, ctx);
        if (node->type == AST_CALL)
            llvm_stmt_emit_zone_action_effect_runtime(node, ctx);
        break;

    default:
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM statement emitter has no lowering for AST node type %d; add an explicit lowering or keep this node in declaration/domain metadata",
            (int)node->type);
        break;
    }
}

#endif /* PGY_LLVM_ENABLED */
