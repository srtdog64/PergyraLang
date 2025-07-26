/*
 * Copyright (c) 2025 Pergyra Language Project
 * Parser implementation with generic-first design
 */

#include "parser.h"
#include "ast.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

// Forward declarations for static functions
static ASTNode* parse_type(Parser* parser);
static ASTNode* parse_logical_or(Parser* parser);
static ASTNode* parse_logical_and(Parser* parser);
static ASTNode* parse_equality(Parser* parser);
static ASTNode* parse_comparison(Parser* parser);
static ASTNode* parse_addition(Parser* parser);
static ASTNode* parse_multiplication(Parser* parser);
static ASTNode* parse_unary(Parser* parser);
static ASTNode* finish_call(Parser* parser, ASTNode* callee);
static ASTNode* parse_function_declaration(Parser* parser);
static ASTNode* parse_class_declaration(Parser* parser);
static ASTNode* parse_for_loop(Parser* parser);
static ASTNode* parse_if_statement(Parser* parser);
static ASTNode* parse_return_statement(Parser* parser);
static ASTNode* parse_type_constraint(Parser* parser);

// 파서 생성
Parser* parser_create(Lexer* lexer) {
    Parser* parser = calloc(1, sizeof(Parser));
    if (!parser) return NULL;
    
    parser->lexer = lexer;
    parser->has_error = false;
    parser->scope_depth = 0;
    parser->in_parallel_block = false;
    parser->in_with_statement = false;
    
    // 첫 번째 토큰 읽기
    parser->current_token = lexer_next_token(lexer);
    
    return parser;
}

// 파서 소멸
void parser_destroy(Parser* parser) {
    if (parser) {
        free(parser);
    }
}

// 토큰 진행
Token parser_advance(Parser* parser) {
    parser->previous_token = parser->current_token;
    parser->current_token = lexer_next_token(parser->lexer);
    return parser->previous_token;
}

// 토큰 타입 확인
bool parser_check(Parser* parser, TokenType type) {
    return parser->current_token.type == type;
}

// 토큰 매칭 및 진행
bool parser_match(Parser* parser, TokenType type) {
    if (!parser_check(parser, type)) return false;
    parser_advance(parser);
    return true;
}

// 토큰 소비 (필수)
Token parser_consume(Parser* parser, TokenType type, const char* message) {
    if (parser_check(parser, type)) {
        return parser_advance(parser);
    }
    
    parser_error(parser, message);
    return parser->current_token;
}

// 에러 처리
void parser_error(Parser* parser, const char* format, ...) {
    parser->has_error = true;
    
    va_list args;
    va_start(args, format);
    vsnprintf(parser->error_msg, sizeof(parser->error_msg), format, args);
    va_end(args);
    
    // 에러 위치 정보 추가
    char location[256];
    snprintf(location, sizeof(location), " at line %d, column %d", 
             parser->current_token.line, parser->current_token.column);
    strncat(parser->error_msg, location, 
            sizeof(parser->error_msg) - strlen(parser->error_msg) - 1);
}

// 에러 복구 - 다음 문장까지 건너뛰기
void parser_synchronize(Parser* parser) {
    parser_advance(parser);
    
    while (!parser_is_at_end(parser)) {
        if (parser->previous_token.type == TOKEN_SEMICOLON) return;
        
        switch (parser->current_token.type) {
            case TOKEN_CLASS:
            case TOKEN_FUNC:
            case TOKEN_LET:
            case TOKEN_WITH:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_RETURN:
                return;
            default:
                break;
        }
        
        parser_advance(parser);
    }
}

// 파싱 종료 확인
bool parser_is_at_end(const Parser* parser) {
    return parser->current_token.type == TOKEN_EOF;
}

// 에러 확인
bool parser_has_error(const Parser* parser) {
    return parser->has_error;
}

// 에러 메시지 가져오기
const char* parser_get_error(const Parser* parser) {
    return parser->error_msg;
}

// ============= 제네릭 파싱 =============

