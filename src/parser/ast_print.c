/*
 * Copyright (c) 2025 Pergyra Language Project
 * AST print/debug helpers
 */

#include "ast.h"
#include <stdio.h>

static void
print_escaped_string(const char *value)
{
    const unsigned char *p = (const unsigned char *)value;

    printf("\"");
    if (p == NULL) {
        printf("\"");
        return;
    }

    while (*p != '\0') {
        switch (*p) {
        case '\\':
            printf("\\\\");
            break;
        case '"':
            printf("\\\"");
            break;
        case '\n':
            printf("\\n");
            break;
        case '\r':
            printf("\\r");
            break;
        case '\t':
            printf("\\t");
            break;
        default:
            putchar((int)*p);
            break;
        }
        p++;
    }
    printf("\"");
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
    case NOMINAL_DECL_DTO:
        return "TObject";
    case NOMINAL_DECL_CLASS:
    default:
        return "Class";
    }
}

// ============= AST 출력 (디버깅용) =============

static void print_indent(int level) {
    for (int i = 0; i < level; i++) {
        printf("  ");
    }
}

static const char* ast_operator_to_string(PgyTokenType type) {
    switch (type) {
        case TOKEN_PLUS: return "+";
        case TOKEN_MINUS: return "-";
        case TOKEN_STAR: return "*";
        case TOKEN_SLASH: return "/";
        case TOKEN_PERCENT: return "%";
        case TOKEN_EQUAL: return "==";
        case TOKEN_NOT_EQUAL: return "!=";
        case TOKEN_LESS: return "<";
        case TOKEN_LESS_EQUAL: return "<=";
        case TOKEN_GREATER: return ">";
        case TOKEN_GREATER_EQUAL: return ">=";
        case TOKEN_AND: return "&&";
        case TOKEN_OR: return "||";
        case TOKEN_NOT: return "!";
        case TOKEN_ASSIGN: return "=";
        default: return "?";
    }
}

static void ast_print_inline(ASTNode* node);
static void print_generic_params_inline(GenericParams* params);
static void print_where_clause_inline(WhereClause* clause);

