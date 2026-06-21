#include "transpiler_statement_dispatch.h"

#include <stdlib.h>
#include <stdio.h>

#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "../semantic/lifecycle_state.h"

#include "transpiler_context.h"
#include "transpiler_control_flow_emit.h"
#include "transpiler_defer_emit.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_destructure_emit.h"
#include "transpiler_domain_ability_emit.h"
#include "transpiler_enum_decl_emit.h"
#include "transpiler_event_emit.h"
#include "transpiler_match_emit.h"
#include "transpiler_mir_ssa_names.h"
#include "transpiler_projection_sync.h"
#include "transpiler_relation_effect_emit.h"
#include "transpiler_role_ability_helpers.h"
#include "transpiler_symbols.h"
#include "transpiler_type_alias.h"
#include "transpiler_zone_decl_emit.h"

bool
transpiler_emit_bind_statement_parts(TranspilerCtx *ctx,
                                     const char *pvar,
                                     const char *slot_name,
                                     const char *role_name)
{
    const char *party_type;
    const char *pvar_ssa;
    char *pvar_c_owned;
    const char *pvar_c;
    const char *ability_name = NULL;
    char *ability_tag = NULL;
    ASTNode *party_decl;

    if (ctx == NULL)
        return false;
    party_type = lookup_typed_var(ctx, pvar);
    pvar_ssa = transpiler_resolve_active_ssa_name(ctx, pvar);
    pvar_c_owned = pvar_ssa != NULL
        ? transpiler_make_c_ssa_name(ctx, pvar_ssa)
        : NULL;
    pvar_c = pvar_c_owned != NULL ? pvar_c_owned : pvar;
    if (pvar_c == NULL)
        pvar_c = pvar;
    if (party_type == NULL) {
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "cannot resolve party type for bind statement '%s.%s = %s'",
            pvar != NULL ? pvar : "<party>",
            slot_name != NULL ? slot_name : "<slot>",
            role_name != NULL ? role_name : "<role>");
        free(pvar_c_owned);
        return false;
    }

    party_decl = find_party_decl(ctx, party_type);
    if (party_decl == NULL) {
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "cannot resolve party declaration '%s' while emitting bind statement '%s.%s = %s'",
            party_type,
            pvar != NULL ? pvar : "<party>",
            slot_name != NULL ? slot_name : "<slot>",
            role_name != NULL ? role_name : "<role>");
        free(pvar_c_owned);
        return false;
    }
    ability_tag = transpiler_party_slot_first_ability_tag(ctx, party_decl,
                                                          slot_name);
    ability_name = ability_tag;
    if (ability_name == NULL) {
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "cannot resolve required ability tag for party slot '%s.%s' while emitting bind statement",
            party_type,
            slot_name != NULL ? slot_name : "<slot>");
        free(ability_tag);
        free(pvar_c_owned);
        return false;
    }
    write_indent(ctx);
    codebuf_write(ctx->out,
        "%s_bind_%s(&%s, NULL, &%s_%s_vtable_instance);\n",
        party_type, slot_name, pvar_c, role_name, ability_name);
    free(ability_tag);
    free(pvar_c_owned);
    return true;
}

/* Emit the domain-lifecycle runtime guard the semantic pass annotated onto this
 * `v.Op()` call (doc/12 section 2.3). LC_GUARD_CHECK is the fail-closed guard for
 * an ambiguous state; LC_GUARD_SET records a proven transition so a later
 * ambiguous guard sees the right state. The receiver is re-emitted through
 * emit_expression so it resolves to the same (SSA-mapped) lvalue the call uses,
 * keeping the C and LLVM lowerings reading the identical AST-node annotation.
 * Construction state defaults to the initial index (absent == state 0) in the
 * runtime side-map, so no separate init is emitted here. */
static void
emit_lifecycle_guard_for_call(ASTNode *node, TranspilerCtx *ctx)
{
    const LcGuardSite *g = lc_guard_find(node);
    ASTNode *callee;
    ASTNode *obj;
    char    *recv;

    if (g == NULL || node->type != AST_CALL)
        return;
    callee = ast_call_callee(node);
    obj = callee != NULL ? ast_member_object(callee) : NULL;
    if (obj == NULL || obj->type != AST_IDENTIFIER)
        return;
    recv = emit_expression(obj, ctx);
    if (recv == NULL || recv[0] == '\0') {
        free(recv);
        return;
    }
    write_indent(ctx);
    if (g->kind == LC_GUARD_CHECK)
        codebuf_write(ctx->out,
            "pgy_runtime_lifecycle_guard_export(&(%s), %uu, %d, \"%s\", \"%s\");\n",
            recv, (unsigned)g->valid_mask, g->to_state, g->op, g->subject);
    else
        codebuf_write(ctx->out,
            "pgy_runtime_lifecycle_set_export(&(%s), %d);\n",
            recv, g->to_state);
    free(recv);
}