// 제네릭 파라미터 파싱: <T, U: Trait, V = Default>
static GenericParams* parse_generic_params(Parser* parser) {
    if (!parser_match(parser, TOKEN_LESS)) return NULL;
    
    GenericParams* params = calloc(1, sizeof(GenericParams));
    params->count = 0;
    params->params = NULL;
    
    while (!parser_check(parser, TOKEN_GREATER) && !parser_is_at_end(parser)) {
        GenericParam* param = calloc(1, sizeof(GenericParam));
        
        // 타입 파라미터 이름
        Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected type parameter name");
        param->name = strdup(name.text);
        
        // 제약조건 ': Trait'
        if (parser_match(parser, TOKEN_COLON)) {
            param->constraint = parse_type_constraint(parser);
        }
        
        // 기본값 '= Type'
        if (parser_match(parser, TOKEN_ASSIGN)) {
            param->default_type = parse_type(parser);
        }
        
        // 파라미터 추가
        params->count++;
        params->params = realloc(params->params, params->count * sizeof(GenericParam*));
        params->params[params->count - 1] = param;
        
        if (!parser_match(parser, TOKEN_COMMA)) break;
    }
    
    parser_consume(parser, TOKEN_GREATER, "Expected '>' after generic parameters");
    
    return params;
}

// where 절 파싱: where T: Comparable + Clone
static WhereClause* parse_where_clause(Parser* parser) {
    if (!parser_match(parser, TOKEN_WHERE)) return NULL;
    
    WhereClause* where = calloc(1, sizeof(WhereClause));
    where->count = 0;
    where->constraints = NULL;
    
    do {
        TypeConstraint* constraint = calloc(1, sizeof(TypeConstraint));
        
        // 타입 파라미터
        Token param = parser_consume(parser, TOKEN_IDENTIFIER, "Expected type parameter");
        constraint->type_param = strdup(param.text);
        
        parser_consume(parser, TOKEN_COLON, "Expected ':' after type parameter");
        
        // Trait 바운드 (Trait1 + Trait2 + ...)
        constraint->bound_count = 0;
        constraint->bounds = NULL;
        
        do {
            ASTNode* trait = parse_type(parser);
            constraint->bound_count++;
            constraint->bounds = realloc(constraint->bounds, 
                                       constraint->bound_count * sizeof(ASTNode*));
            constraint->bounds[constraint->bound_count - 1] = trait;
        } while (parser_match(parser, TOKEN_PLUS));
        
        // 제약조건 추가
        where->count++;
        where->constraints = realloc(where->constraints, 
                                   where->count * sizeof(TypeConstraint*));
        where->constraints[where->count - 1] = constraint;
        
    } while (parser_match(parser, TOKEN_COMMA));
    
    return where;
}

// 타입 파싱: Type<T, U>
static ASTNode* parse_type(Parser* parser) {
    Token type_name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected type name");
    
    ASTNode* type_node = ast_create_type(type_name.text);
    
    // 제네릭 인자
    if (parser_check(parser, TOKEN_LESS)) {
        type_node->data.type.generic_args = parse_generic_params(parser);
    }
    
    return type_node;
}

// ============= 문장 파싱 =============

// 프로그램 파싱
ASTNode* parser_parse_program(Parser* parser) {
    ASTNode* program = ast_create_program();
    
    while (!parser_is_at_end(parser)) {
        ASTNode* stmt = parser_parse_statement(parser);
        if (stmt) {
            ast_add_statement(program, stmt);
        }
        
        if (parser->has_error) {
            parser_synchronize(parser);
        }
    }
    
    return program;
}

// 문장 파싱
ASTNode* parser_parse_statement(Parser* parser) {
    // 함수 선언
    if (parser_match(parser, TOKEN_FUNC)) {
        return parse_function_declaration(parser);
    }
    
    // 클래스 선언
    if (parser_match(parser, TOKEN_CLASS)) {
        return parse_class_declaration(parser);
    }
    
    // let 선언
    if (parser_match(parser, TOKEN_LET)) {
        return parser_parse_let_declaration(parser);
    }
    
    // with 문
    if (parser_match(parser, TOKEN_WITH)) {
        return parser_parse_with_statement(parser);
    }
    
    // parallel 블록
    if (parser_match(parser, TOKEN_PARALLEL)) {
        return parser_parse_parallel_block(parser);
    }
    
    // for 루프
    if (parser_match(parser, TOKEN_FOR)) {
        return parse_for_loop(parser);
    }
    
    // if 문
    if (parser_match(parser, TOKEN_IF)) {
        return parse_if_statement(parser);
    }
    
    // return 문
    if (parser_match(parser, TOKEN_RETURN)) {
        return parse_return_statement(parser);
    }
    
    // 표현식 문장
    return parser_parse_expression_statement(parser);
}

