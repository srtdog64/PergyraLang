#ifndef PERGYRA_LEXER_H
#define PERGYRA_LEXER_H

#include <stdint.h>
#include <stdbool.h>

// 토큰 타입 정의
typedef enum {
    // 키워드
    TOKEN_LET,
    TOKEN_CLAIM_SLOT,
    TOKEN_WRITE,
    TOKEN_READ,
    TOKEN_RELEASE,
    TOKEN_WITH,
    TOKEN_AS,
    TOKEN_PARALLEL,
    TOKEN_LOG,
    
    // 연산자
    TOKEN_ASSIGN,       // =
    TOKEN_LPAREN,       // (
    TOKEN_RPAREN,       // )
    TOKEN_LBRACE,       // {
    TOKEN_RBRACE,       // }
    TOKEN_LANGLE,       // <
    TOKEN_RANGLE,       // >
    TOKEN_DOT,          // .
    TOKEN_SEMICOLON,    // ;
    TOKEN_COMMA,        // ,
    
    // 리터럴
    TOKEN_INTEGER,
    TOKEN_FLOAT,
    TOKEN_STRING,
    TOKEN_BOOL_TRUE,
    TOKEN_BOOL_FALSE,
    
    // 식별자
    TOKEN_IDENTIFIER,
    TOKEN_TYPE_NAME,
    
    // 특수
    TOKEN_EOF,
    TOKEN_ERROR,
    TOKEN_NEWLINE
} TokenType;

// 토큰 구조체
typedef struct {
    TokenType type;
    char* text;
    size_t length;
    uint32_t line;
    uint32_t column;
    union {
        int64_t int_value;
        double float_value;
        bool bool_value;
    } value;
} Token;

// 렉서 상태
typedef struct {
    const char* source;
    const char* current;
    size_t position;
    uint32_t line;
    uint32_t column;
    bool has_error;
    char error_msg[256];
} Lexer;

// 함수 선언
Lexer* lexer_create(const char* source);
void lexer_destroy(Lexer* lexer);
Token lexer_next_token(Lexer* lexer);
bool lexer_has_error(const Lexer* lexer);
const char* lexer_get_error(const Lexer* lexer);

// 토큰 유틸리티
const char* token_type_to_string(TokenType type);
void token_print(const Token* token);

#endif // PERGYRA_LEXER_H