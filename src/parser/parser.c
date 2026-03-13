/*
 * Copyright (c) 2025 Pergyra Language Project
 * Parser implementation with generic-first design
 */

#include "parser.h"
#include "ast.h"
#include "../common/string_compat.h"
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
static ASTNode* parse_struct_declaration(Parser* parser);
static ASTNode* parse_type_declaration(Parser* parser, bool is_struct);
static ASTNode* parse_extern_block(Parser* parser);
static ASTNode* parse_for_loop(Parser* parser);
static ASTNode* parse_while_statement(Parser* parser);
static ASTNode* parse_match_statement(Parser* parser);
static ASTNode* parse_if_statement(Parser* parser);
static ASTNode* parse_return_statement(Parser* parser);
static ASTNode* parse_type_constraint(Parser* parser);
static void skip_generic_arguments(Parser* parser);
static Token consume_name_token(Parser* parser, const char* message);

/* Role/Ability system parsing functions */
static ASTNode* parse_ability_declaration(Parser* parser);
static ASTNode* parse_role_declaration(Parser* parser);

/* Party system parsing functions */
static ASTNode* parse_party_declaration(Parser* parser);

/* Systemic/World system parsing functions */
static ASTNode* parse_systemic_declaration(Parser* parser);
static ASTNode* parse_world_declaration(Parser* parser);

/* Event system parsing functions */
static ASTNode* parse_event_declaration(Parser* parser);
static ASTNode* parse_lambda_expression(Parser* parser);