// 함수 선언 파싱
static ASTNode* parse_function_declaration(Parser* parser) {
    // 함수 이름
    Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected function name");
    
    ASTNode* func = ast_create_function(name.text);
    
    // 제네릭 파라미터
    func->data.func_decl.generic_params = parse_generic_params(parser);
    
    // 함수 파라미터
    parser_consume(parser, TOKEN_LPAREN, "Expected '(' after function name");
    
    while (!parser_check(parser, TOKEN_RPAREN) && !parser_is_at_end(parser)) {
        // 파라미터 이름
        Token param_name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected parameter name");
        parser_consume(parser, TOKEN_COLON, "Expected ':' after parameter name");
        
        // 파라미터 타입
        ASTNode* param_type = parse_type(parser);
        
        FuncParam* param = calloc(1, sizeof(FuncParam));
        param->name = strdup(param_name.text);
        param->type = param_type;
        
        // 파라미터 추가
        func->data.func_decl.param_count++;
        func->data.func_decl.params = realloc(func->data.func_decl.params,
                                             func->data.func_decl.param_count * sizeof(FuncParam*));
        func->data.func_decl.params[func->data.func_decl.param_count - 1] = param;
        
        if (!parser_match(parser, TOKEN_COMMA)) break;
    }
    
    parser_consume(parser, TOKEN_RPAREN, "Expected ')' after parameters");
    
    // 반환 타입
    if (parser_match(parser, TOKEN_ARROW)) {
        func->data.func_decl.return_type = parse_type(parser);
    }
    
    // where 절
    func->data.func_decl.where_clause = parse_where_clause(parser);
    
    // 함수 본문
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' before function body");
    func->data.func_decl.body = parser_parse_block(parser);
    
    return func;
}

// let 선언 파싱
ASTNode* parser_parse_let_declaration(Parser* parser) {
    // 변수 이름
    Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected variable name");
    
    ASTNode* let_decl = ast_create_let_declaration(name.text);
    
    // 타입 어노테이션 (선택적)
    if (parser_match(parser, TOKEN_COLON)) {
        let_decl->data.let_decl.type = parse_type(parser);
    }
    
    // 초기화 표현식
    parser_consume(parser, TOKEN_ASSIGN, "Expected '=' in let declaration");
    let_decl->data.let_decl.initializer = parser_parse_expression(parser);
    
    parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after let declaration");
    
    return let_decl;
}

// with 문 파싱
ASTNode* parser_parse_with_statement(Parser* parser) {
    ASTNode* with_stmt = ast_create_with_statement();
    
    // 슬롯 타입
    if (parser_match(parser, TOKEN_IDENTIFIER)) {
        if (strcmp(parser->previous_token.text, "slot") == 0 ||
            strcmp(parser->previous_token.text, "SecureSlot") == 0) {
            with_stmt->data.with_stmt.is_secure = 
                strcmp(parser->previous_token.text, "SecureSlot") == 0;
        }
    }
    
    // 제네릭 타입
    parser_consume(parser, TOKEN_LESS, "Expected '<' after slot type");
    with_stmt->data.with_stmt.slot_type = parse_type(parser);
    parser_consume(parser, TOKEN_GREATER, "Expected '>' after slot type");
    
    // 보안 레벨 (선택적)
    if (parser_match(parser, TOKEN_LPAREN)) {
        Token level = parser_consume(parser, TOKEN_IDENTIFIER, "Expected security level");
        with_stmt->data.with_stmt.security_level = strdup(level.text);
        parser_consume(parser, TOKEN_RPAREN, "Expected ')' after security level");
    }
    
    // as 변수명
    parser_consume(parser, TOKEN_AS, "Expected 'as' in with statement");
    Token alias = parser_consume(parser, TOKEN_IDENTIFIER, "Expected variable name after 'as'");
    with_stmt->data.with_stmt.alias = strdup(alias.text);
    
    // 블록
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after with statement");
    parser->in_with_statement = true;
    with_stmt->data.with_stmt.body = parser_parse_block(parser);
    parser->in_with_statement = false;
    
    return with_stmt;
}