static void
ast_print_compact(ASTNode* node)
{
    if (node == NULL) {
        printf("(null)");
        return;
    }

    switch (node->type) {
        case AST_IDENTIFIER:
            printf("%s", node->data.identifier.name);
            break;

        case AST_NUMBER:
            printf("%g", node->data.number.value);
            break;

        case AST_STRING:
            print_escaped_string(node->data.string.value);
            break;

        case AST_BOOLEAN:
            printf("%s", node->data.boolean.value ? "true" : "false");
            break;

        case AST_TYPE:
            printf("%s", node->data.type.name);
            if (node->data.type.generic_args)
                print_generic_params_inline(node->data.type.generic_args);
            break;

        case AST_CHANNEL_TYPE:
            printf("Channel<");
            ast_print_compact(node->data.channel_type.element_type);
            printf(">");
            if (node->data.channel_type.capacity != NULL) {
                printf("[");
                ast_print_compact(node->data.channel_type.capacity);
                printf("]");
            }
            break;

        case AST_FUTURE_TYPE:
            printf("Future<");
            ast_print_compact(node->data.future_type.value_type);
            printf(">");
            break;

        case AST_CALL:
            ast_print_compact(node->data.call.callee);
            printf("(");
            for (size_t i = 0; i < node->data.call.arg_count; i++) {
                if (i > 0)
                    printf(", ");
                ast_print_compact(node->data.call.arguments[i]);
            }
            printf(")");
            break;

        case AST_BINARY:
            printf("(");
            ast_print_compact(node->data.binary.left);
            printf(" %s ", ast_operator_to_string(node->data.binary.op.type));
            ast_print_compact(node->data.binary.right);
            printf(")");
            break;

        case AST_UNARY:
            printf("(%s", ast_operator_to_string(node->data.unary.op.type));
            ast_print_compact(node->data.unary.operand);
            printf(")");
            break;

        case AST_MEMBER_ACCESS:
            ast_print_compact(node->data.member.object);
            printf(".%s", node->data.member.name);
            break;

        case AST_ARRAY_ACCESS:
            ast_print_compact(node->data.array_access.array);
            printf("[");
            ast_print_compact(node->data.array_access.index);
            printf("]");
            break;

        case AST_ASSIGNMENT:
            ast_print_compact(node->data.assignment.target);
            printf(" = ");
            ast_print_compact(node->data.assignment.value);
            break;

        case AST_AWAIT_EXPR:
            printf("await ");
            ast_print_compact(node->data.await_expr.expression);
            break;

        case AST_CHANNEL_SEND:
            ast_print_compact(node->data.channel_send.channel);
            printf(" <- ");
            ast_print_compact(node->data.channel_send.value);
            break;

        case AST_CHANNEL_RECV:
            printf("<-");
            ast_print_compact(node->data.channel_recv.channel);
            break;

        case AST_SPAWN_EXPR:
            printf("spawn ");
            ast_print_compact(node->data.spawn_expr.function);
            break;

        case AST_CONTEXT_ACCESS:
            printf("%s(%s",
                   node->data.context_access.method_name,
                   node->data.context_access.role_slot_name);
            if (node->data.context_access.ability_type != NULL) {
                printf(", ");
                ast_print_compact(node->data.context_access.ability_type);
            }
            printf(")");
            break;

        case AST_EVENT_INVOKE:
            ast_print_compact(node->data.event_invoke.event);
            printf("(");
            for (size_t i = 0; i < node->data.event_invoke.arg_count; i++) {
                if (i > 0)
                    printf(", ");
                ast_print_compact(node->data.event_invoke.arguments[i]);
            }
            printf(")");
            break;

        case AST_EVENT_HANDLER_TYPE:
            printf("func(");
            for (size_t i = 0; i < node->data.event_handler_type.param_count; i++) {
                if (i > 0)
                    printf(", ");
                ast_print_compact(node->data.event_handler_type.param_types[i]);
            }
            printf(")");
            if (node->data.event_handler_type.return_type != NULL) {
                printf(" -> ");
                ast_print_compact(node->data.event_handler_type.return_type);
            }
            break;

        case AST_REQUIRE_FIELD:
            printf("%s", node->data.require_field.name);
            if (node->data.require_field.type != NULL) {
                printf(": ");
                ast_print_compact(node->data.require_field.type);
            }
            break;

        case AST_ROLE_SLOT:
            printf("%s", node->data.role_slot.slot_name);
            if (node->data.role_slot.is_array)
                printf("[]");
            break;

        case AST_PARTY_SHARED:
            printf("%s", node->data.party_shared.name);
            if (node->data.party_shared.type != NULL) {
                printf(": ");
                ast_print_compact(node->data.party_shared.type);
            }
            if (node->data.party_shared.initializer != NULL) {
                printf(" = ");
                ast_print_compact(node->data.party_shared.initializer);
            }
            break;

        case AST_LET_DESTRUCTURE:
            printf("let (");
            for (size_t i = 0; i < node->data.let_destructure.name_count; i++) {
                if (i > 0)
                    printf(", ");
                printf("%s", node->data.let_destructure.names[i] != NULL
                                ? node->data.let_destructure.names[i]
                                : "?");
            }
            printf(")");
            if (node->data.let_destructure.initializer != NULL) {
                printf(" = ");
                ast_print_compact(node->data.let_destructure.initializer);
            }
            break;

        case AST_LET_DECL:
            printf("let %s", node->data.let_decl.name);
            if (node->data.let_decl.type != NULL) {
                printf(": ");
                ast_print_compact(node->data.let_decl.type);
            }
            if (node->data.let_decl.initializer != NULL) {
                printf(" = ");
                ast_print_compact(node->data.let_decl.initializer);
            }
            break;

        case AST_RETURN:
            printf("return");
            if (node->data.return_stmt.value != NULL) {
                printf(" ");
                ast_print_compact(node->data.return_stmt.value);
            }
            break;

        case AST_BLOCK:
            printf("{...}");
            break;

        case AST_PARALLEL_BLOCK:
            printf("parallel {...}");
            break;

        case AST_MATCH_CASE:
            printf("case ");
            ast_print_compact(node->data.match_case.pattern);
            if (node->data.match_case.guard != NULL) {
                printf(" if ");
                ast_print_compact(node->data.match_case.guard);
            }
            break;

        case AST_MATCH_STMT:
            printf("match ");
            ast_print_compact(node->data.match_stmt.subject);
            printf(" {...}");
            break;

        case AST_FOR_LOOP:
            printf("for %s in ", node->data.for_loop.variable);
            ast_print_compact(node->data.for_loop.range_start);
            printf("..");
            ast_print_compact(node->data.for_loop.range_end);
            break;

        case AST_WHILE_LOOP:
            printf("while ");
            ast_print_compact(node->data.while_loop.condition);
            break;

        case AST_IF_STMT:
            printf("if ");
            ast_print_compact(node->data.if_stmt.condition);
            break;

        case AST_BREAK:
            printf("break");
            break;

        case AST_CONTINUE:
            printf("continue");
            break;

        case AST_PARTY_INSTANCE:
            printf("%s{...}", node->data.party_instance.party_type);
            break;

        case AST_ARRAY_LITERAL:
            printf("[");
            for (size_t i = 0; i < node->data.array_literal.count; i++) {
                if (i > 0)
                    printf(", ");
                ast_print_compact(node->data.array_literal.elements[i]);
            }
            printf("]");
            break;

        case AST_LAMBDA_EXPR:
            printf("%slambda(", node->data.lambda_expr.is_async ? "async " : "");
            for (size_t i = 0; i < node->data.lambda_expr.param_count; i++) {
                if (i > 0)
                    printf(", ");
                ast_print_compact(node->data.lambda_expr.params[i]);
            }
            printf(")");
            if (node->data.lambda_expr.return_type != NULL) {
                printf(" -> ");
                ast_print_compact(node->data.lambda_expr.return_type);
            }
            break;

        default:
            printf("<node:%d>", node->type);
            break;
    }
}

static void
ast_print_inline(ASTNode* node)
{
    ast_print_compact(node);
}

static void
print_generic_params_inline(GenericParams* params)
{
    if (params == NULL || params->count == 0) {
        return;
    }

    printf("<");
    for (size_t i = 0; i < params->count; i++) {
        GenericParam* param = params->params[i];
        if (i > 0)
            printf(", ");
        if (param == NULL) {
            printf("?");
            continue;
        }
        printf("%s", param->name != NULL ? param->name : "?");
        if (param->constraint != NULL) {
            printf(": ");
            ast_print_inline(param->constraint);
        }
        if (param->default_type != NULL) {
            printf(" = ");
            ast_print_inline(param->default_type);
        }
    }
    printf(">");
}

static void
print_where_clause_inline(WhereClause* clause)
{
    if (clause == NULL || clause->count == 0)
        return;

    printf(" where ");
    for (size_t i = 0; i < clause->count; i++) {
        TypeConstraint* constraint = clause->constraints[i];
        if (i > 0)
            printf(", ");
        if (constraint == NULL) {
            printf("?");
            continue;
        }

        printf("%s", constraint->type_param != NULL ? constraint->type_param : "?");
        if (constraint->bound_count > 0) {
            printf(": ");
            for (size_t j = 0; j < constraint->bound_count; j++) {
                if (j > 0)
                    printf(" + ");
                ast_print_inline(constraint->bounds[j]);
            }
        }
    }
}