// 파서 생성
Parser* parser_create(Lexer* lexer) {
    Parser* parser = calloc(1, sizeof(Parser));
    if (!parser) return NULL;
    
    parser->lexer = lexer;
    parser->has_error = false;
    parser->scope_depth = 0;
    parser->in_parallel_block = false;
    parser->in_with_statement = false;
    parser->in_extern_block = false;
    
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

static Token
consume_name_token(Parser* parser, const char* message)
{
    if (parser_check(parser, TOKEN_IDENTIFIER) || parser_check(parser, TOKEN_SLOT))
        return parser_advance(parser);
    return parser_consume(parser, TOKEN_IDENTIFIER, message);
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
            case TOKEN_STRUCT:
            case TOKEN_EXTERN:
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
        param->name = pergyra_strdup(name.text);
        
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
        constraint->type_param = pergyra_strdup(param.text);
        
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
    // async 함수 선언
    if (parser_match(parser, TOKEN_ASYNC)) {
        return parser_parse_async_function(parser);
    }
    
    // actor 선언
    if (parser_match(parser, TOKEN_ACTOR)) {
        return parser_parse_actor_declaration(parser);
    }
    
    // select 문
    if (parser_match(parser, TOKEN_SELECT)) {
        return parser_parse_select_statement(parser);
    }
    
    // 함수 선언
    if (parser_match(parser, TOKEN_FUNC)) {
        return parse_function_declaration(parser);
    }

    // extern 블록
    if (parser_match(parser, TOKEN_EXTERN)) {
        return parse_extern_block(parser);
    }
    
    // 클래스 선언
    if (parser_match(parser, TOKEN_CLASS)) {
        return parse_class_declaration(parser);
    }

    // 구조체 선언
    if (parser_match(parser, TOKEN_STRUCT)) {
        return parse_struct_declaration(parser);
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
    
    // while 루프
    if (parser_match(parser, TOKEN_WHILE)) {
        return parse_while_statement(parser);
    }

    // match 문
    if (parser_match(parser, TOKEN_MATCH)) {
        return parse_match_statement(parser);
    }

    // if 문
    if (parser_match(parser, TOKEN_IF)) {
        return parse_if_statement(parser);
    }
    
    // return 문
    if (parser_match(parser, TOKEN_RETURN)) {
        return parse_return_statement(parser);
    }

    // systemic 선언
    if (parser_match(parser, TOKEN_SYSTEMIC)) {
        return parse_systemic_declaration(parser);
    }

    // world 선언
    if (parser_match(parser, TOKEN_WORLD)) {
        return parse_world_declaration(parser);
    }

    // party 선언
    if (parser_match(parser, TOKEN_PARTY)) {
        return parse_party_declaration(parser);
    }

    // ability 선언
    if (parser_match(parser, TOKEN_ABILITY)) {
        return parse_ability_declaration(parser);
    }

    // role 선언
    if (parser_match(parser, TOKEN_ROLE)) {
        return parse_role_declaration(parser);
    }

    // event 선언
    if (parser_match(parser, TOKEN_EVENT)) {
        return parse_event_declaration(parser);
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

        FuncParam* param = calloc(1, sizeof(FuncParam));
        param->name = pergyra_strdup(param_name.text);

        // self 파라미터: no type annotation needed
        if (strcmp(param_name.text, "self") == 0
            && !parser_check(parser, TOKEN_COLON)) {
            param->type = NULL; // self type resolved by codegen
        } else {
            parser_consume(parser, TOKEN_COLON, "Expected ':' after parameter name");
            ASTNode* param_type = parse_type(parser);
            param->type = param_type;
        }
        
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

    if (parser->in_extern_block) {
        parser_consume(parser, TOKEN_SEMICOLON,
            "Expected ';' after extern function declaration");
        return func;
    }

    // Abstract/declaration-only method (ends with ';' instead of '{')
    if (parser_match(parser, TOKEN_SEMICOLON)) {
        func->data.func_decl.body = NULL;
        return func;
    }

    // 함수 본문
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' before function body");
    func->data.func_decl.body = parser_parse_block(parser);
    
    return func;
}

// let 선언 파싱
ASTNode* parser_parse_let_declaration(Parser* parser) {
    // 변수 이름
    Token name = consume_name_token(parser, "Expected variable name");
    
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
    if (parser_match(parser, TOKEN_SLOT)) {
        with_stmt->data.with_stmt.is_secure = false;
    } else {
        Token slot_kind = parser_consume(
            parser,
            TOKEN_IDENTIFIER,
            "Expected 'slot' or 'SecureSlot' after 'with'"
        );

        if (strcmp(slot_kind.text, "SecureSlot") == 0) {
            with_stmt->data.with_stmt.is_secure = true;
        } else if (strcmp(slot_kind.text, "slot") != 0) {
            parser_error(parser, "Expected 'slot' or 'SecureSlot' after 'with'");
        }
    }
    
    // 제네릭 타입
    parser_consume(parser, TOKEN_LESS, "Expected '<' after slot type");
    with_stmt->data.with_stmt.slot_type = parse_type(parser);
    parser_consume(parser, TOKEN_GREATER, "Expected '>' after slot type");
    
    // 보안 레벨 (선택적)
    if (parser_match(parser, TOKEN_LPAREN)) {
        Token level = parser_consume(parser, TOKEN_IDENTIFIER, "Expected security level");
        with_stmt->data.with_stmt.security_level = pergyra_strdup(level.text);
        parser_consume(parser, TOKEN_RPAREN, "Expected ')' after security level");
    }
    
    // as 변수명
    parser_consume(parser, TOKEN_AS, "Expected 'as' in with statement");
    Token alias = parser_consume(parser, TOKEN_IDENTIFIER, "Expected variable name after 'as'");
    with_stmt->data.with_stmt.alias = pergyra_strdup(alias.text);
    
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
    
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after 'parallel'");
    
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

    /* Event subscription (+=) and unsubscription (-=) */
    if (parser_match(parser, TOKEN_SUBSCRIBE)) {
        ASTNode* handler = parser_parse_assignment(parser);
        return ast_create_event_subscribe(expr, handler);
    }

    if (parser_match(parser, TOKEN_UNSUBSCRIBE)) {
        ASTNode* handler = parser_parse_assignment(parser);
        return ast_create_event_unsubscribe(expr, handler);
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
        } else if (parser_check(parser, TOKEN_DOT) &&
                   parser->current_token.length == 1 &&
                   strcmp(parser->current_token.text, ".") == 0) {
            parser_advance(parser);
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
    // await 표현식
    if (parser_match(parser, TOKEN_AWAIT)) {
        return parser_parse_await_expression(parser);
    }
    
    // spawn 표현식
    if (parser_match(parser, TOKEN_SPAWN)) {
        return parser_parse_spawn_expression(parser);
    }

    // parallel 블록은 초기화 식에서도 쓰인다.
    if (parser_match(parser, TOKEN_PARALLEL)) {
        return parser_parse_parallel_block(parser);
    }
    
    // 채널 수신: <-channel
    if (parser_check(parser, TOKEN_CHANNEL_OP)) {
        return parser_parse_channel_expression(parser);
    }
    
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
    if (parser_match(parser, TOKEN_IDENTIFIER) || parser_match(parser, TOKEN_SLOT)) {
        char* name = pergyra_strdup(parser->previous_token.text);

        if ((strcmp(name, "ClaimSlot") == 0 ||
             strcmp(name, "ClaimSecureSlot") == 0) &&
            parser_check(parser, TOKEN_LESS)) {
            skip_generic_arguments(parser);
        }
        
        // 내장 함수 처리
        if (strcmp(name, "ClaimSlot") == 0 || 
            strcmp(name, "ClaimSecureSlot") == 0 ||
            strcmp(name, "Write") == 0 ||
            strcmp(name, "Read") == 0 ||
            strcmp(name, "Release") == 0 ||
            strcmp(name, "Log") == 0 ||
            strcmp(name, "Channel") == 0) {
            ASTNode* ident = ast_create_identifier(name);
            free(name);
            return ident;
        }
        
        // 채널 송신 체크: channel <- value
        ASTNode* ident = ast_create_identifier(name);
        free(name);
        if (parser_check(parser, TOKEN_CHANNEL_OP)) {
            parser_advance(parser);
            ASTNode* value = parser_parse_expression(parser);
            return ast_create_channel_send(ident, value);
        }
        
        return ident;
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
    for_loop->data.for_loop.variable = pergyra_strdup(var.text);
    
    parser_consume(parser, TOKEN_IN, "Expected 'in' in for loop");
    
    // 범위 표현식 (예: 1..10)
    ASTNode* start = parser_parse_expression(parser);
    Token range = parser_consume(parser, TOKEN_DOT, "Expected '..' in range");
    if (range.length != 2 || strcmp(range.text, "..") != 0) {
        parser_error(parser, "Expected '..' in range");
    }
    ASTNode* end = parser_parse_expression(parser);
    
    for_loop->data.for_loop.range_start = start;
    for_loop->data.for_loop.range_end = end;
    
    // 루프 본문
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after for loop header");
    for_loop->data.for_loop.body = parser_parse_block(parser);
    
    return for_loop;
}

// while 문 파싱
static ASTNode* parse_while_statement(Parser* parser) {
    ASTNode* while_loop = ast_create_while_loop();
    while_loop->line = parser->previous_token.line;
    while_loop->column = parser->previous_token.column;

    // 조건식
    while_loop->data.while_loop.condition = parser_parse_expression(parser);

    // 본문
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after while condition");
    while_loop->data.while_loop.body = parser_parse_block(parser);

    return while_loop;
}

// match 문 파싱
static ASTNode* parse_match_statement(Parser* parser) {
    ASTNode* match = ast_create_match_statement();
    match->line = parser->previous_token.line;
    match->column = parser->previous_token.column;

    // subject 표현식
    match->data.match_stmt.subject = parser_parse_expression(parser);

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after match expression");

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        if (parser_match(parser, TOKEN_CASE)) {
            ASTNode* mc = ast_create_match_case();
            mc->line = parser->previous_token.line;
            mc->column = parser->previous_token.column;

            // 패턴: 리터럴 또는 식별자
            mc->data.match_case.pattern = parser_parse_expression(parser);

            // guard (선택적)
            if (parser_match(parser, TOKEN_IF)) {
                mc->data.match_case.guard = parser_parse_expression(parser);
            }

            parser_consume(parser, TOKEN_COLON, "Expected ':' after case pattern");

            // 본문: 다음 case/default/} 전까지
            ASTNode* body = ast_create_block();
            while (!parser_check(parser, TOKEN_CASE) &&
                   !parser_check(parser, TOKEN_DEFAULT) &&
                   !parser_check(parser, TOKEN_RBRACE) &&
                   !parser_is_at_end(parser)) {
                ASTNode* stmt = parser_parse_statement(parser);
                if (stmt) ast_add_statement(body, stmt);
            }
            mc->data.match_case.body = body;

            // case를 match에 추가
            match->data.match_stmt.case_count++;
            match->data.match_stmt.cases = realloc(
                match->data.match_stmt.cases,
                sizeof(ASTNode*) * match->data.match_stmt.case_count);
            match->data.match_stmt.cases[match->data.match_stmt.case_count - 1] = mc;

        } else if (parser_match(parser, TOKEN_DEFAULT)) {
            parser_consume(parser, TOKEN_COLON, "Expected ':' after default");

            ASTNode* body = ast_create_block();
            while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
                ASTNode* stmt = parser_parse_statement(parser);
                if (stmt) ast_add_statement(body, stmt);
            }
            match->data.match_stmt.default_body = body;
        } else {
            parser_error(parser, "Expected 'case' or 'default' in match");
            parser_advance(parser);
        }
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after match body");
    return match;
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
    return parse_type_declaration(parser, false);
}

// 구조체 선언 파싱
static ASTNode* parse_struct_declaration(Parser* parser) {
    return parse_type_declaration(parser, true);
}

// 클래스/구조체 선언 공통 파싱
static ASTNode* parse_type_declaration(Parser* parser, bool is_struct) {
    // 클래스 이름
    Token name = parser_consume(parser, TOKEN_IDENTIFIER,
        is_struct ? "Expected struct name" : "Expected class name");

    ASTNode* class_decl = is_struct ? ast_create_struct(name.text)
                                    : ast_create_class(name.text);
    
    // 제네릭 파라미터
    class_decl->data.class_decl.generic_params = parse_generic_params(parser);
    
    // where 절
    class_decl->data.class_decl.where_clause = parse_where_clause(parser);
    
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after class name");
    
    // 클래스 멤버들
    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        // 접근 제어자
        AccessModifier access = is_struct ? ACCESS_PUBLIC : ACCESS_PRIVATE;
        if (parser_match(parser, TOKEN_PUBLIC)) {
            access = ACCESS_PUBLIC;
        } else if (parser_match(parser, TOKEN_PRIVATE)) {
            access = ACCESS_PRIVATE;
        }
        
        // 클래스 필드 또는 구조체 bare field / let field
        if (parser_check(parser, TOKEN_LET) ||
            (is_struct && parser_check(parser, TOKEN_IDENTIFIER))) {
            bool has_let = parser_match(parser, TOKEN_LET);
            if (!has_let && !is_struct) {
                parser_error(parser, "Expected field or method declaration");
                return class_decl;
            }

            // 필드
            Token field_name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected field name");
            parser_consume(parser, TOKEN_COLON, "Expected ':' after field name");
            ASTNode* field_type = parse_type(parser);

            ClassField* field = calloc(1, sizeof(ClassField));
            field->name = pergyra_strdup(field_name.text);
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
        } else {
            parser_error(parser, "Expected %s member declaration",
                is_struct ? "struct" : "class");
            return class_decl;
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

static ASTNode* parse_extern_block(Parser* parser) {
    Token abi = parser_consume(parser, TOKEN_STRING,
        "Expected ABI string after extern");
    if (abi.length < 2) {
        parser_error(parser, "Invalid ABI string");
        return NULL;
    }

    char* abi_name = pergyra_strndup(abi.text + 1, abi.length - 2);
    ASTNode* block = ast_create_extern_block(abi_name);
    free(abi_name);
    if (!block) return NULL;

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after extern ABI");

    bool prev_extern = parser->in_extern_block;
    parser->in_extern_block = true;

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        parser_consume(parser, TOKEN_FUNC,
            "Expected 'func' declaration inside extern block");
        ASTNode* decl = parse_function_declaration(parser);
        if (decl) {
            ast_add_statement(block, decl);
        }
    }

    parser->in_extern_block = prev_extern;
    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after extern block");
    return block;
}

/* =================================================================
 * Systemic/World system parsing functions
 * ================================================================= */

/*
 * systemic CombatSystem {
 *     party slot team1: DungeonTeam
 *     party slot team2: DungeonTeam
 *     shared rules: CombatRules
 *     func ScheduleMatches() -> Void { ... }
 * }
 */
static ASTNode* parse_systemic_declaration(Parser* parser) {
    Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected systemic name");
    ASTNode* sys = ast_create_systemic_declaration(name.text);
    sys->line = name.line;
    sys->column = name.column;

    sys->data.systemic_decl.generic_params = parse_generic_params(parser);

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after systemic name");

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        if (parser_match(parser, TOKEN_PARTY)) {
            /* party slot name: PartyType */
            parser_consume(parser, TOKEN_SLOT,
                "Expected 'slot' after 'party' in systemic");
            Token slot_name = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected slot name");
            parser_consume(parser, TOKEN_COLON,
                "Expected ':' after party slot name");
            Token party_type = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected party type");

            ASTNode* ps = ast_create_systemic_slot(slot_name.text, party_type.text);
            ps->line = slot_name.line;
            ps->column = slot_name.column;

            sys->data.systemic_decl.party_count++;
            sys->data.systemic_decl.party_slots = realloc(
                sys->data.systemic_decl.party_slots,
                sys->data.systemic_decl.party_count * sizeof(ASTNode*));
            sys->data.systemic_decl.party_slots[
                sys->data.systemic_decl.party_count - 1] = ps;

            parser_match(parser, TOKEN_SEMICOLON);

        } else if (parser_match(parser, TOKEN_SHARED)) {
            /* shared field_name: Type = init */
            Token field_name = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected field name after 'shared'");
            parser_consume(parser, TOKEN_COLON,
                "Expected ':' after shared field name");
            ASTNode* field_type = parse_type(parser);

            ASTNode* shared = ast_create_party_shared(field_name.text);
            shared->data.party_shared.type = field_type;
            shared->line = field_name.line;
            shared->column = field_name.column;

            if (parser_match(parser, TOKEN_ASSIGN)) {
                shared->data.party_shared.initializer =
                    parser_parse_expression(parser);
            }

            sys->data.systemic_decl.shared_count++;
            sys->data.systemic_decl.shared_fields = realloc(
                sys->data.systemic_decl.shared_fields,
                sys->data.systemic_decl.shared_count * sizeof(ASTNode*));
            sys->data.systemic_decl.shared_fields[
                sys->data.systemic_decl.shared_count - 1] = shared;

            parser_match(parser, TOKEN_SEMICOLON);

        } else if (parser_match(parser, TOKEN_FUNC)) {
            ASTNode* method = parse_function_declaration(parser);

            sys->data.systemic_decl.method_count++;
            sys->data.systemic_decl.methods = realloc(
                sys->data.systemic_decl.methods,
                sys->data.systemic_decl.method_count * sizeof(ASTNode*));
            sys->data.systemic_decl.methods[
                sys->data.systemic_decl.method_count - 1] = method;

        } else {
            parser_error(parser,
                "Expected 'party slot', 'shared', or 'func' in systemic body");
            parser_advance(parser);
        }
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after systemic body");
    return sys;
}

/*
 * world GameWorld {
 *     systemic combat: CombatSystem
 *     systemic economy: EconomySystem
 *     shared tick: Int = 0
 *     func Update() -> Void { ... }
 * }
 */
static ASTNode* parse_world_declaration(Parser* parser) {
    Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected world name");
    ASTNode* world = ast_create_world_declaration(name.text);
    world->line = name.line;
    world->column = name.column;

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after world name");

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        if (parser_match(parser, TOKEN_SYSTEMIC)) {
            /* systemic name: SystemicType */
            Token slot_name = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected systemic name");
            parser_consume(parser, TOKEN_COLON,
                "Expected ':' after systemic name");
            Token sys_type = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected systemic type");

            ASTNode* ws = ast_create_world_systemic(
                slot_name.text, sys_type.text);
            ws->line = slot_name.line;
            ws->column = slot_name.column;

            world->data.world_decl.systemic_count++;
            world->data.world_decl.systemics = realloc(
                world->data.world_decl.systemics,
                world->data.world_decl.systemic_count * sizeof(ASTNode*));
            world->data.world_decl.systemics[
                world->data.world_decl.systemic_count - 1] = ws;

            parser_match(parser, TOKEN_SEMICOLON);

        } else if (parser_match(parser, TOKEN_SHARED)) {
            Token field_name = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected field name after 'shared'");
            parser_consume(parser, TOKEN_COLON,
                "Expected ':' after shared field name");
            ASTNode* field_type = parse_type(parser);

            ASTNode* shared = ast_create_party_shared(field_name.text);
            shared->data.party_shared.type = field_type;
            shared->line = field_name.line;
            shared->column = field_name.column;

            if (parser_match(parser, TOKEN_ASSIGN)) {
                shared->data.party_shared.initializer =
                    parser_parse_expression(parser);
            }

            world->data.world_decl.shared_count++;
            world->data.world_decl.shared_fields = realloc(
                world->data.world_decl.shared_fields,
                world->data.world_decl.shared_count * sizeof(ASTNode*));
            world->data.world_decl.shared_fields[
                world->data.world_decl.shared_count - 1] = shared;

            parser_match(parser, TOKEN_SEMICOLON);

        } else if (parser_match(parser, TOKEN_FUNC)) {
            ASTNode* method = parse_function_declaration(parser);

            world->data.world_decl.method_count++;
            world->data.world_decl.methods = realloc(
                world->data.world_decl.methods,
                world->data.world_decl.method_count * sizeof(ASTNode*));
            world->data.world_decl.methods[
                world->data.world_decl.method_count - 1] = method;

        } else {
            parser_error(parser,
                "Expected 'systemic', 'shared', or 'func' in world body");
            parser_advance(parser);
        }
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after world body");
    return world;
}

/* =================================================================
 * Party system parsing functions
 * ================================================================= */

/*
 * party HolyPaladin extends BaseParty {
 *     role slot tank: Damageable & Taunting
 *     role slot healer: Healing
 *     shared formation: String = "standard"
 *     func Execute() -> Void { ... }
 * }
 */
static ASTNode* parse_party_declaration(Parser* parser) {
    Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected party name");
    ASTNode* party = ast_create_party_declaration(name.text);
    party->line = name.line;
    party->column = name.column;

    /* Optional generic params */
    party->data.party_decl.generic_params = parse_generic_params(parser);

    /* Optional extends */
    if (parser_match(parser, TOKEN_EXTENDS)) {
        party->data.party_decl.extends = parse_type(parser);
    }

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after party header");

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        if (parser_match(parser, TOKEN_ROLE)) {
            /* role slot name: AbilityType & AbilityType */
            parser_consume(parser, TOKEN_SLOT,
                "Expected 'slot' after 'role' in party");
            Token slot_name = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected slot name");
            parser_consume(parser, TOKEN_COLON,
                "Expected ':' after role slot name");

            ASTNode* rs = ast_create_role_slot(slot_name.text);
            rs->line = slot_name.line;
            rs->column = slot_name.column;

            /* Parse ability types separated by & */
            do {
                ASTNode* ability_type = parse_type(parser);
                rs->data.role_slot.ability_count++;
                rs->data.role_slot.required_abilities = realloc(
                    rs->data.role_slot.required_abilities,
                    rs->data.role_slot.ability_count * sizeof(ASTNode*));
                rs->data.role_slot.required_abilities[
                    rs->data.role_slot.ability_count - 1] = ability_type;
            } while (parser_match(parser, TOKEN_AND));

            party->data.party_decl.role_count++;
            party->data.party_decl.role_slots = realloc(
                party->data.party_decl.role_slots,
                party->data.party_decl.role_count * sizeof(ASTNode*));
            party->data.party_decl.role_slots[
                party->data.party_decl.role_count - 1] = rs;

            parser_match(parser, TOKEN_SEMICOLON);

        } else if (parser_match(parser, TOKEN_SHARED)) {
            /* shared field_name: Type = initializer */
            Token field_name = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected field name after 'shared'");
            parser_consume(parser, TOKEN_COLON,
                "Expected ':' after shared field name");
            ASTNode* field_type = parse_type(parser);

            ASTNode* shared = ast_create_party_shared(field_name.text);
            shared->data.party_shared.type = field_type;
            shared->line = field_name.line;
            shared->column = field_name.column;

            if (parser_match(parser, TOKEN_ASSIGN)) {
                shared->data.party_shared.initializer =
                    parser_parse_expression(parser);
            }

            party->data.party_decl.shared_count++;
            party->data.party_decl.shared_fields = realloc(
                party->data.party_decl.shared_fields,
                party->data.party_decl.shared_count * sizeof(ASTNode*));
            party->data.party_decl.shared_fields[
                party->data.party_decl.shared_count - 1] = shared;

            parser_match(parser, TOKEN_SEMICOLON);

        } else if (parser_match(parser, TOKEN_FUNC)) {
            /* Party method */
            ASTNode* method = parse_function_declaration(parser);

            party->data.party_decl.method_count++;
            party->data.party_decl.methods = realloc(
                party->data.party_decl.methods,
                party->data.party_decl.method_count * sizeof(ASTNode*));
            party->data.party_decl.methods[
                party->data.party_decl.method_count - 1] = method;

        } else {
            parser_error(parser,
                "Expected 'role slot', 'shared', or 'func' in party body");
            parser_advance(parser);
        }
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after party body");
    return party;
}

/* =================================================================
 * Role/Ability system parsing functions
 * ================================================================= */

/*
 * ability Damageable {
 *     require health: Int
 *     func TakeDamage(amount: Int) -> Void
 *     func GetHealth() -> Int { return self.health; }
 * }
 */
static ASTNode* parse_ability_declaration(Parser* parser) {
    Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected ability name");
    ASTNode* ability = ast_create_ability_declaration(name.text);
    ability->line = name.line;
    ability->column = name.column;

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after ability name");

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        if (parser_match(parser, TOKEN_REQUIRE)) {
            /* require field_name: Type */
            Token field_name = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected field name after 'require'");
            parser_consume(parser, TOKEN_COLON, "Expected ':' after require field name");
            ASTNode* field_type = parse_type(parser);

            ASTNode* req = ast_create_require_field(field_name.text);
            req->data.require_field.type = field_type;
            req->line = field_name.line;
            req->column = field_name.column;

            ability->data.ability_decl.require_count++;
            ability->data.ability_decl.require_fields = realloc(
                ability->data.ability_decl.require_fields,
                ability->data.ability_decl.require_count * sizeof(ASTNode*));
            ability->data.ability_decl.require_fields[
                ability->data.ability_decl.require_count - 1] = req;

            /* Optional semicolon */
            parser_match(parser, TOKEN_SEMICOLON);

        } else if (parser_match(parser, TOKEN_FUNC)) {
            /* Method declaration (may have body or be abstract) */
            ASTNode* method = parse_function_declaration(parser);

            ability->data.ability_decl.method_count++;
            ability->data.ability_decl.methods = realloc(
                ability->data.ability_decl.methods,
                ability->data.ability_decl.method_count * sizeof(ASTNode*));
            ability->data.ability_decl.methods[
                ability->data.ability_decl.method_count - 1] = method;

        } else {
            parser_error(parser, "Expected 'require' or 'func' in ability body");
            parser_advance(parser);
        }
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after ability body");
    return ability;
}

/*
 * role PlayerDamageable for Player {
 *     include role BuffableRole<Int>
 *     impl ability Damageable {
 *         func TakeDamage(amount: Int) -> Void { ... }
 *     }
 *     override func GetHealth() -> Int { super.GetHealth() + bonus; }
 * }
 */
static ASTNode* parse_role_declaration(Parser* parser) {
    Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected role name");
    ASTNode* role = ast_create_role_declaration(name.text);
    role->line = name.line;
    role->column = name.column;

    /* Optional generic params */
    role->data.role_decl.generic_params = parse_generic_params(parser);

    /* 'for' TargetType (reuse TOKEN_FOR) */
    if (parser_match(parser, TOKEN_FOR)) {
        role->data.role_decl.for_type = parse_type(parser);
    }

    /* Optional where clause */
    role->data.role_decl.where_clause = parse_where_clause(parser);

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after role header");

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        if (parser_match(parser, TOKEN_INCLUDE)) {
            /* include role RoleName<T> */
            parser_match(parser, TOKEN_ROLE); /* optional 'role' keyword */
            Token role_name = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected role name after 'include'");
            ASTNode* inc = ast_create_include_statement(role_name.text);
            inc->line = role_name.line;
            inc->column = role_name.column;
            /* Optional generic args */
            inc->data.include_stmt.type_args = parse_generic_params(parser);

            role->data.role_decl.include_count++;
            role->data.role_decl.includes = realloc(
                role->data.role_decl.includes,
                role->data.role_decl.include_count * sizeof(ASTNode*));
            role->data.role_decl.includes[
                role->data.role_decl.include_count - 1] = inc;

            parser_match(parser, TOKEN_SEMICOLON);

        } else if (parser_match(parser, TOKEN_IMPL)) {
            /* impl ability AbilityName { ... } */
            parser_match(parser, TOKEN_ABILITY); /* optional 'ability' keyword */
            Token ability_name = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected ability name after 'impl'");
            ASTNode* impl = ast_create_impl_ability(ability_name.text);
            impl->line = ability_name.line;
            impl->column = ability_name.column;

            parser_consume(parser, TOKEN_LBRACE,
                "Expected '{' after impl ability name");

            while (!parser_check(parser, TOKEN_RBRACE)
                   && !parser_is_at_end(parser)) {
                if (parser_match(parser, TOKEN_FUNC)) {
                    ASTNode* method = parse_function_declaration(parser);
                    impl->data.impl_ability.method_count++;
                    impl->data.impl_ability.methods = realloc(
                        impl->data.impl_ability.methods,
                        impl->data.impl_ability.method_count * sizeof(ASTNode*));
                    impl->data.impl_ability.methods[
                        impl->data.impl_ability.method_count - 1] = method;
                } else {
                    parser_error(parser,
                        "Expected 'func' in impl ability body");
                    parser_advance(parser);
                }
            }
            parser_consume(parser, TOKEN_RBRACE,
                "Expected '}' after impl ability body");

            role->data.role_decl.impl_count++;
            role->data.role_decl.impl_abilities = realloc(
                role->data.role_decl.impl_abilities,
                role->data.role_decl.impl_count * sizeof(ASTNode*));
            role->data.role_decl.impl_abilities[
                role->data.role_decl.impl_count - 1] = impl;

        } else if (parser_match(parser, TOKEN_OVERRIDE)) {
            /* override func FuncName(...) { ... } */
            parser_consume(parser, TOKEN_FUNC,
                "Expected 'func' after 'override'");
            ASTNode* func = parse_function_declaration(parser);
            ASTNode* ovr = ast_create_override_func(func);
            ovr->line = func->line;
            ovr->column = func->column;

            /* Check if body contains 'super' calls — simple heuristic */
            ovr->data.override_func.calls_super = false;

            /* Add as an impl with special name "__override__" */
            role->data.role_decl.impl_count++;
            role->data.role_decl.impl_abilities = realloc(
                role->data.role_decl.impl_abilities,
                role->data.role_decl.impl_count * sizeof(ASTNode*));
            role->data.role_decl.impl_abilities[
                role->data.role_decl.impl_count - 1] = ovr;

        } else if (parser_match(parser, TOKEN_FUNC)) {
            /* Direct method in role (not in impl block) */
            ASTNode* method = parse_function_declaration(parser);

            /* Wrap as impl with no ability name (role's own method) */
            ASTNode* impl = ast_create_impl_ability(NULL);
            impl->data.impl_ability.method_count = 1;
            impl->data.impl_ability.methods = calloc(1, sizeof(ASTNode*));
            impl->data.impl_ability.methods[0] = method;

            role->data.role_decl.impl_count++;
            role->data.role_decl.impl_abilities = realloc(
                role->data.role_decl.impl_abilities,
                role->data.role_decl.impl_count * sizeof(ASTNode*));
            role->data.role_decl.impl_abilities[
                role->data.role_decl.impl_count - 1] = impl;

        } else {
            parser_error(parser,
                "Expected 'include', 'impl', 'override', or 'func' in role body");
            parser_advance(parser);
        }
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after role body");
    return role;
}

/* =================================================================
 * Event system parsing functions
 * ================================================================= */

// 이벤트 선언 파싱: event OnClick(sender: Object, args: EventArgs);
static ASTNode* parse_event_declaration(Parser* parser) {
    // 이벤트 이름
    Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected event name");
    
    ASTNode* event_decl = ast_create_event_declaration(name.text);
    
    // 접근 제어자 (선택적)
    if (parser_match(parser, TOKEN_PUBLIC)) {
        event_decl->data.event_decl.access = ACCESS_PUBLIC;
    } else if (parser_match(parser, TOKEN_PRIVATE)) {
        event_decl->data.event_decl.access = ACCESS_PRIVATE;
    }
    
    // 파라미터 파싱
    parser_consume(parser, TOKEN_LPAREN, "Expected '(' after event name");
    
    while (!parser_check(parser, TOKEN_RPAREN) && !parser_is_at_end(parser)) {
        // 파라미터 이름
        Token param_name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected parameter name");
        parser_consume(parser, TOKEN_COLON, "Expected ':' after parameter name");
        
        // 파라미터 타입
        ASTNode* param_type = parse_type(parser);
        
        // 파라미터 추가
        event_decl->data.event_decl.param_count++;
        event_decl->data.event_decl.params = realloc(
            event_decl->data.event_decl.params,
            event_decl->data.event_decl.param_count * sizeof(ASTNode*)
        );
        
        // 파라미터 노드 생성 (let decl 와 유사)
        ASTNode* param = ast_create_let_declaration(param_name.text);
        param->data.let_decl.type = param_type;
        event_decl->data.event_decl.params[event_decl->data.event_decl.param_count - 1] = param;
        
        if (!parser_match(parser, TOKEN_COMMA)) break;
    }
    
    parser_consume(parser, TOKEN_RPAREN, "Expected ')' after event parameters");
    
    // 반환 타입 (선택적, 기본은 Void)
    if (parser_match(parser, TOKEN_ARROW)) {
        event_decl->data.event_decl.return_type = parse_type(parser);
    }
    
    parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after event declaration");
    
    return event_decl;
}

// 람다식 파싱: (x, y) => x + y
#if defined(__GNUC__)
__attribute__((unused))
#endif
static ASTNode* parse_lambda_expression(Parser* parser) {
    ASTNode* lambda = ast_create_lambda_expression();
    
    // 파라미터 파싱
    parser_consume(parser, TOKEN_LPAREN, "Expected '(' before lambda parameters");
    
    while (!parser_check(parser, TOKEN_RPAREN) && !parser_is_at_end(parser)) {
        Token param_name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected parameter name");
        
        ASTNode* param = ast_create_identifier(param_name.text);
        
        // 타입 어노테이션 (선택적)
        if (parser_match(parser, TOKEN_COLON)) {
            ASTNode* param_type = parse_type(parser);
            // 타입 정보를 param 에 첨부 (향후 확장)
            (void)param_type;
        }
        
        lambda->data.lambda_expr.param_count++;
        lambda->data.lambda_expr.params = realloc(
            lambda->data.lambda_expr.params,
            lambda->data.lambda_expr.param_count * sizeof(ASTNode*)
        );
        lambda->data.lambda_expr.params[lambda->data.lambda_expr.param_count - 1] = param;
        
        if (!parser_match(parser, TOKEN_COMMA)) break;
    }
    
    parser_consume(parser, TOKEN_RPAREN, "Expected ')' after lambda parameters");
    
    // 람다 연산자 =>
    parser_consume(parser, TOKEN_LAMBDA, "Expected '=>' in lambda expression");
    
    // 바디 파싱 (표현식 또는 블록)
    if (parser_check(parser, TOKEN_LBRACE)) {
        // 블록 바디
        parser_consume(parser, TOKEN_LBRACE, "Expected '{' for lambda body");
        lambda->data.lambda_expr.body = parser_parse_block(parser);
    } else {
        // 표현식 바디
        lambda->data.lambda_expr.body = parser_parse_expression(parser);
    }
    
    return lambda;
}

static void skip_generic_arguments(Parser* parser) {
    int depth = 0;

    if (!parser_match(parser, TOKEN_LESS)) {
        return;
    }

    depth = 1;
    while (depth > 0 && !parser_is_at_end(parser)) {
        if (parser_match(parser, TOKEN_LESS)) {
            depth++;
        } else if (parser_match(parser, TOKEN_GREATER)) {
            depth--;
        } else {
            parser_advance(parser);
        }
    }
}
