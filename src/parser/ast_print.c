/*
 * Copyright (c) 2025 Pergyra Language Project
 * AST print/debug helpers
 */

#include "ast_print_internal.h"
#include "ast_api.h"
#include "../semantic/callable_contract_vocabulary.h"
#include <stdio.h>

static void
print_func_mask_clause(const char *label, uint32_t mask,
                       PgyCallableContractAxis axis, int indent)
{
    bool first = true;
    size_t count = pgy_callable_contract_vocabulary_axis_count(axis);

    ast_print_indent(indent);
    printf("%s:", label);
    if (mask == 0 && axis == PGY_CALLABLE_CONTRACT_AXIS_EFFECT) {
        const PgyCallableContractWordSpec *zero =
            pgy_callable_contract_vocabulary_at_rank(axis, count - 1);
        printf(" %s", zero->spelling);
    } else {
        for (size_t i = 0; i < count; i++) {
            const PgyCallableContractWordSpec *spec =
                pgy_callable_contract_vocabulary_at_rank(axis, i);
            if (spec == NULL || spec->mask == 0 ||
                (mask & spec->mask) == 0)
                continue;
            printf("%s%s", first ? " " : ", ", spec->spelling);
            first = false;
        }
    }
    printf("\n");
}

static void
print_func_contract_masks(const ASTNode *node, int indent)
{
    if (ast_func_has_caps_clause(node)) {
        print_func_mask_clause("Caps", ast_func_declared_capabilities(node),
                               PGY_CALLABLE_CONTRACT_AXIS_CAPABILITY, indent);
    }
    if (ast_func_has_effects_clause(node)) {
        print_func_mask_clause("Effects", ast_func_declared_effects(node),
                               PGY_CALLABLE_CONTRACT_AXIS_EFFECT, indent);
    }
}

static const char *
nominal_decl_kind_name(NominalDeclKind kind)
{
    switch (kind) {
    case NOMINAL_DECL_SUBJECT:
        return "Subject";
    case NOMINAL_DECL_VESSEL:
        return "Vessel";
    case NOMINAL_DECL_STRUCT:
        return "Struct";
    case NOMINAL_DECL_OBJECT:
        return "Object";
    case NOMINAL_DECL_TOBJECT:
        return "TObject";
    case NOMINAL_DECL_CLASS:
    default:
        return "Class";
    }
}

void
ast_print_indent(int level)
{
    for (int i = 0; i < level; i++) {
        printf("  ");
    }
}

// ============= AST debug output =============

static void
print_func_params(FuncParam** params, size_t count, int indent)
{
    ast_print_indent(indent);
    printf("Parameters:\n");
    for (size_t i = 0; i < count; i++) {
        FuncParam* param = params[i];
        ast_print_indent(indent + 1);
        if (param == NULL) {
            printf("?\n");
            continue;
        }
        switch (param->mode) {
        case PARAM_MODE_OWN:
            printf("own ");
            break;
        case PARAM_MODE_REF:
            printf("ref ");
            break;
        case PARAM_MODE_MUT_REF:
            printf("inout ");
            break;
        case PARAM_MODE_DEFAULT:
        default:
            break;
        }
        printf("%s", param->name != NULL ? param->name : "?");
        if (param->type != NULL) {
            printf(": ");
            ast_print_inline(param->type);
        }
        if (param->default_value != NULL) {
            printf(" = ");
            ast_print_inline(param->default_value);
        }
        printf("\n");
    }
}