// parallel 블록 파싱
ASTNode* parser_parse_parallel_block(Parser* parser) {
    ASTNode* parallel = ast_create_parallel_block();
    
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after 'Parallel'");
    
    parser->in_parallel_block = true;
    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        ASTNode* stmt = parser_parse_statement(parser);
        if (stmt) {
            ast_add_parallel_task(parallel, stmt);
        }
    }
    parser->in_parallel_block = false;
    
    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after parallel block");
    
    return parallel;
}

// 블록 파싱
ASTNode* parser_parse_block(Parser* parser) {
    ASTNode* block = ast_create_block();
    
    parser->scope_depth++;
    
    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        ASTNode* stmt = parser_parse_statement(parser);
        if (stmt) {
            ast_add_statement(block, stmt);
        }
    }
    
    parser->scope_depth--;
    
    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after block");
    
    return block;
}

// ============= 표현식 파싱 =============

// 표현식 문장
ASTNode* parser_parse_expression_statement(Parser* parser) {
    ASTNode* expr = parser_parse_expression(parser);
    parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after expression");
    return expr;
}

// 표현식 파싱 (우선순위 기반)
ASTNode* parser_parse_expression(Parser* parser) {
    return parser_parse_assignment(parser);
}

// 할당 표현식
ASTNode* parser_parse_assignment(Parser* parser) {
    ASTNode* expr = parse_logical_or(parser);
    
    if (parser_match(parser, TOKEN_ASSIGN)) {
        ASTNode* value = parser_parse_assignment(parser);
        ASTNode* assign = ast_create_assignment(expr, value);
        return assign;
    }
    
    return expr;
}

// 논리 OR
static ASTNode* parse_logical_or(Parser* parser) {
    ASTNode* expr = parse_logical_and(parser);
    
    while (parser_match(parser, TOKEN_OR)) {
        Token op = parser->previous_token;
        ASTNode* right = parse_logical_and(parser);
        expr = ast_create_binary(expr, op, right);
    }
    
    return expr;
}

// 논리 AND
static ASTNode* parse_logical_and(Parser* parser) {
    ASTNode* expr = parse_equality(parser);
    
    while (parser_match(parser, TOKEN_AND)) {
        Token op = parser->previous_token;
        ASTNode* right = parse_equality(parser);
        expr = ast_create_binary(expr, op, right);
    }
    
    return expr;
}

// 동등성 비교
static ASTNode* parse_equality(Parser* parser) {
    ASTNode* expr = parse_comparison(parser);
    
    while (parser_match(parser, TOKEN_EQUAL) || 
           parser_match(parser, TOKEN_NOT_EQUAL)) {
        Token op = parser->previous_token;
        ASTNode* right = parse_comparison(parser);
        expr = ast_create_binary(expr, op, right);
    }
    
    return expr;
}

// 비교 연산
static ASTNode* parse_comparison(Parser* parser) {
    ASTNode* expr = parse_addition(parser);
    
    while (parser_match(parser, TOKEN_LESS) || 
           parser_match(parser, TOKEN_LESS_EQUAL) ||
           parser_match(parser, TOKEN_GREATER) || 
           parser_match(parser, TOKEN_GREATER_EQUAL)) {
        Token op = parser->previous_token;
        ASTNode* right = parse_addition(parser);
        expr = ast_create_binary(expr, op, right);
    }
    
    return expr;
}

// 덧셈/뺄셈
static ASTNode* parse_addition(Parser* parser) {
    ASTNode* expr = parse_multiplication(parser);
    
    while (parser_match(parser, TOKEN_PLUS) || 
           parser_match(parser, TOKEN_MINUS)) {
        Token op = parser->previous_token;
        ASTNode* right = parse_multiplication(parser);
        expr = ast_create_binary(expr, op, right);
    }
    
    return expr;
}