static void
print_func_params(FuncParam** params, size_t count, int indent)
{
    print_indent(indent);
    printf("Parameters:\n");
    for (size_t i = 0; i < count; i++) {
        FuncParam* param = params[i];
        print_indent(indent + 1);
        if (param == NULL) {
            printf("?\n");
            continue;
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

static bool
ast_print_needs_trailing_newline(ASTNodeType type)
{
    switch (type) {
        case AST_IDENTIFIER:
        case AST_NUMBER:
        case AST_STRING:
        case AST_BOOLEAN:
        case AST_TYPE:
        case AST_CHANNEL_TYPE:
        case AST_FUTURE_TYPE:
        case AST_CALL:
        case AST_BINARY:
        case AST_UNARY:
        case AST_MEMBER_ACCESS:
        case AST_ARRAY_ACCESS:
        case AST_AWAIT_EXPR:
        case AST_SPAWN_EXPR:
        case AST_EVENT_HANDLER_TYPE:
            return true;
        default:
            return false;
    }
}

void ast_print(ASTNode* node, int indent) {
    if (!node) {
        print_indent(indent);
        printf("(null)\n");
        return;
    }
    
    print_indent(indent);
    if (node->is_exported)
        printf("[export] ");
    
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
                print_indent(indent + 1);
                printf("Generic params: ");
                print_generic_params_inline(node->data.func_decl.generic_params);
                printf("\n");
            }
            if (node->data.func_decl.where_clause) {
                print_indent(indent + 1);
                printf("Constraints:");
                print_where_clause_inline(node->data.func_decl.where_clause);
                printf("\n");
            }
            print_func_params(node->data.func_decl.params,
                              node->data.func_decl.param_count,
                              indent + 1);
            if (node->data.func_decl.return_type) {
                print_indent(indent + 1);
                printf("Returns: ");
                ast_print_inline(node->data.func_decl.return_type);
                printf("\n");
            }
            if (!node->is_async_decl
                && node->data.func_decl.required_ability_count > 0) {
                print_indent(indent + 1);
                printf("Requires:");
                for (size_t i = 0; i < node->data.func_decl.required_ability_count; i++) {
                    printf("%s%s", i == 0 ? " " : ", ",
                           node->data.func_decl.required_abilities[i]);
                }
                printf("\n");
            }
            if (!node->is_async_decl
                && node->data.func_decl.within_zone != NULL) {
                print_indent(indent + 1);
                printf("Within: %s\n", node->data.func_decl.within_zone);
            }
            if (!node->is_async_decl
                && node->data.func_decl.causes_effect != NULL) {
                print_indent(indent + 1);
                printf("Causes: %s\n", node->data.func_decl.causes_effect);
            }
            if (!node->is_async_decl
                && node->data.func_decl.authorized_by_count > 0) {
                print_indent(indent + 1);
                printf("Authorized by:");
                for (size_t i = 0; i < node->data.func_decl.authorized_by_count; i++) {
                    printf("%s%s", i == 0 ? " " : ", ",
                           node->data.func_decl.authorized_by[i]);
                }
                printf("\n");
            }
            if (node->data.func_decl.body) {
                print_indent(indent + 1);
                printf("Body:\n");
                ast_print(node->data.func_decl.body, indent + 2);
            }
            break;

        case AST_CLASS_DECL:
            printf("%s: %s\n",
                nominal_decl_kind_name(node->data.class_decl.nominal_kind),
                node->data.class_decl.name);
            if (node->data.class_decl.generic_params) {
                print_indent(indent + 1);
                printf("Generic params: ");
                print_generic_params_inline(node->data.class_decl.generic_params);
                printf("\n");
            }
            if (node->data.class_decl.where_clause) {
                print_indent(indent + 1);
                printf("Constraints:");
                print_where_clause_inline(node->data.class_decl.where_clause);
                printf("\n");
            }
            if (node->data.class_decl.field_count > 0) {
                print_indent(indent + 1);
                printf("Fields:\n");
                for (size_t i = 0; i < node->data.class_decl.field_count; i++) {
                    print_indent(indent + 2);
                    printf("%s: ", node->data.class_decl.fields[i]->name);
                    ast_print_inline(node->data.class_decl.fields[i]->type);
                    printf("\n");
                }
            }
            if (node->data.class_decl.method_count > 0) {
                print_indent(indent + 1);
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
            
        case AST_IDENTIFIER:
            printf("%s", node->data.identifier.name);
            break;
            
        case AST_NUMBER:
            printf("%g", node->data.number.value);
            break;
            
        case AST_STRING:
            printf("\"%s\"", node->data.string.value);
            break;
            
        case AST_BOOLEAN:
            printf("%s", node->data.boolean.value ? "true" : "false");
            break;
            
        case AST_TYPE:
            printf("%s", node->data.type.name);
            if (node->data.type.generic_args) {
                print_generic_params_inline(node->data.type.generic_args);
            }
            break;

        case AST_CALL:
            ast_print_inline(node->data.call.callee);
            printf("(");
            for (size_t i = 0; i < node->data.call.arg_count; i++) {
                if (i > 0) printf(", ");
                ast_print_inline(node->data.call.arguments[i]);
            }
            printf(")");
            break;
            
        case AST_BINARY:
            printf("(");
            ast_print_inline(node->data.binary.left);
            printf(" %s ", ast_operator_to_string(node->data.binary.op.type));
            ast_print_inline(node->data.binary.right);
            printf(")");
            break;

        case AST_BLOCK:
            printf("Block:\n");
            for (size_t i = 0; i < node->data.block.count; i++) {
                ast_print(node->data.block.statements[i], indent + 1);
            }
            break;

        case AST_FOR_LOOP:
            printf("For: %s in ", node->data.for_loop.variable);
            ast_print_inline(node->data.for_loop.range_start);
            printf("..");
            ast_print_inline(node->data.for_loop.range_end);
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
            print_indent(indent + 1);
            printf("Then:\n");
            ast_print(node->data.if_stmt.then_branch, indent + 2);
            if (node->data.if_stmt.else_branch != NULL) {
                print_indent(indent + 1);
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

        case AST_UNARY:
            printf("(%s", ast_operator_to_string(node->data.unary.op.type));
            ast_print_inline(node->data.unary.operand);
            printf(")");
            break;

        case AST_MEMBER_ACCESS:
            ast_print_inline(node->data.member.object);
            printf(".%s", node->data.member.name);
            break;

        case AST_ARRAY_ACCESS:
            ast_print_inline(node->data.array_access.array);
            printf("[");
            ast_print_inline(node->data.array_access.index);
            printf("]");
            break;

        case AST_ASSIGNMENT:
            printf("Assign: ");
            ast_print_inline(node->data.assignment.target);
            printf(" = ");
            ast_print_inline(node->data.assignment.value);
            printf("\n");
            break;

        case AST_AWAIT_EXPR:
            printf("await ");
            ast_print_inline(node->data.await_expr.expression);
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
                print_indent(indent + 1);
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
                print_indent(indent + 1);
                printf("Default:\n");
                ast_print(node->data.match_stmt.default_body, indent + 2);
            }
            break;

        case AST_MATCH_CASE:
            printf("Case: ");
            ast_print_inline(node->data.match_case.pattern);
            if (node->data.match_case.guard != NULL) {
                printf(" if ");
                ast_print_inline(node->data.match_case.guard);
            }
            printf("\n");
            ast_print(node->data.match_case.body, indent + 1);
            break;

        case AST_CHANNEL_TYPE:
            printf("Channel<");
            ast_print_inline(node->data.channel_type.element_type);
            printf(">");
            if (node->data.channel_type.capacity != NULL) {
                printf("[");
                ast_print_inline(node->data.channel_type.capacity);
                printf("]");
            }
            break;

        case AST_FUTURE_TYPE:
            printf("Future<");
            ast_print_inline(node->data.future_type.value_type);
            printf(">");
            break;

        case AST_ASYNC_BLOCK:
            printf("AsyncBlock:\n");
            for (size_t i = 0; i < node->data.async_block.statement_count; i++) {
                ast_print(node->data.async_block.statements[i], indent + 1);
            }
            break;

        case AST_SPAWN_EXPR:
            printf("spawn ");
            ast_print_inline(node->data.spawn_expr.function);
            break;

        case AST_TASK_GROUP:
            printf("TaskGroup (%s):\n", node->data.task_group.wait_all ? "all" : "any");
            for (size_t i = 0; i < node->data.task_group.task_count; i++) {
                ast_print(node->data.task_group.tasks[i], indent + 1);
            }
            break;

        case AST_ABILITY_DECL:
            printf("Ability: %s\n", node->data.ability_decl.name);
            for (size_t i = 0; i < node->data.ability_decl.require_count; i++) {
                ast_print(node->data.ability_decl.require_fields[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.ability_decl.method_count; i++) {
                ast_print(node->data.ability_decl.methods[i], indent + 1);
            }
            break;

        case AST_ROLE_DECL:
            printf("Role: %s", node->data.role_decl.name);
            if (node->data.role_decl.for_type != NULL) {
                printf(" for ");
                ast_print_inline(node->data.role_decl.for_type);
            }
            print_generic_params_inline(node->data.role_decl.generic_params);
            print_where_clause_inline(node->data.role_decl.where_clause);
            printf("\n");
            for (size_t i = 0; i < node->data.role_decl.include_count; i++) {
                ast_print(node->data.role_decl.includes[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.role_decl.impl_count; i++) {
                ast_print(node->data.role_decl.impl_abilities[i], indent + 1);
            }
            if (node->data.role_decl.parallel_block != NULL) {
                print_indent(indent + 1);
                printf("Parallel On:\n");
                ast_print(node->data.role_decl.parallel_block, indent + 2);
            }
            break;

        case AST_INCLUDE_STMT:
            printf("Include role %s", node->data.include_stmt.role_name);
            print_generic_params_inline(node->data.include_stmt.type_args);
            printf("\n");
            break;

        case AST_REQUIRE_FIELD:
            printf("Require: %s", node->data.require_field.name);
            if (node->data.require_field.type != NULL) {
                printf(": ");
                ast_print_inline(node->data.require_field.type);
            }
            printf("\n");
            break;

        case AST_IMPL_ABILITY:
            printf("Impl ability: %s\n", node->data.impl_ability.ability_name);
            for (size_t i = 0; i < node->data.impl_ability.method_count; i++) {
                ast_print(node->data.impl_ability.methods[i], indent + 1);
            }
            break;

        case AST_OVERRIDE_FUNC:
            printf("Override%s\n",
                   node->data.override_func.calls_super ? " (calls super)" : "");
            ast_print(node->data.override_func.func_decl, indent + 1);
            break;

        case AST_PARTY_DECL:
            printf("Party: %s", node->data.party_decl.name);
            if (node->data.party_decl.extends != NULL) {
                printf(" extends ");
                ast_print_inline(node->data.party_decl.extends);
            }
            print_generic_params_inline(node->data.party_decl.generic_params);
            printf("\n");
            for (size_t i = 0; i < node->data.party_decl.role_count; i++) {
                ast_print(node->data.party_decl.role_slots[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.party_decl.shared_count; i++) {
                ast_print(node->data.party_decl.shared_fields[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.party_decl.method_count; i++) {
                ast_print(node->data.party_decl.methods[i], indent + 1);
            }
            break;

        case AST_ROLE_SLOT:
            printf("RoleSlot: %s", node->data.role_slot.slot_name);
            if (node->data.role_slot.is_array)
                printf("[]");
            if (node->data.role_slot.ability_count > 0) {
                printf(" requires ");
                for (size_t i = 0; i < node->data.role_slot.ability_count; i++) {
                    if (i > 0)
                        printf(", ");
                    ast_print_inline(node->data.role_slot.required_abilities[i]);
                }
            }
            printf("\n");
            break;

        case AST_PARTY_SHARED:
            printf("Shared: %s", node->data.party_shared.name);
            if (node->data.party_shared.type != NULL) {
                printf(": ");
                ast_print_inline(node->data.party_shared.type);
            }
            if (node->data.party_shared.initializer != NULL) {
                printf(" = ");
                ast_print_inline(node->data.party_shared.initializer);
            }
            printf("\n");
            break;

        case AST_CONTEXT_ACCESS:
            printf("ContextAccess: %s(%s",
                   node->data.context_access.method_name,
                   node->data.context_access.role_slot_name);
            if (node->data.context_access.ability_type != NULL) {
                printf(", ");
                ast_print_inline(node->data.context_access.ability_type);
            }
            printf(")\n");
            break;

        case AST_PARTY_INSTANCE:
            printf("PartyInstance: %s\n", node->data.party_instance.party_type);
            for (size_t i = 0; i < node->data.party_instance.assignment_count; i++) {
                print_indent(indent + 1);
                printf("%s = ",
                       node->data.party_instance.assignments[i].slot_name);
                ast_print_inline(node->data.party_instance.assignments[i].value);
                printf("\n");
            }
            break;

        case AST_SYSTEMIC_DECL:
            printf("Systemic: %s", node->data.systemic_decl.name);
            print_generic_params_inline(node->data.systemic_decl.generic_params);
            printf("\n");
            for (size_t i = 0; i < node->data.systemic_decl.party_count; i++) {
                ast_print(node->data.systemic_decl.party_slots[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.systemic_decl.shared_count; i++) {
                ast_print(node->data.systemic_decl.shared_fields[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.systemic_decl.method_count; i++) {
                ast_print(node->data.systemic_decl.methods[i], indent + 1);
            }
            break;

        case AST_SYSTEMIC_SLOT:
            printf("SystemicSlot: %s: %s", node->data.systemic_slot.slot_name,
                   node->data.systemic_slot.party_type);
            if (node->data.systemic_slot.is_array)
                printf("[]");
            printf("\n");
            break;

        case AST_WORLD_DECL:
            printf("World: %s\n", node->data.world_decl.name);
            for (size_t i = 0; i < node->data.world_decl.systemic_count; i++) {
                ast_print(node->data.world_decl.systemics[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.world_decl.zone_count; i++) {
                ast_print(node->data.world_decl.zones[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.world_decl.activate_count; i++) {
                ast_print(node->data.world_decl.activations[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.world_decl.deactivate_count; i++) {
                ast_print(node->data.world_decl.deactivations[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.world_decl.maintained_zone_count; i++) {
                ast_print(node->data.world_decl.maintained_zones[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.world_decl.state_count; i++) {
                ast_print(node->data.world_decl.states[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.world_decl.shared_count; i++) {
                ast_print(node->data.world_decl.shared_fields[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.world_decl.method_count; i++) {
                ast_print(node->data.world_decl.methods[i], indent + 1);
            }
            break;

        case AST_WORLD_SYSTEMIC:
            printf("WorldSystemic: %s: %s",
                   node->data.world_systemic.slot_name,
                   node->data.world_systemic.systemic_type);
            if (node->data.world_systemic.initializer != NULL) {
                printf(" = ");
                ast_print_inline(node->data.world_systemic.initializer);
            }
            printf("\n");
            break;

        case AST_WORLD_ZONE:
            printf("WorldZone: %s: %s",
                   node->data.world_zone.slot_name,
                   node->data.world_zone.zone_type);
            if (node->data.world_zone.initializer != NULL) {
                printf(" = ");
                ast_print_inline(node->data.world_zone.initializer);
            }
            printf("\n");
            break;

        case AST_WORLD_ACTIVATE:
            if (node->data.world_activate.state_name != NULL)
                printf("ActivateState: %s\n", node->data.world_activate.state_name);
            else
                printf("ActivateZone: %s\n", node->data.world_activate.zone_slot_name);
            break;

        case AST_WORLD_DEACTIVATE:
            if (node->data.world_deactivate.state_name != NULL)
                printf("DeactivateState: %s\n", node->data.world_deactivate.state_name);
            else
                printf("DeactivateZone: %s\n", node->data.world_deactivate.zone_slot_name);
            break;

        case AST_WORLD_MAINTAIN:
            if (node->data.world_maintain.state_name != NULL)
                printf("MaintainZoneState: %s\n", node->data.world_maintain.state_name);
            else
                printf("MaintainZone: %s\n", node->data.world_maintain.zone_slot_name);
            break;

        case AST_WORLD_STATE:
            if (node->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL
                || node->data.world_state.source_kind == WORLD_STATE_SOURCE_ANY) {
                const char *label =
                    node->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL
                        ? "all" : "any";
                printf("WorldState: %s: %s ", node->data.world_state.state_name, label);
                for (size_t i = 0; i < node->data.world_state.input_count; i++) {
                    if (i > 0)
                        printf(", ");
                    printf("%s", node->data.world_state.input_names[i]);
                }
            } else {
                printf("WorldState: %s: zone %s",
                       node->data.world_state.state_name,
                       node->data.world_state.zone_slot_name);
            }
            if (node->data.world_state.detail_name != NULL) {
                const char *label = "zone";
                switch (node->data.world_state.source_kind) {
                    case WORLD_STATE_SOURCE_PROJECTION: label = "projection"; break;
                    case WORLD_STATE_SOURCE_LAYER: label = "layer"; break;
                    case WORLD_STATE_SOURCE_STATE: label = "state"; break;
                    case WORLD_STATE_SOURCE_ZONE:
                    default: break;
                }
                printf(" %s %s", label, node->data.world_state.detail_name);
            }
            printf("\n");
            break;

        case AST_INTENT_DECL:
            printf("Intent: %s\n", node->data.intent_decl.name);
            if (node->data.intent_decl.is_concurrent) {
                print_indent(indent + 1);
                printf("IntentMode: concurrent\n");
            } else {
                print_indent(indent + 1);
                printf("IntentMode: exclusive\n");
            }
            print_indent(indent + 1);
            printf("IntentRollback: %s\n",
                node->data.intent_decl.rollback_policy == INTENT_ROLLBACK_NONE ? "none" :
                node->data.intent_decl.rollback_policy == INTENT_ROLLBACK_CURRENT ? "current" :
                "full");
            if (node->data.intent_decl.priority_expr != NULL) {
                print_indent(indent + 1);
                printf("IntentPriority: ");
                ast_print_inline(node->data.intent_decl.priority_expr);
                printf("\n");
            }
            for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
                ast_print(node->data.intent_decl.involves[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.intent_decl.step_count; i++) {
                ast_print(node->data.intent_decl.steps[i], indent + 1);
            }
            if (node->data.intent_decl.success_expr != NULL) {
                print_indent(indent + 1);
                printf("IntentSuccess: ");
                ast_print_inline(node->data.intent_decl.success_expr);
                printf("\n");
            }
            if (node->data.intent_decl.failure_expr != NULL) {
                print_indent(indent + 1);
                printf("IntentFailure: ");
                ast_print_inline(node->data.intent_decl.failure_expr);
                printf("\n");
            }
            break;

        case AST_INTENT_INVOLVES:
            printf("IntentInvolves: %s: ", node->data.intent_involves.alias);
            ast_print_inline(node->data.intent_involves.subject_type);
            printf("\n");
            break;

        case AST_INTENT_STEP:
            printf("IntentStep: %s", node->data.intent_step.name);
            if (node->data.intent_step.where_type != NULL) {
                printf(" where ");
                ast_print_inline(node->data.intent_step.where_type);
            }
            if (node->data.intent_step.using_expr != NULL) {
                printf(" using ");
                ast_print_inline(node->data.intent_step.using_expr);
            }
            if (node->data.intent_step.intent_expr != NULL) {
                printf(" intent ");
                ast_print_inline(node->data.intent_step.intent_expr);
            }
            if (node->data.intent_step.transfer_from_alias != NULL
                && node->data.intent_step.transfer_to_alias != NULL) {
                printf(" transfer %s -> %s",
                    node->data.intent_step.transfer_from_alias,
                    node->data.intent_step.transfer_to_alias);
            }
            printf("\n");
            if (node->data.intent_step.who_count > 0) {
                print_indent(indent + 1);
                printf("Who: ");
                for (size_t i = 0; i < node->data.intent_step.who_count; i++) {
                    if (i > 0) printf(", ");
                    printf("%s", node->data.intent_step.who_names[i]);
                }
                printf("\n");
            }
            if (node->data.intent_step.using_expr != NULL) {
                print_indent(indent + 1);
                printf("Using: ");
                ast_print_inline(node->data.intent_step.using_expr);
                printf("\n");
            }
            if (node->data.intent_step.intent_expr != NULL) {
                print_indent(indent + 1);
                printf("Intent: ");
                ast_print_inline(node->data.intent_step.intent_expr);
                printf("\n");
            }
            if (node->data.intent_step.transfer_from_alias != NULL
                && node->data.intent_step.transfer_to_alias != NULL) {
                print_indent(indent + 1);
                printf("Transfer: %s -> %s\n",
                    node->data.intent_step.transfer_from_alias,
                    node->data.intent_step.transfer_to_alias);
            }
            if (node->data.intent_step.on_expr_count > 0) {
                for (size_t i = 0; i < node->data.intent_step.on_expr_count; i++) {
                    print_indent(indent + 1);
                    printf("On: ");
                    ast_print_inline(node->data.intent_step.on_exprs[i]);
                    printf("\n");
                }
            }
            if (node->data.intent_step.compensate_expr_count > 0) {
                for (size_t i = 0; i < node->data.intent_step.compensate_expr_count; i++) {
                    print_indent(indent + 1);
                    printf("Compensate: ");
                    ast_print_inline(node->data.intent_step.compensate_exprs[i]);
                    printf("\n");
                }
            }
            if (node->data.intent_step.pre_expr != NULL) {
                print_indent(indent + 1);
                printf("Pre: ");
                ast_print_inline(node->data.intent_step.pre_expr);
                printf("\n");
            }
            if (node->data.intent_step.guard_expr != NULL) {
                print_indent(indent + 1);
                printf("Guard: ");
                ast_print_inline(node->data.intent_step.guard_expr);
                printf("\n");
            }
            if (node->data.intent_step.post_expr != NULL) {
                print_indent(indent + 1);
                printf("Post: ");
                ast_print_inline(node->data.intent_step.post_expr);
                printf("\n");
            }
            if (node->data.intent_step.invariant_expr != NULL) {
                print_indent(indent + 1);
                printf("Invariant: ");
                ast_print_inline(node->data.intent_step.invariant_expr);
                printf("\n");
            }
            if (node->data.intent_step.required_ability_count > 0) {
                print_indent(indent + 1);
                printf("Requires: ");
                for (size_t i = 0; i < node->data.intent_step.required_ability_count; i++) {
                    if (i > 0) printf(", ");
                    printf("%s", node->data.intent_step.required_abilities[i]);
                }
                printf("\n");
            }
            if (node->data.intent_step.authorized_by_count > 0) {
                print_indent(indent + 1);
                printf("AuthorizedBy: ");
                for (size_t i = 0; i < node->data.intent_step.authorized_by_count; i++) {
                    if (i > 0) printf(", ");
                    printf("%s", node->data.intent_step.authorized_by[i]);
                }
                printf("\n");
            }
            if (node->data.intent_step.causes_effect != NULL) {
                print_indent(indent + 1);
                printf("Causes: %s\n", node->data.intent_step.causes_effect);
            }
            if (node->data.intent_step.expect_expr != NULL) {
                print_indent(indent + 1);
                printf("Expect: ");
                ast_print_inline(node->data.intent_step.expect_expr);
                printf("\n");
            }
            break;

        case AST_RELATION_DECL:
            printf("Relation: %s", node->data.relation_decl.name);
            if (node->data.relation_decl.between_left) {
                printf(" between %s%s, %s%s",
                    node->data.relation_decl.between_left,
                    node->data.relation_decl.between_left_many ? "[]" : "",
                    node->data.relation_decl.between_right,
                    node->data.relation_decl.between_right_many ? "[]" : "");
            }
            printf("\n");
            for (size_t i = 0; i < node->data.relation_decl.slot_count; i++) {
                ast_print(node->data.relation_decl.slots[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.relation_decl.refresh_count; i++) {
                ast_print(node->data.relation_decl.refreshes[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.relation_decl.shared_count; i++) {
                ast_print(node->data.relation_decl.shared_fields[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.relation_decl.method_count; i++) {
                ast_print(node->data.relation_decl.methods[i], indent + 1);
            }
            break;

        case AST_EFFECT_DECL:
            printf("Effect: %s\n", node->data.effect_decl.name);
            for (size_t i = 0; i < node->data.effect_decl.slot_count; i++) {
                ast_print(node->data.effect_decl.slots[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.effect_decl.refresh_count; i++) {
                ast_print(node->data.effect_decl.refreshes[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.effect_decl.shared_count; i++) {
                ast_print(node->data.effect_decl.shared_fields[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.effect_decl.method_count; i++) {
                ast_print(node->data.effect_decl.methods[i], indent + 1);
            }
            break;

        case AST_ZONE_DECL:
            printf("Zone: %s\n", node->data.zone_decl.name);
            for (size_t i = 0; i < node->data.zone_decl.slot_count; i++) {
                ast_print(node->data.zone_decl.slots[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.zone_decl.layer_slot_count; i++) {
                ast_print(node->data.zone_decl.layer_slots[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.zone_decl.apply_count; i++) {
                ast_print(node->data.zone_decl.applies[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.zone_decl.link_count; i++) {
                ast_print(node->data.zone_decl.links[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.zone_decl.detach_count; i++) {
                ast_print(node->data.zone_decl.detaches[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.zone_decl.unlink_count; i++) {
                ast_print(node->data.zone_decl.unlinks[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.zone_decl.refresh_count; i++) {
                ast_print(node->data.zone_decl.refreshes[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.zone_decl.maintained_effect_count; i++) {
                ast_print(node->data.zone_decl.maintained_effects[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.zone_decl.maintained_relation_count; i++) {
                ast_print(node->data.zone_decl.maintained_relations[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.zone_decl.authority_count; i++) {
                ast_print(node->data.zone_decl.authorities[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.zone_decl.state_count; i++) {
                ast_print(node->data.zone_decl.states[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.zone_decl.shared_count; i++) {
                ast_print(node->data.zone_decl.shared_fields[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.zone_decl.method_count; i++) {
                ast_print(node->data.zone_decl.methods[i], indent + 1);
            }
            break;

        case AST_DOMAIN_SLOT:
            printf("%sSlot: %s",
                   node->data.domain_slot.is_subject ? "Subject"
                   : (node->data.domain_slot.is_vessel ? "Vessel"
                      : (node->data.domain_slot.is_dto ? "TObject" : "Object")),
                   node->data.domain_slot.slot_name);
            if (node->data.domain_slot.type != NULL) {
                printf(": ");
                ast_print_inline(node->data.domain_slot.type);
            }
            if (node->data.domain_slot.initializer != NULL) {
                printf(" = ");
                ast_print_inline(node->data.domain_slot.initializer);
            }
            printf("\n");
            break;

        case AST_ZONE_LAYER_SLOT:
            if (node->data.zone_layer_slot.is_pool) {
                printf("%sPool: %s: %s capacity %d\n",
                       node->data.zone_layer_slot.is_relation ? "Relation" : "Effect",
                       node->data.zone_layer_slot.slot_name,
                       node->data.zone_layer_slot.layer_type,
                       node->data.zone_layer_slot.pool_capacity);
            } else {
                printf("%sSlot: %s: %s\n",
                       node->data.zone_layer_slot.is_relation ? "Relation" : "Effect",
                       node->data.zone_layer_slot.slot_name,
                       node->data.zone_layer_slot.layer_type);
            }
            break;

        case AST_ZONE_APPLY:
            if (node->data.zone_apply.state_name != NULL) {
                printf("ApplyState: %s", node->data.zone_apply.state_name);
            } else {
                printf("Apply: %s -> %s",
                       node->data.zone_apply.effect_slot_name,
                       node->data.zone_apply.target_slot_name);
            }
            if (node->data.zone_apply.actor_slot_name != NULL)
                printf(" by %s", node->data.zone_apply.actor_slot_name);
            printf("\n");
            break;

        case AST_ZONE_LINK:
            if (node->data.zone_link.state_name != NULL) {
                printf("LinkState: %s", node->data.zone_link.state_name);
            } else {
                printf("Link: %s between %s, %s",
                       node->data.zone_link.relation_slot_name,
                       node->data.zone_link.left_slot_name,
                       node->data.zone_link.right_slot_name);
            }
            if (node->data.zone_link.actor_slot_name != NULL)
                printf(" by %s", node->data.zone_link.actor_slot_name);
            printf("\n");
            break;

        case AST_ZONE_DETACH:
            if (node->data.zone_detach.state_name != NULL) {
                printf("DetachState: %s", node->data.zone_detach.state_name);
            } else {
                printf("Detach: %s from %s",
                       node->data.zone_detach.effect_slot_name,
                       node->data.zone_detach.target_slot_name);
            }
            if (node->data.zone_detach.actor_slot_name != NULL)
                printf(" by %s", node->data.zone_detach.actor_slot_name);
            printf("\n");
            break;

        case AST_ZONE_UNLINK:
            if (node->data.zone_unlink.state_name != NULL) {
                printf("UnlinkState: %s", node->data.zone_unlink.state_name);
            } else {
                printf("Unlink: %s between %s, %s",
                       node->data.zone_unlink.relation_slot_name,
                       node->data.zone_unlink.left_slot_name,
                       node->data.zone_unlink.right_slot_name);
            }
            if (node->data.zone_unlink.actor_slot_name != NULL)
                printf(" by %s", node->data.zone_unlink.actor_slot_name);
            printf("\n");
            break;

        case AST_ZONE_REFRESH:
            printf("%s: %s from %s",
                   node->data.zone_refresh.infer_target_kind ? "Bind"
                   : (node->data.zone_refresh.requires_dto ? "Publish" : "Refresh"),
                   node->data.zone_refresh.object_slot_name,
                   node->data.zone_refresh.source_slot_name);
            if (node->data.zone_refresh.actor_slot_name != NULL)
                printf(" by %s", node->data.zone_refresh.actor_slot_name);
            printf("\n");
            break;

        case AST_ZONE_MAINTAIN_EFFECT:
            printf("MaintainEffect: %s on %s",
                   node->data.zone_maintain_effect.effect_slot_name,
                   node->data.zone_maintain_effect.target_slot_name);
            if (node->data.zone_maintain_effect.actor_slot_name != NULL)
                printf(" by %s", node->data.zone_maintain_effect.actor_slot_name);
            printf("\n");
            break;

        case AST_ZONE_MAINTAIN_RELATION:
            printf("MaintainRelation: %s between %s, %s",
                   node->data.zone_maintain_relation.relation_slot_name,
                   node->data.zone_maintain_relation.left_slot_name,
                   node->data.zone_maintain_relation.right_slot_name);
            if (node->data.zone_maintain_relation.actor_slot_name != NULL)
                printf(" by %s", node->data.zone_maintain_relation.actor_slot_name);
            printf("\n");
            break;

        case AST_ZONE_MAINTAIN_STATE:
            printf("MaintainState: %s",
                   node->data.zone_maintain_state.state_name);
            if (node->data.zone_maintain_state.actor_slot_name != NULL)
                printf(" by %s", node->data.zone_maintain_state.actor_slot_name);
            printf("\n");
            break;

        case AST_ZONE_AUTHORITY:
            printf("Authority: %s\n",
                   node->data.zone_authority.subject_slot_name);
            if (node->data.zone_authority.ability_count > 0) {
                print_indent(indent + 1);
                printf("Requires:");
                for (size_t i = 0; i < node->data.zone_authority.ability_count; i++) {
                    printf("%s%s",
                           i == 0 ? " " : ", ",
                           node->data.zone_authority.required_abilities[i]);
                }
                printf("\n");
            }
            break;

        case AST_ZONE_STATE:
            printf("State: %s: %s %s %s",
                   node->data.zone_state.state_name,
                   node->data.zone_state.is_relation ? "relation" : "effect",
                   node->data.zone_state.layer_slot_name,
                   node->data.zone_state.is_relation ? "between" : "on");
            printf(" %s",
                   node->data.zone_state.left_or_target_slot_name);
            if (node->data.zone_state.is_relation
                && node->data.zone_state.right_slot_name != NULL) {
                printf(", %s", node->data.zone_state.right_slot_name);
            }
            printf("\n");
            break;

        case AST_ACTOR_DECL:
            printf("Actor: %s\n", node->data.actor_decl.name);
            if (node->data.actor_decl.field_count > 0) {
                print_indent(indent + 1);
                printf("Fields:\n");
                for (size_t i = 0; i < node->data.actor_decl.field_count; i++) {
                    print_indent(indent + 2);
                    printf("%s: ", node->data.actor_decl.fields[i]->name);
                    ast_print_inline(node->data.actor_decl.fields[i]->type);
                    printf("\n");
                }
            }
            for (size_t i = 0; i < node->data.actor_decl.method_count; i++) {
                ast_print(node->data.actor_decl.methods[i], indent + 1);
            }
            break;

        case AST_EVENT_DECL:
            printf("Event: %s\n", node->data.event_decl.name);
            if (node->data.event_decl.param_count > 0) {
                print_indent(indent + 1);
                printf("Parameters:\n");
                for (size_t i = 0; i < node->data.event_decl.param_count; i++) {
                    ast_print(node->data.event_decl.params[i], indent + 2);
                }
            }
            if (node->data.event_decl.return_type != NULL) {
                print_indent(indent + 1);
                printf("Returns: ");
                ast_print_inline(node->data.event_decl.return_type);
                printf("\n");
            }
            break;

        case AST_EVENT_SUBSCRIBE:
            printf("EventSubscribe: ");
            ast_print_inline(node->data.event_op.event);
            printf(" += ");
            ast_print_inline(node->data.event_op.handler);
            printf("\n");
            break;

        case AST_EVENT_UNSUBSCRIBE:
            printf("EventUnsubscribe: ");
            ast_print_inline(node->data.event_op.event);
            printf(" -= ");
            ast_print_inline(node->data.event_op.handler);
            printf("\n");
            break;

        case AST_EVENT_INVOKE:
            printf("EventInvoke: ");
            ast_print_inline(node->data.event_invoke.event);
            printf("(");
            for (size_t i = 0; i < node->data.event_invoke.arg_count; i++) {
                if (i > 0)
                    printf(", ");
                ast_print_inline(node->data.event_invoke.arguments[i]);
            }
            printf(")\n");
            break;

        case AST_EVENT_HANDLER_TYPE:
            printf("func(");
            for (size_t i = 0; i < node->data.event_handler_type.param_count; i++) {
                if (i > 0)
                    printf(", ");
                ast_print_inline(node->data.event_handler_type.param_types[i]);
            }
            printf(")");
            if (node->data.event_handler_type.return_type != NULL) {
                printf(" -> ");
                ast_print_inline(node->data.event_handler_type.return_type);
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
                }
                printf(" }");
            }
            printf("\n");
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
            printf("Break\n");
            break;

        case AST_CONTINUE:
            printf("Continue\n");
            break;

        case AST_PARTY_METHOD:
            printf("PartyMethod\n");
            break;
            
        default:
            printf("AST node type %d\n", node->type);
            break;
    }
    
    if (indent == 0 || ast_print_needs_trailing_newline(node->type))
        printf("\n");
}
