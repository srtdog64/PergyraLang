/*
 * Copyright (c) 2025 Pergyra Language Project
 * AST (Abstract Syntax Tree) implementation
 */

#include "ast.h"
#include <stdlib.h>
#include <string.h>

// ============= AST 노드 생성 함수들 =============

// 기본 노드 생성
static ASTNode* ast_create_node(ASTNodeType type) {
    ASTNode* node = calloc(1, sizeof(ASTNode));
    if (!node) return NULL;
    node->type = type;
    return node;
}

// 프로그램 노드
ASTNode* ast_create_program(void) {
    ASTNode* node = ast_create_node(AST_PROGRAM);
    node->data.program.statements = NULL;
    node->data.program.count = 0;
    return node;
}

// 함수 선언
ASTNode* ast_create_function(const char* name) {
    ASTNode* node = ast_create_node(AST_FUNC_DECL);
    node->data.func_decl.name = strdup(name);
    node->data.func_decl.params = NULL;
    node->data.func_decl.param_count = 0;
    node->data.func_decl.return_type = NULL;
    node->data.func_decl.body = NULL;
    node->data.func_decl.generic_params = NULL;
    node->data.func_decl.where_clause = NULL;
    node->data.func_decl.access = ACCESS_PUBLIC;
    return node;
}

// 클래스 선언
ASTNode* ast_create_class(const char* name) {
    ASTNode* node = ast_create_node(AST_CLASS_DECL);
    node->data.class_decl.name = strdup(name);
    node->data.class_decl.fields = NULL;
    node->data.class_decl.field_count = 0;
    node->data.class_decl.methods = NULL;
    node->data.class_decl.method_count = 0;
    node->data.class_decl.generic_params = NULL;
    node->data.class_decl.where_clause = NULL;
    return node;
}

// let 선언
ASTNode* ast_create_let_declaration(const char* name) {
    ASTNode* node = ast_create_node(AST_LET_DECL);
    node->data.let_decl.name = strdup(name);
    node->data.let_decl.type = NULL;
    node->data.let_decl.initializer = NULL;
    node->data.let_decl.is_mutable = false;
    return node;
}

// with 문
ASTNode* ast_create_with_statement(void) {
    ASTNode* node = ast_create_node(AST_WITH_STMT);
    node->data.with_stmt.slot_type = NULL;
    node->data.with_stmt.alias = NULL;
    node->data.with_stmt.body = NULL;
    node->data.with_stmt.is_secure = false;
    node->data.with_stmt.security_level = NULL;
    return node;
}

// parallel 블록
ASTNode* ast_create_parallel_block(void) {
    ASTNode* node = ast_create_node(AST_PARALLEL_BLOCK);
    node->data.parallel.tasks = NULL;
    node->data.parallel.task_count = 0;
    return node;
}

// 블록
ASTNode* ast_create_block(void) {
    ASTNode* node = ast_create_node(AST_BLOCK);
    node->data.block.statements = NULL;
    node->data.block.count = 0;
    return node;
}

// for 루프
ASTNode* ast_create_for_loop(void) {
    ASTNode* node = ast_create_node(AST_FOR_LOOP);
    node->data.for_loop.variable = NULL;
    node->data.for_loop.range_start = NULL;
    node->data.for_loop.range_end = NULL;
    node->data.for_loop.body = NULL;
    return node;
}

// if 문
ASTNode* ast_create_if_statement(void) {
    ASTNode* node = ast_create_node(AST_IF_STMT);
    node->data.if_stmt.condition = NULL;
    node->data.if_stmt.then_branch = NULL;
    node->data.if_stmt.else_branch = NULL;
    return node;
}

// return 문
ASTNode* ast_create_return_statement(void) {
    ASTNode* node = ast_create_node(AST_RETURN);
    node->data.return_stmt.value = NULL;
    return node;
}

// 표현식 노드들

// 이항 연산
ASTNode* ast_create_binary(ASTNode* left, Token op, ASTNode* right) {
    ASTNode* node = ast_create_node(AST_BINARY);
    node->data.binary.left = left;
    node->data.binary.op = op;
    node->data.binary.right = right;
    return node;
}

// 단항 연산
ASTNode* ast_create_unary(Token op, ASTNode* operand) {
    ASTNode* node = ast_create_node(AST_UNARY);
    node->data.unary.op = op;
    node->data.unary.operand = operand;
    return node;
}

// 함수 호출
ASTNode* ast_create_call(ASTNode* callee) {
    ASTNode* node = ast_create_node(AST_CALL);
    node->data.call.callee = callee;
    node->data.call.arguments = NULL;
    node->data.call.arg_count = 0;
    return node;
}

// 멤버 접근
ASTNode* ast_create_member_access(ASTNode* object, const char* member) {
    ASTNode* node = ast_create_node(AST_MEMBER_ACCESS);
    node->data.member.object = object;
    node->data.member.name = strdup(member);
    return node;
}