// 곱셈/나눗셈
static ASTNode* parse_multiplication(Parser* parser) {
    ASTNode* expr = parse_unary(parser);
    
    while (parser_match(parser, TOKEN_STAR) || 
           parser_match(parser, TOKEN_SLASH) ||
           parser_match(parser, TOKEN_PERCENT)) {
        Token op = parser->previous_token;
        ASTNode* right = parse_unary(parser);
        expr = ast_create_binary(expr, op, right);
    }
    
    return expr;
}

// 단항 연산
static ASTNode* parse_unary(Parser* parser) {
    if (parser_match(parser, TOKEN_NOT) || 
        parser_match(parser, TOKEN_MINUS)) {
        Token op = parser->previous_token;
        ASTNode* right = parse_unary(parser);
        return ast_create_unary(op, right);
    }
    
    return parser_parse_call(parser);
}

// 함수 호출 / 멤버 접근
ASTNode* parser_parse_call(Parser* parser) {
    ASTNode* expr = parser_parse_primary(parser);
    
    while (true) {
        if (parser_match(parser, TOKEN_LPAREN)) {
            // 함수 호출
            expr = finish_call(parser, expr);
        } else if (parser_match(parser, TOKEN_DOT)) {
            // 멤버 접근
            Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected property name after '.'");
            expr = ast_create_member_access(expr, name.text);
        } else if (parser_match(parser, TOKEN_LBRACKET)) {
            // 배열 인덱싱
            ASTNode* index = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_RBRACKET, "Expected ']' after array index");
            expr = ast_create_array_access(expr, index);
        } else {
            break;
        }
    }
    
    return expr;
}

// 함수 호출 완성
static ASTNode* finish_call(Parser* parser, ASTNode* callee) {
    ASTNode* call = ast_create_call(callee);
    
    // 인자 파싱
    if (!parser_check(parser, TOKEN_RPAREN)) {
        do {
            ASTNode* arg = parser_parse_expression(parser);
            ast_add_argument(call, arg);
        } while (parser_match(parser, TOKEN_COMMA));
    }
    
    parser_consume(parser, TOKEN_RPAREN, "Expected ')' after arguments");
    
    return call;
}

// 기본 표현식
ASTNode* parser_parse_primary(Parser* parser) {
    // true
    if (parser_match(parser, TOKEN_TRUE)) {
        return ast_create_boolean(true);
    }
    
    // false
    if (parser_match(parser, TOKEN_FALSE)) {
        return ast_create_boolean(false);
    }
    
    // 숫자
    if (parser_match(parser, TOKEN_NUMBER)) {
        return ast_create_number(parser->previous_token.text);
    }
    
    // 문자열
    if (parser_match(parser, TOKEN_STRING)) {
        return ast_create_string(parser->previous_token.text);
    }
    
    // 식별자 또는 슬롯 연산
    if (parser_match(parser, TOKEN_IDENTIFIER)) {
        char* name = strdup(parser->previous_token.text);
        
        // 내장 함수 처리
        if (strcmp(name, "ClaimSlot") == 0 || 
            strcmp(name, "ClaimSecureSlot") == 0 ||
            strcmp(name, "Write") == 0 ||
            strcmp(name, "Read") == 0 ||
            strcmp(name, "Release") == 0 ||
            strcmp(name, "Log") == 0 ||
            strcmp(name, "Parallel") == 0) {
            return ast_create_identifier(name);
        }
        
        return ast_create_identifier(name);
    }
    
    // 괄호 표현식
    if (parser_match(parser, TOKEN_LPAREN)) {
        ASTNode* expr = parser_parse_expression(parser);
        parser_consume(parser, TOKEN_RPAREN, "Expected ')' after expression");
        return expr;
    }
    
    parser_error(parser, "Unexpected token in expression");
    return NULL;
}

// 기타 파싱 함수들...