void
emit_statement(ASTNode *node, TranspilerCtx *ctx)
{
    if (node == NULL)
        return;

    emit_lifecycle_guard_for_call(node, ctx);

    switch (node->type) {
    case AST_LET_DECL:
        emit_let_decl(node, ctx);
        break;
    case AST_TYPE_ALIAS:
        emit_type_alias_decl(node, ctx);
        break;
    case AST_LIFECYCLE_DECL:
        /* Consumed by semantic lifecycle analysis; no C statement emission. */
        break;
    case AST_LET_DESTRUCTURE:
        emit_let_destructure_statement(node, ctx);
        break;
    case AST_FUNC_DECL:
        emit_func_decl(node, ctx);
        break;
    case AST_CLASS_DECL:
        emit_class_decl(node, ctx);
        break;
    case AST_EXTERN_BLOCK:
        emit_extern_block(node, ctx);
        break;
    case AST_IMPORT_DECL:
        /* Import is resolved at driver level (AST merging).
         * Nothing to emit; the imported declarations are
         * already present in the merged AST. */
        break;
    case AST_USE_DECL:
        /* use module; standard library modules.
         * Runtime functions are always available via pgy_runtime.h.
         * Future: emit module-specific includes/initializers here. */
        codebuf_write(ctx->out, "/* use %s */\n",
            ast_use_module_name(node) != NULL
                ? ast_use_module_name(node) : "unknown");
        break;
    case AST_UNSAFE_BLOCK:
        /* unsafe { ... } is a lexical boundary; emit the body directly. */
        write_indent(ctx);
        codebuf_write(ctx->out, "/* unsafe */\n");
        if (ast_unsafe_block_body(node) != NULL)
            emit_block(ast_unsafe_block_body(node), ctx);
        break;
    case AST_TRANSACTION_BLOCK: {
        /* Saga lowering: run the body; a `fail` sets the failed flag and jumps
         * to the epilogue; registered compensations then run in reverse; control
         * continues past the transaction either way. This mirrors the intent
         * saga shape (run -> on-fail -> reverse-compensate -> exit). */
        int txn_id = ctx->txn_counter++;
        int saved_txn = ctx->current_txn_id;
        size_t comp_count = node->data.transaction_block.compensation_count;
        size_t i;

        write_indent(ctx);
        codebuf_write(ctx->out, "{\n");
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "int __txn_failed_%d = 0;\n", txn_id);

        ctx->current_txn_id = txn_id;
        if (ast_transaction_block_body(node) != NULL)
            emit_block(ast_transaction_block_body(node), ctx);
        ctx->current_txn_id = saved_txn;

        write_indent(ctx);
        codebuf_write(ctx->out, "goto __txn_end_%d;\n", txn_id);
        write_indent(ctx);
        codebuf_write(ctx->out, "__txn_end_%d:\n", txn_id);
        write_indent(ctx);
        codebuf_write(ctx->out, "if (__txn_failed_%d) {\n", txn_id);
        ctx->indent++;
        for (i = comp_count; i > 0; i--) {
            char *comp = emit_expression(
                node->data.transaction_block.compensations[i - 1], ctx);
            if (comp != NULL && comp[0] != '\0') {
                write_indent(ctx);
                codebuf_write(ctx->out, "%s;\n", comp);
            }
            free(comp);
        }
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
        break;
    }
    case AST_FAIL_STMT:
        /* Trigger the innermost transaction's rollback epilogue. Outside any
         * transaction this is a no-op marker (semantic scope validation, which
         * would reject it, is a later step). */
        if (ctx->current_txn_id >= 0) {
            ASTNode *fail_reason = ast_fail_stmt_reason(node);
            if (fail_reason != NULL) {
                char *reason = emit_expression(fail_reason, ctx);
                if (reason != NULL && reason[0] != '\0') {
                    write_indent(ctx);
                    codebuf_write(ctx->out, "(void)(%s);\n", reason);
                }
                free(reason);
            }
            write_indent(ctx);
            codebuf_write(ctx->out, "__txn_failed_%d = 1;\n", ctx->current_txn_id);
            write_indent(ctx);
            codebuf_write(ctx->out, "goto __txn_end_%d;\n", ctx->current_txn_id);
        } else {
            write_indent(ctx);
            codebuf_write(ctx->out, "/* fail: no enclosing transaction */\n");
        }
        break;
    case AST_DEFER_STMT: {
        transpiler_register_defer(ast_defer_body(node), ctx);
        break;
    }
    case AST_BIND_STMT: {
        (void)transpiler_emit_bind_statement_parts(ctx,
            ast_bind_statement_party_var(node),
            ast_bind_statement_slot_name(node),
            ast_bind_statement_role_name(node));
        break;
    }
    case AST_ABILITY_DECL:
        emit_ability_decl(node, ctx);
        break;
    case AST_ROLE_DECL:
        emit_role_decl(node, ctx);
        break;
    case AST_PARTY_DECL:
        emit_party_decl(node, ctx);
        break;
    case AST_ROSTER_DECL:
        emit_roster_decl(node, ctx);
        break;
    case AST_WORLD_DECL:
        emit_world_decl(node, ctx);
        break;
    case AST_RELATION_DECL:
        emit_relation_decl(node, ctx);
        break;
    case AST_EFFECT_DECL:
        emit_effect_decl(node, ctx);
        break;
    case AST_ZONE_DECL:
        emit_zone_decl(node, ctx);
        break;
    case AST_EVENT_DECL:
        emit_event_decl(node, ctx);
        break;
    case AST_EVENT_SUBSCRIBE:
        emit_event_subscribe(node, ctx);
        break;
    case AST_EVENT_UNSUBSCRIBE:
        emit_event_unsubscribe(node, ctx);
        break;
    case AST_IF_STMT:
        emit_if_stmt(node, ctx);
        break;
    case AST_FOR_LOOP:
        emit_for_loop(node, ctx);
        break;
    case AST_WHILE_LOOP:
        emit_while_loop(node, ctx);
        break;
    case AST_MATCH_STMT:
        emit_match_stmt(node, ctx);
        break;
    case AST_RETURN:
        emit_return_stmt(node, ctx);
        break;
    case AST_BREAK:
        if (ast_break_label(node) != NULL) {
            int target = transpiler_find_loop_label_depth(
                ctx, ast_break_label(node));
            if (target >= 0) {
                ctx->loop_break_label_used[target] = true;
                transpiler_emit_defers_from(ctx,
                    ctx->loop_defer_base_depth[target]);
                write_indent(ctx);
                codebuf_write(ctx->out, "goto %s;\n",
                    ctx->loop_break_labels[target]);
            } else {
                write_indent(ctx);
                codebuf_write(ctx->out, "break;\n");
            }
        } else {
            if (ctx->loop_depth > 0) {
                int target = ctx->loop_depth - 1;
                transpiler_emit_defers_from(ctx,
                    ctx->loop_defer_base_depth[target]);
            }
            write_indent(ctx);
            codebuf_write(ctx->out, "break;\n");
        }
        break;
    case AST_ENUM_DECL:
        emit_enum_decl_stmt(node, ctx);
        break;
    case AST_CONTINUE:
        if (ast_continue_label(node) != NULL) {
            int target = transpiler_find_loop_label_depth(
                ctx, ast_continue_label(node));
            if (target >= 0) {
                ctx->loop_continue_label_used[target] = true;
                transpiler_emit_defers_from(ctx,
                    ctx->loop_defer_base_depth[target]);
                write_indent(ctx);
                codebuf_write(ctx->out, "goto %s;\n",
                    ctx->loop_continue_labels[target]);
            } else {
                write_indent(ctx);
                codebuf_write(ctx->out, "continue;\n");
            }
        } else {
            if (ctx->loop_depth > 0) {
                int target = ctx->loop_depth - 1;
                transpiler_emit_defers_from(ctx,
                    ctx->loop_defer_base_depth[target]);
            }
            write_indent(ctx);
            codebuf_write(ctx->out, "continue;\n");
        }
        break;
    case AST_WITH_STMT:
        emit_with_stmt(node, ctx);
        break;
    case AST_PARALLEL_BLOCK:
        emit_parallel_block(node, ctx);
        break;
    case AST_BLOCK:
        emit_block(node, ctx);
        break;
    case AST_LAMBDA_EXPR:
        {
            char *expr = emit_expression(node, ctx);
            if (expr == NULL) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                    "C statement lowering could not lower lambda expression statement");
                break;
            }
            if (expr != NULL && expr[0] != '\0') {
                write_indent(ctx);
                codebuf_write(ctx->out, "%s;\n", expr);
            }
            free(expr);
            break;
        }
    case AST_SELECT_STMT:
        emit_select_stmt(node, ctx);
        break;
    case AST_ASYNC_BLOCK:
        emit_async_block(node, ctx);
        break;
    default: {
        /* Expression statement (including event invoke) */
        char *expr = emit_expression(node, ctx);
        if (expr == NULL) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C statement lowering could not lower expression statement");
            break;
        }
        if (expr != NULL && expr[0] != '\0') {
            write_indent(ctx);
            codebuf_write(ctx->out, "%s;\n", expr);
            if (node->type == AST_CALL)
                emit_zone_action_effect_runtime(ctx->out, node, ctx);
        }
        free(expr);
        break;
    }
    }
}