void ast_print(ASTNode* node, int indent) {
    if (!node) {
        ast_print_indent(indent);
        printf("(null)\n");
        return;
    }

    ast_print_indent(indent);
    if (node->is_exported)
        printf("[export] ");

    if (ast_print_domain_node(node, indent))
        goto done;
    if (ast_print_expr_node(node, indent))
        goto done;

    switch (node->type) {
        case AST_PROGRAM:
            printf("Program:\n");
            for (size_t i = 0; i < node->data.program.count; i++) {
                ast_print(node->data.program.statements[i], indent + 1);
            }
            break;

        case AST_FUNC_DECL:
            printf("%s: %s\n",
                   (!node->is_async_decl && node->data.func_decl.is_action)
                       ? "Action" : "Function",
                   node->data.func_decl.name);
            if (node->data.func_decl.generic_params) {
                ast_print_indent(indent + 1);
                printf("Generic params: ");
                print_generic_params_inline(node->data.func_decl.generic_params);
                printf("\n");
            }
            if (node->data.func_decl.where_clause) {
                ast_print_indent(indent + 1);
                printf("Constraints:");
                print_where_clause_inline(node->data.func_decl.where_clause);
                printf("\n");
            }
            print_func_params(node->data.func_decl.params,
                              node->data.func_decl.param_count,
                              indent + 1);
            if (node->data.func_decl.return_type) {
                ast_print_indent(indent + 1);
                printf("Returns: ");
                ast_print_inline(node->data.func_decl.return_type);
                printf("\n");
            }
            if (!node->is_async_decl
                && node->data.func_decl.required_ability_count > 0) {
                ast_print_indent(indent + 1);
                printf("Requires:");
                for (size_t i = 0; i < node->data.func_decl.required_ability_count; i++) {
                    printf("%s", i == 0 ? " " : ", ");
                    ast_print_inline(node->data.func_decl.required_abilities[i]);
                }
                printf("\n");
            }
            if (!node->is_async_decl
                && node->data.func_decl.within_zone != NULL) {
                ast_print_indent(indent + 1);
                printf("Within: %s\n", node->data.func_decl.within_zone);
            }
            if (!node->is_async_decl
                && node->data.func_decl.causes_effect != NULL) {
                ast_print_indent(indent + 1);
                printf("Causes: %s\n", node->data.func_decl.causes_effect);
            }
            if (!node->is_async_decl
                && node->data.func_decl.authorized_by_count > 0) {
                ast_print_indent(indent + 1);
                printf("Authorized by:");
                for (size_t i = 0; i < node->data.func_decl.authorized_by_count; i++) {
                    printf("%s%s", i == 0 ? " " : ", ",
                           node->data.func_decl.authorized_by[i]);
                }
                printf("\n");
            }
            print_func_contract_masks(node, indent + 1);
            if (node->data.func_decl.body) {
                ast_print_indent(indent + 1);
                printf("Body:\n");
                ast_print(node->data.func_decl.body, indent + 2);
            }
            break;

        case AST_CLASS_DECL:
            printf("%s: %s\n",
                nominal_decl_kind_name(node->data.class_decl.nominal_kind),
                node->data.class_decl.name);
            if (node->data.class_decl.generic_params) {
                ast_print_indent(indent + 1);
                printf("Generic params: ");
                print_generic_params_inline(node->data.class_decl.generic_params);
                printf("\n");
            }
            if (node->data.class_decl.where_clause) {
                ast_print_indent(indent + 1);
                printf("Constraints:");
                print_where_clause_inline(node->data.class_decl.where_clause);
                printf("\n");
            }
            if (node->data.class_decl.field_count > 0) {
                ast_print_indent(indent + 1);
                printf("Fields:\n");
                for (size_t i = 0; i < node->data.class_decl.field_count; i++) {
                    ast_print_indent(indent + 2);
                    printf("%s: ", node->data.class_decl.fields[i]->name);
                    ast_print_inline(node->data.class_decl.fields[i]->type);
                    printf("\n");
                }
            }
            if (node->data.class_decl.method_count > 0) {
                ast_print_indent(indent + 1);
                printf("Methods:\n");
                for (size_t i = 0; i < node->data.class_decl.method_count; i++) {
                    ast_print(node->data.class_decl.methods[i], indent + 2);
                }
            }
            break;

        case AST_EXTERN_BLOCK:
            printf("Extern Block (%s):\n", node->data.extern_block.abi);
            for (size_t i = 0; i < node->data.extern_block.count; i++) {
                ast_print(node->data.extern_block.declarations[i], indent + 1);
            }
            break;

        case AST_LET_DECL:
            printf("Let: %s", node->data.let_decl.name);
            if (node->data.let_decl.type) {
                printf(" : ");
                ast_print_inline(node->data.let_decl.type);
            }
            printf(" = ");
            ast_print_inline(node->data.let_decl.initializer);
            printf("\n");
            break;

        case AST_TYPE_ALIAS:
            printf("TypeAlias: %s = ", node->data.type_alias.name);
            ast_print_inline(node->data.type_alias.target_type);
            printf("\n");
            break;

        case AST_WITH_STMT:
            printf("With %s<", node->data.with_stmt.is_secure ? "SecureSlot" : "slot");
            ast_print_inline(node->data.with_stmt.slot_type);
            printf("> as %s", node->data.with_stmt.alias);
            if (node->data.with_stmt.security_level != NULL) {
                printf(" [security=%s]", node->data.with_stmt.security_level);
            }
            printf("\n");
            ast_print(node->data.with_stmt.body, indent + 1);
            break;

        case AST_PARALLEL_BLOCK:
            printf("Parallel:\n");
            for (size_t i = 0; i < node->data.parallel.task_count; i++) {
                ast_print(node->data.parallel.tasks[i], indent + 1);
            }
            break;

        case AST_BLOCK:
            if (node->data.block.is_pin_block) {
                printf("Pin Block: %s as %s (%s)\n",
                    node->data.block.pin_source_name != NULL
                        ? node->data.block.pin_source_name : "<expr>",
                    node->data.block.pin_view_name != NULL
                        ? node->data.block.pin_view_name : "<view>",
                    node->data.block.pin_view_is_write
                        ? "WriteView" : "ReadView");
            } else {
                printf("Block:\n");
            }
            for (size_t i = 0; i < node->data.block.count; i++) {
                ast_print(node->data.block.statements[i], indent + 1);
            }
            break;

        case AST_FOR_LOOP:
            printf("For: %s in ", node->data.for_loop.variable);
            if (node->data.for_loop.iterable != NULL) {
                /* for-each over a collection: print the iterable expression. */
                ast_print_inline(node->data.for_loop.iterable);
            } else {
                /* range loop `for i in a..b`. */
                ast_print_inline(node->data.for_loop.range_start);
                printf("..");
                ast_print_inline(node->data.for_loop.range_end);
            }
            printf("\n");
            ast_print(node->data.for_loop.body, indent + 1);
            break;

        case AST_WHILE_LOOP:
            printf("While: ");
            ast_print_inline(node->data.while_loop.condition);
            printf("\n");
            ast_print(node->data.while_loop.body, indent + 1);
            break;

        case AST_IF_STMT:
            printf("If: ");
            ast_print_inline(node->data.if_stmt.condition);
            printf("\n");
            ast_print_indent(indent + 1);
            printf("Then:\n");
            ast_print(node->data.if_stmt.then_branch, indent + 2);
            if (node->data.if_stmt.else_branch != NULL) {
                ast_print_indent(indent + 1);
                printf("Else:\n");
                ast_print(node->data.if_stmt.else_branch, indent + 2);
            }
            break;

        case AST_RETURN:
            printf("Return");
            if (node->data.return_stmt.value != NULL) {
                printf(": ");
                ast_print_inline(node->data.return_stmt.value);
            }
            printf("\n");
            break;

        case AST_GIVE_STMT:
            printf("Give: ");
            ast_print_inline(node->data.give_stmt.value);
            printf("\n");
            break;

        case AST_ASSIGNMENT:
            printf("Assign: ");
            ast_print_inline(node->data.assignment.target);
            printf(" = ");
            ast_print_inline(node->data.assignment.value);
            printf("\n");
            break;

        case AST_CHANNEL_SEND:
            printf("ChannelSend: ");
            ast_print_inline(node->data.channel_send.channel);
            printf(" <- ");
            ast_print_inline(node->data.channel_send.value);
            printf("\n");
            break;

        case AST_CHANNEL_RECV:
            printf("ChannelRecv: ");
            ast_print_inline(node->data.channel_recv.channel);
            printf("\n");
            break;

        case AST_SELECT_STMT:
            printf("Select:\n");
            for (size_t i = 0; i < node->data.select_stmt.case_count; i++) {
                ast_print(node->data.select_stmt.cases[i], indent + 1);
            }
            if (node->data.select_stmt.default_case != NULL) {
                ast_print_indent(indent + 1);
                printf("Default:\n");
                ast_print(node->data.select_stmt.default_case, indent + 2);
            }
            break;

        case AST_MATCH_STMT:
            printf("Match: ");
            ast_print_inline(node->data.match_stmt.subject);
            printf("\n");
            for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
                ast_print(node->data.match_stmt.cases[i], indent + 1);
            }
            if (node->data.match_stmt.default_body != NULL) {
                ast_print_indent(indent + 1);
                printf("Default:\n");
                ast_print(node->data.match_stmt.default_body, indent + 2);
            }
            break;

        case AST_MATCH_CASE:
            printf("Case: ");
            if (node->data.match_case.patterns != NULL
                && node->data.match_case.pattern_count > 0) {
                for (size_t i = 0; i < node->data.match_case.pattern_count; i++) {
                    if (i > 0)
                        printf(" | ");
                    ast_print_inline(node->data.match_case.patterns[i]);
                }
            } else {
                ast_print_inline(node->data.match_case.pattern);
            }
            if (node->data.match_case.guard != NULL) {
                printf(" if ");
                ast_print_inline(node->data.match_case.guard);
            }
            printf("\n");
            ast_print(node->data.match_case.body, indent + 1);
            break;

        case AST_ASYNC_BLOCK:
            printf("AsyncBlock:\n");
            for (size_t i = 0; i < node->data.async_block.statement_count; i++) {
                ast_print(node->data.async_block.statements[i], indent + 1);
            }
            break;

        case AST_LAMBDA_EXPR:
            printf("%slambda(", node->data.lambda_expr.is_async ? "async " : "");
            for (size_t i = 0; i < node->data.lambda_expr.param_count; i++) {
                if (i > 0)
                    printf(", ");
                ast_print_inline(node->data.lambda_expr.params[i]);
            }
            printf(")");
            if (node->data.lambda_expr.return_type != NULL) {
                printf(" -> ");
                ast_print_inline(node->data.lambda_expr.return_type);
            }
            printf("\n");
            ast_print(node->data.lambda_expr.body, indent + 1);
            break;

        case AST_IMPORT_DECL:
            printf("Import: \"%s\"\n", node->data.import_decl.path);
            break;

        case AST_USE_DECL:
            printf("Use: %s\n", node->data.use_decl.module_name != NULL
                                     ? node->data.use_decl.module_name
                                     : "<unknown>");
            break;

        case AST_ENUM_DECL:
            printf("Enum: %s", node->data.enum_decl.name != NULL
                                  ? node->data.enum_decl.name
                                  : "<anonymous>");
            if (node->data.enum_decl.variant_count > 0) {
                printf(" { ");
                for (size_t i = 0; i < node->data.enum_decl.variant_count; i++) {
                    if (i > 0)
                        printf(", ");
                    printf("%s",
                           node->data.enum_decl.variants[i] != NULL
                               ? node->data.enum_decl.variants[i]
                               : "?");
                    size_t param_count =
                        node->data.enum_decl.variant_param_counts != NULL
                            ? node->data.enum_decl.variant_param_counts[i]
                            : 0;
                    if (param_count > 0) {
                        printf("(");
                        for (size_t p = 0; p < param_count; p++) {
                            if (p > 0)
                                printf(", ");
                            ast_print_inline(
                                node->data.enum_decl.variant_params[i][p]);
                        }
                        printf(")");
                    }
                }
                printf(" }");
            }
            printf("\n");
            for (size_t i = 0; i < node->data.enum_decl.method_count; i++)
                ast_print(node->data.enum_decl.methods[i], indent + 1);
            break;

        case AST_LIFECYCLE_DECL:
            printf("Lifecycle: %s\n",
                   node->data.lifecycle_decl.subject != NULL
                       ? node->data.lifecycle_decl.subject
                       : "<unknown>");
            for (size_t i = 0; i < node->data.lifecycle_decl.transition_count; i++) {
                const LifecycleTransitionDecl *t =
                    &node->data.lifecycle_decl.transitions[i];
                ast_print_indent(indent + 1);
                printf("%s: %s -> %s\n",
                       t->op != NULL ? t->op : "<op>",
                       t->from_state != NULL ? t->from_state : "<from>",
                       t->to_state != NULL ? t->to_state : "<to>");
            }
            break;

        case AST_NAMESPACE_DECL:
            printf("Namespace: %s\n", node->data.namespace_decl.name);
            for (size_t i = 0; i < node->data.namespace_decl.count; i++) {
                ast_print(node->data.namespace_decl.statements[i], indent + 1);
            }
            break;

        case AST_UNSAFE_BLOCK:
            printf("UnsafeBlock:\n");
            ast_print(node->data.unsafe_block.body, indent + 1);
            break;

        case AST_TRANSACTION_BLOCK:
            printf("TransactionBlock:\n");
            ast_print(node->data.transaction_block.body, indent + 1);
            break;

        case AST_FAIL_STMT:
            printf("Fail:\n");
            ast_print(node->data.fail_stmt.reason, indent + 1);
            break;

        case AST_DEFER_STMT:
            printf("Defer:\n");
            ast_print(node->data.defer_stmt.body, indent + 1);
            break;

        case AST_BIND_STMT:
            printf("BindStmt: %s.%s = %s\n",
                   node->data.bind_stmt.party_var != NULL
                       ? node->data.bind_stmt.party_var
                       : "<unknown>",
                   node->data.bind_stmt.slot_name != NULL
                       ? node->data.bind_stmt.slot_name
                       : "<unknown>",
                   node->data.bind_stmt.role_name != NULL
                       ? node->data.bind_stmt.role_name
                       : "<unknown>");
            break;

        case AST_BREAK:
            printf("Break");
            if (node->data.break_stmt.label != NULL)
                printf(": %s", node->data.break_stmt.label);
            printf("\n");
            break;

        case AST_CONTINUE:
            printf("Continue");
            if (node->data.continue_stmt.label != NULL)
                printf(": %s", node->data.continue_stmt.label);
            printf("\n");
            break;

        case AST_PARTY_METHOD:
            printf("PartyMethod\n");
            break;

        default:
            printf("AST node type %d\n", node->type);
            break;
    }

done:
    if (indent == 0 || ast_print_needs_trailing_newline(node->type))
        printf("\n");
}