// 배열 접근
ASTNode* ast_create_array_access(ASTNode* array, ASTNode* index) {
    ASTNode* node = ast_create_node(AST_ARRAY_ACCESS);
    node->data.array_access.array = array;
    node->data.array_access.index = index;
    return node;
}

// 할당
ASTNode* ast_create_assignment(ASTNode* target, ASTNode* value) {
    ASTNode* node = ast_create_node(AST_ASSIGNMENT);
    node->data.assignment.target = target;
    node->data.assignment.value = value;
    return node;
}

// 리터럴

// 숫자
ASTNode* ast_create_number(const char* value) {
    ASTNode* node = ast_create_node(AST_NUMBER);
    node->data.number.value = strtod(value, NULL);
    return node;
}

// 문자열
ASTNode* ast_create_string(const char* value) {
    ASTNode* node = ast_create_node(AST_STRING);
    // 따옴표 제거
    size_t len = strlen(value);
    if (len >= 2 && value[0] == '"' && value[len-1] == '"') {
        node->data.string.value = strndup(value + 1, len - 2);
    } else {
        node->data.string.value = strdup(value);
    }
    return node;
}

// 불린
ASTNode* ast_create_boolean(bool value) {
    ASTNode* node = ast_create_node(AST_BOOLEAN);
    node->data.boolean.value = value;
    return node;
}

// 식별자
ASTNode* ast_create_identifier(const char* name) {
    ASTNode* node = ast_create_node(AST_IDENTIFIER);
    node->data.identifier.name = strdup(name);
    return node;
}

// 타입
ASTNode* ast_create_type(const char* name) {
    ASTNode* node = ast_create_node(AST_TYPE);
    node->data.type.name = strdup(name);
    node->data.type.generic_args = NULL;
    return node;
}

// ============= AST 조작 함수들 =============

// 문장 추가 (프로그램/블록)
void ast_add_statement(ASTNode* parent, ASTNode* statement) {
    if (parent->type == AST_PROGRAM) {
        parent->data.program.count++;
        parent->data.program.statements = realloc(
            parent->data.program.statements,
            parent->data.program.count * sizeof(ASTNode*)
        );
        parent->data.program.statements[parent->data.program.count - 1] = statement;
    } else if (parent->type == AST_BLOCK) {
        parent->data.block.count++;
        parent->data.block.statements = realloc(
            parent->data.block.statements,
            parent->data.block.count * sizeof(ASTNode*)
        );
        parent->data.block.statements[parent->data.block.count - 1] = statement;
    }
}

// parallel 태스크 추가
void ast_add_parallel_task(ASTNode* parallel, ASTNode* task) {
    if (parallel->type != AST_PARALLEL_BLOCK) return;
    
    parallel->data.parallel.task_count++;
    parallel->data.parallel.tasks = realloc(
        parallel->data.parallel.tasks,
        parallel->data.parallel.task_count * sizeof(ASTNode*)
    );
    parallel->data.parallel.tasks[parallel->data.parallel.task_count - 1] = task;
}

// 함수 인자 추가
void ast_add_argument(ASTNode* call, ASTNode* arg) {
    if (call->type != AST_CALL) return;
    
    call->data.call.arg_count++;
    call->data.call.arguments = realloc(
        call->data.call.arguments,
        call->data.call.arg_count * sizeof(ASTNode*)
    );
    call->data.call.arguments[call->data.call.arg_count - 1] = arg;
}

// ============= AST 메모리 해제 =============

