#ifndef PERGYRA_PARSER_H
#define PERGYRA_PARSER_H

#include "ast.h"
#include "../lexer/lexer.h"

// 파서 상태
typedef struct {
    Lexer* lexer;
    Token current_token;
    Token previous_token;
    bool has_error;
    char error_msg[512];
    
    // 파싱 컨텍스트
    bool in_parallel_block;
    bool in_with_statement;
    int scope_depth;
} Parser;

// 파서 생성 및 해제
Parser* parser_create(Lexer* lexer);
void parser_destroy(Parser* parser);

// 메인 파싱 함수
ASTNode* parser_parse_program(Parser* parser);

// 문장 파싱
ASTNode* parser_parse_statement(Parser* parser);
ASTNode* parser_parse_let_declaration(Parser* parser);
ASTNode* parser_parse_with_statement(Parser* parser);
ASTNode* parser_parse_parallel_block(Parser* parser);
ASTNode* parser_parse_expression_statement(Parser* parser);
ASTNode* parser_parse_block(Parser* parser);

// 표현식 파싱 (우선순위별)
ASTNode* parser_parse_expression(Parser* parser);
ASTNode* parser_parse_assignment(Parser* parser);
ASTNode* parser_parse_call(Parser* parser);
ASTNode* parser_parse_member_access(Parser* parser);
ASTNode* parser_parse_primary(Parser* parser);

// 특수 표현식
ASTNode* parser_parse_function_call(Parser* parser, const char* function_name);
ASTNode* parser_parse_slot_operation(Parser* parser);
ASTNode* parser_parse_type_parameter(Parser* parser);

// 토큰 관리
bool parser_match(Parser* parser, TokenType type);
bool parser_check(Parser* parser, TokenType type);
Token parser_advance(Parser* parser);
Token parser_consume(Parser* parser, TokenType type, const char* message);

// 에러 처리
bool parser_has_error(const Parser* parser);
const char* parser_get_error(const Parser* parser);
void parser_error(Parser* parser, const char* message);
void parser_synchronize(Parser* parser);

// 유틸리티
bool parser_is_at_end(const Parser* parser);
bool parser_is_statement_start(TokenType type);
bool parser_is_expression_start(TokenType type);

#endif // PERGYRA_PARSER_H