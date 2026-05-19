#ifndef PGY_TRANSPILER_STATEMENT_DISPATCH_H
#define PGY_TRANSPILER_STATEMENT_DISPATCH_H

#include "transpiler_control_flow_emit.h"
#include "transpiler_role_ability_helpers.h"

void
emit_statement(ASTNode *node, TranspilerCtx *ctx)
{
    if (node == NULL)
        return;

    switch (node->type) {
    case AST_LET_DECL:
        emit_let_decl(node, ctx);
        break;
    case AST_TYPE_ALIAS:
        emit_type_alias_decl(node, ctx);
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
         * Nothing to emit ??the imported declarations are
         * already present in the merged AST. */
        break;
    case AST_USE_DECL:
        /* use module; ??standard library modules.
         * Runtime functions are always available via pgy_runtime.h.
         * Future: emit module-specific includes/initializers here. */
        codebuf_write(ctx->out, "/* use %s */\n",
            ast_use_module_name(node) != NULL
                ? ast_use_module_name(node) : "unknown");
        break;
    case AST_UNSAFE_BLOCK:
        /* unsafe { ... } ??emit body directly (no safety wrappers) */
        write_indent(ctx);
        codebuf_write(ctx->out, "/* unsafe */\n");
        if (ast_unsafe_block_body(node) != NULL)
            emit_block(ast_unsafe_block_body(node), ctx);
        break;
    case AST_DEFER_STMT: {
        transpiler_register_defer(ast_defer_body(node), ctx);
        break;
    }
    case AST_BIND_STMT: {
        /* bind party.slot = Role;
         * ??lookup party's typed_var to get PartyType,
         *   then emit PartyType_bind_slot(&party, NULL, &Role_Ability_vtable_instance)
         * For now: use the typed_var mapping to find the party type. */
        const char *pvar = ast_bind_statement_party_var(node);
        const char *slot = ast_bind_statement_slot_name(node);
        const char *role = ast_bind_statement_role_name(node);
        const char *party_type = NULL;
        for (int ti = 0; ti < ctx->typed_var_count; ti++) {
            if (strcmp(ctx->typed_vars[ti].name, pvar) == 0) {
                party_type = ctx->typed_vars[ti].type_name;
                break;
            }
        }
        if (party_type == NULL) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot resolve party type for bind statement '%s.%s = %s'",
                pvar != NULL ? pvar : "<party>",
                slot != NULL ? slot : "<slot>",
                role != NULL ? role : "<role>");
            break;
        }

        /* Find the ability name by scanning the current program view for the
         * party declaration. In MIR-backed emission this view is synthesized
         * from MIRProgram inventory, not from the original HIR. The dyn role
         * slot records the required ability. */
        const char *ability_name = NULL;
        char *ability_tag = NULL;
        {
            ASTNode *it = find_party_decl(ctx, party_type);
            if (it == NULL) {
                transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot resolve party declaration '%s' while emitting bind statement '%s.%s = %s'",
                    party_type,
                    pvar != NULL ? pvar : "<party>",
                    slot != NULL ? slot : "<slot>",
                    role != NULL ? role : "<role>");
                break;
            }
            for (size_t ri = 0; ri < ast_party_role_count(it); ri++) {
                ASTNode *rs = ast_party_role(it, ri);
                const char *role_slot_name = ast_role_slot_name(rs);
                ASTNode *first_ability = ast_role_slot_required_ability(rs, 0);
                if (role_slot_name != NULL
                    && strcmp(role_slot_name, slot) == 0
                    && first_ability != NULL) {
                    ability_tag = render_ability_ref_vtable_tag(
                        first_ability);
                    ability_name = ability_tag;
                    break;
                }
            }
        }
        if (ability_name == NULL) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot resolve required ability tag for party slot '%s.%s' while emitting bind statement",
                party_type,
                slot != NULL ? slot : "<slot>");
            free(ability_tag);
            break;
        }
        write_indent(ctx);
        codebuf_write(ctx->out,
            "%s_bind_%s(&%s, NULL, &%s_%s_vtable_instance);\n",
            party_type, slot, pvar,
            role, ability_name);
        free(ability_tag);
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
        if (expr != NULL && expr[0] != '\0') {
            write_indent(ctx);
            codebuf_write(ctx->out, "%s;\n", expr);
            if (node->type == AST_CALL)
                emit_zone_action_effect_runtime(node, ctx);
        }
        free(expr);
        break;
    }
    }
}

#endif /* PGY_TRANSPILER_STATEMENT_DISPATCH_H */