// for 루프 파싱
static ASTNode* parse_for_loop(Parser* parser) {
    ASTNode* for_loop = ast_create_for_loop();
    
    // 루프 변수
    Token var = parser_consume(parser, TOKEN_IDENTIFIER, "Expected loop variable");
    for_loop->data.for_loop.variable = strdup(var.text);
    
    parser_consume(parser, TOKEN_IN, "Expected 'in' in for loop");
    
    // 범위 표현식 (예: 1..10)
    ASTNode* start = parser_parse_expression(parser);
    parser_consume(parser, TOKEN_DOT, "Expected '..' in range");
    parser_consume(parser, TOKEN_DOT, "Expected '..' in range");
    ASTNode* end = parser_parse_expression(parser);
    
    for_loop->data.for_loop.range_start = start;
    for_loop->data.for_loop.range_end = end;
    
    // 루프 본문
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after for loop header");
    for_loop->data.for_loop.body = parser_parse_block(parser);
    
    return for_loop;
}

// if 문 파싱
static ASTNode* parse_if_statement(Parser* parser) {
    ASTNode* if_stmt = ast_create_if_statement();
    
    // 조건
    if_stmt->data.if_stmt.condition = parser_parse_expression(parser);
    
    // then 블록
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after if condition");
    if_stmt->data.if_stmt.then_branch = parser_parse_block(parser);
    
    // else 블록 (선택적)
    if (parser_match(parser, TOKEN_ELSE)) {
        parser_consume(parser, TOKEN_LBRACE, "Expected '{' after else");
        if_stmt->data.if_stmt.else_branch = parser_parse_block(parser);
    }
    
    return if_stmt;
}

// return 문 파싱
static ASTNode* parse_return_statement(Parser* parser) {
    ASTNode* return_stmt = ast_create_return_statement();
    
    if (!parser_check(parser, TOKEN_SEMICOLON)) {
        return_stmt->data.return_stmt.value = parser_parse_expression(parser);
    }
    
    parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after return statement");
    
    return return_stmt;
}

// 클래스 선언 파싱
static ASTNode* parse_class_declaration(Parser* parser) {
    // 클래스 이름
    Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected class name");
    
    ASTNode* class_decl = ast_create_class(name.text);
    
    // 제네릭 파라미터
    class_decl->data.class_decl.generic_params = parse_generic_params(parser);
    
    // where 절
    class_decl->data.class_decl.where_clause = parse_where_clause(parser);
    
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after class name");
    
    // 클래스 멤버들
    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        // 접근 제어자
        AccessModifier access = ACCESS_PRIVATE;
        if (parser_match(parser, TOKEN_PUBLIC)) {
            access = ACCESS_PUBLIC;
        } else if (parser_match(parser, TOKEN_PRIVATE)) {
            access = ACCESS_PRIVATE;
        }
        
        // 필드 또는 메서드
        if (parser_check(parser, TOKEN_LET)) {
            parser_advance(parser);
            // 필드
            Token field_name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected field name");
            parser_consume(parser, TOKEN_COLON, "Expected ':' after field name");
            ASTNode* field_type = parse_type(parser);
            
            ClassField* field = calloc(1, sizeof(ClassField));
            field->name = strdup(field_name.text);
            field->type = field_type;
            field->access = access;
            
            // 필드 추가
            class_decl->data.class_decl.field_count++;
            class_decl->data.class_decl.fields = realloc(
                class_decl->data.class_decl.fields,
                class_decl->data.class_decl.field_count * sizeof(ClassField*)
            );
            class_decl->data.class_decl.fields[class_decl->data.class_decl.field_count - 1] = field;
            
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after field declaration");
        } else if (parser_match(parser, TOKEN_FUNC)) {
            // 메서드
            ASTNode* method = parse_function_declaration(parser);
            method->data.func_decl.access = access;
            
            // 메서드 추가
            class_decl->data.class_decl.method_count++;
            class_decl->data.class_decl.methods = realloc(
                class_decl->data.class_decl.methods,
                class_decl->data.class_decl.method_count * sizeof(ASTNode*)
            );
            class_decl->data.class_decl.methods[class_decl->data.class_decl.method_count - 1] = method;
        }
    }
    
    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after class body");
    
    return class_decl;
}

// 타입 제약조건 파싱
static ASTNode* parse_type_constraint(Parser* parser) {
    // 단순 버전 - 향후 확장 필요
    return parse_type(parser);
}