void ast_destroy(ASTNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case AST_PROGRAM:
            for (size_t i = 0; i < node->data.program.count; i++) {
                ast_destroy(node->data.program.statements[i]);
            }
            free(node->data.program.statements);
            break;
            
        case AST_FUNC_DECL:
            free(node->data.func_decl.name);
            for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
                free(node->data.func_decl.params[i]->name);
                ast_destroy(node->data.func_decl.params[i]->type);
                free(node->data.func_decl.params[i]);
            }
            free(node->data.func_decl.params);
            ast_destroy(node->data.func_decl.return_type);
            ast_destroy(node->data.func_decl.body);
            // TODO: generic_params, where_clause 해제
            break;
            
        case AST_CLASS_DECL:
            free(node->data.class_decl.name);
            for (size_t i = 0; i < node->data.class_decl.field_count; i++) {
                free(node->data.class_decl.fields[i]->name);
                ast_destroy(node->data.class_decl.fields[i]->type);
                free(node->data.class_decl.fields[i]);
            }
            free(node->data.class_decl.fields);
            for (size_t i = 0; i < node->data.class_decl.method_count; i++) {
                ast_destroy(node->data.class_decl.methods[i]);
            }
            free(node->data.class_decl.methods);
            break;
            
        case AST_LET_DECL:
            free(node->data.let_decl.name);
            ast_destroy(node->data.let_decl.type);
            ast_destroy(node->data.let_decl.initializer);
            break;
            
        case AST_WITH_STMT:
            ast_destroy(node->data.with_stmt.slot_type);
            free(node->data.with_stmt.alias);
            ast_destroy(node->data.with_stmt.body);
            free(node->data.with_stmt.security_level);
            break;
            
        case AST_PARALLEL_BLOCK:
            for (size_t i = 0; i < node->data.parallel.task_count; i++) {
                ast_destroy(node->data.parallel.tasks[i]);
            }
            free(node->data.parallel.tasks);
            break;
            
        case AST_BLOCK:
            for (size_t i = 0; i < node->data.block.count; i++) {
                ast_destroy(node->data.block.statements[i]);
            }
            free(node->data.block.statements);
            break;
            
        case AST_FOR_LOOP:
            free(node->data.for_loop.variable);
            ast_destroy(node->data.for_loop.range_start);
            ast_destroy(node->data.for_loop.range_end);
            ast_destroy(node->data.for_loop.body);
            break;
            
        case AST_IF_STMT:
            ast_destroy(node->data.if_stmt.condition);
            ast_destroy(node->data.if_stmt.then_branch);
            ast_destroy(node->data.if_stmt.else_branch);
            break;
            
        case AST_RETURN:
            ast_destroy(node->data.return_stmt.value);
            break;
            
        case AST_BINARY:
            ast_destroy(node->data.binary.left);
            ast_destroy(node->data.binary.right);
            break;
            
        case AST_UNARY:
            ast_destroy(node->data.unary.operand);
            break;
            
        case AST_CALL:
            ast_destroy(node->data.call.callee);
            for (size_t i = 0; i < node->data.call.arg_count; i++) {
                ast_destroy(node->data.call.arguments[i]);
            }
            free(node->data.call.arguments);
            break;
            
        case AST_MEMBER_ACCESS:
            ast_destroy(node->data.member.object);
            free(node->data.member.name);
            break;
            
        case AST_ARRAY_ACCESS:
            ast_destroy(node->data.array_access.array);
            ast_destroy(node->data.array_access.index);
            break;
            
        case AST_ASSIGNMENT:
            ast_destroy(node->data.assignment.target);
            ast_destroy(node->data.assignment.value);
            break;
            
        case AST_STRING:
            free(node->data.string.value);
            break;
            
        case AST_IDENTIFIER:
            free(node->data.identifier.name);
            break;
            
        case AST_TYPE:
            free(node->data.type.name);
            // TODO: generic_args 해제
            break;
            
        default:
            break;
    }
    
    free(node);
}

// ============= AST 출력 (디버깅용) =============

static void print_indent(int level) {
    for (int i = 0; i < level; i++) {
        printf("  ");
    }
}

void ast_print(ASTNode* node, int indent) {
    if (!node) {
        print_indent(indent);
        printf("(null)\n");
        return;
    }
    
    print_indent(indent);
    
    switch (node->type) {
        case AST_PROGRAM:
            printf("Program:\n");
            for (size_t i = 0; i < node->data.program.count; i++) {
                ast_print(node->data.program.statements[i], indent + 1);
            }
            break;
            
        case AST_FUNC_DECL:
            printf("Function: %s\n", node->data.func_decl.name);
            if (node->data.func_decl.generic_params) {
                print_indent(indent + 1);
                printf("Generic params: <...>\n");
            }
            print_indent(indent + 1);
            printf("Parameters:\n");
            for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
                print_indent(indent + 2);
                printf("%s: ", node->data.func_decl.params[i]->name);
                ast_print(node->data.func_decl.params[i]->type, 0);
            }
            if (node->data.func_decl.return_type) {
                print_indent(indent + 1);
                printf("Returns: ");
                ast_print(node->data.func_decl.return_type, 0);
            }
            if (node->data.func_decl.body) {
                print_indent(indent + 1);
                printf("Body:\n");
                ast_print(node->data.func_decl.body, indent + 2);
            }
            break;
            
        case AST_LET_DECL:
            printf("Let: %s", node->data.let_decl.name);
            if (node->data.let_decl.type) {
                printf(" : ");
                ast_print(node->data.let_decl.type, 0);
            }
            printf(" = ");
            ast_print(node->data.let_decl.initializer, 0);
            break;
            
        case AST_WITH_STMT:
            printf("With %s<", node->data.with_stmt.is_secure ? "SecureSlot" : "slot");
            ast_print(node->data.with_stmt.slot_type, 0);
            printf("> as %s\n", node->data.with_stmt.alias);
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
                printf("<...>");
            }
            break;
            
        case AST_CALL:
            ast_print(node->data.call.callee, 0);
            printf("(");
            for (size_t i = 0; i < node->data.call.arg_count; i++) {
                if (i > 0) printf(", ");
                ast_print(node->data.call.arguments[i], 0);
            }
            printf(")");
            break;
            
        case AST_BINARY:
            printf("(");
            ast_print(node->data.binary.left, 0);
            printf(" %s ", token_type_to_string(node->data.binary.op.type));
            ast_print(node->data.binary.right, 0);
            printf(")");
            break;
            
        default:
            printf("AST node type %d\n", node->type);
            break;
    }
    
    if (indent == 0) printf("\n");
}

// 토큰 타입을 문자열로 변환 (디버깅용)
const char* token_type_to_string(TokenType type) {
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
