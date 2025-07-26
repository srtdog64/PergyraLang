#include "lexer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

// 키워드 테이블 (해시맵 대신 배열로 간단히 구현)
typedef struct {
    const char* text;
    TokenType type;
} Keyword;

static const Keyword keywords[] = {
    {"let", TOKEN_LET},
    {"claim_slot", TOKEN_CLAIM_SLOT},
    {"write", TOKEN_WRITE},
    {"read", TOKEN_READ},
    {"release", TOKEN_RELEASE},
    {"with", TOKEN_WITH},
    {"as", TOKEN_AS},
    {"parallel", TOKEN_PARALLEL},
    {"log", TOKEN_LOG},
    {"true", TOKEN_BOOL_TRUE},
    {"false", TOKEN_BOOL_FALSE},
    {NULL, TOKEN_ERROR}
};

// 인라인 어셈블리를 사용한 고속 문자 검사 함수
static inline bool is_alpha_fast(char c) {
    bool result;
    __asm__ volatile (
        "cmpb $'A', %%al\n\t"
        "jl 1f\n\t"
        "cmpb $'Z', %%al\n\t"
        "jle 2f\n\t"
        "cmpb $'a', %%al\n\t"
        "jl 1f\n\t"
        "cmpb $'z', %%al\n\t"
        "jle 2f\n\t"
        "cmpb $'_', %%al\n\t"
        "je 2f\n\t"
        "1: movb $0, %%dl\n\t"
        "jmp 3f\n\t"
        "2: movb $1, %%dl\n\t"
        "3: movb %%dl, %0"
        : "=m" (result)
        : "a" (c)
        : "dl"
    );
    return result;
}

static inline bool is_digit_fast(char c) {
    bool result;
    __asm__ volatile (
        "cmpb $'0', %%al\n\t"
        "jl 1f\n\t"
        "cmpb $'9', %%al\n\t"
        "jle 2f\n\t"
        "1: movb $0, %%dl\n\t"
        "jmp 3f\n\t"
        "2: movb $1, %%dl\n\t"
        "3: movb %%dl, %0"
        : "=m" (result)
        : "a" (c)
        : "dl"
    );
    return result;
}

static inline bool is_alnum_fast(char c) {
    return is_alpha_fast(c) || is_digit_fast(c);
}

// 렉서 생성
Lexer* lexer_create(const char* source) {
    if (!source) return NULL;
    
    Lexer* lexer = malloc(sizeof(Lexer));
    if (!lexer) return NULL;
    
    lexer->source = source;
    lexer->current = source;
    lexer->position = 0;
    lexer->line = 1;
    lexer->column = 1;
    lexer->has_error = false;
    lexer->error_msg[0] = '\0';
    
    return lexer;
}

// 렉서 해제
void lexer_destroy(Lexer* lexer) {
    if (lexer) {
        free(lexer);
    }
}

// 현재 문자 반환
static char peek(Lexer* lexer) {
    return *lexer->current;
}

// 다음 문자 반환
static char peek_next(Lexer* lexer) {
    if (*lexer->current == '\0') return '\0';
    return *(lexer->current + 1);
}

// 문자 하나 전진
static char advance(Lexer* lexer) {
    if (*lexer->current == '\0') return '\0';
    
    char c = *lexer->current;
    lexer->current++;
    lexer->position++;
    
    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    
    return c;
}

// 공백 건너뛰기
static void skip_whitespace(Lexer* lexer) {
    while (true) {
        char c = peek(lexer);
        if (c == ' ' || c == '\t' || c == '\r') {
            advance(lexer);
        } else if (c == '\n') {
            advance(lexer);
            // 개행은 토큰으로 처리할 수도 있음
            break;
        } else {
            break;
        }
    }
}

// 주석 건너뛰기
static void skip_comment(Lexer* lexer) {
    if (peek(lexer) == '/' && peek_next(lexer) == '/') {
        // 한 줄 주석
        while (peek(lexer) != '\n' && peek(lexer) != '\0') {
            advance(lexer);
        }
    } else if (peek(lexer) == '/' && peek_next(lexer) == '*') {
        // 여러 줄 주석
        advance(lexer); // '/'
        advance(lexer); // '*'
        
        while (true) {
            if (peek(lexer) == '\0') break;
            if (peek(lexer) == '*' && peek_next(lexer) == '/') {
                advance(lexer); // '*'
                advance(lexer); // '/'
                break;
            }
            advance(lexer);
        }
    }
}

// 키워드 검사
static TokenType check_keyword(const char* text, size_t length) {
    for (int i = 0; keywords[i].text != NULL; i++) {
        if (strlen(keywords[i].text) == length && 
            strncmp(keywords[i].text, text, length) == 0) {
            return keywords[i].type;
        }
    }
    return TOKEN_IDENTIFIER;
}

// 식별자 토큰 생성
static Token make_identifier_token(Lexer* lexer, const char* start) {
    Token token = {0};
    token.text = (char*)start;
    token.length = lexer->current - start;
    token.line = lexer->line;
    token.column = lexer->column - token.length;
    
    // 키워드 체크
    token.type = check_keyword(start, token.length);
    
    // 타입명 체크 (대문자로 시작하는 식별자)
    if (token.type == TOKEN_IDENTIFIER && isupper(*start)) {
        token.type = TOKEN_TYPE_NAME;
    }
    
    return token;
}

// 숫자 토큰 생성
static Token make_number_token(Lexer* lexer, const char* start) {
    Token token = {0};
    token.text = (char*)start;
    token.length = lexer->current - start;
    token.line = lexer->line;
    token.column = lexer->column - token.length;
    
    bool is_float = false;
    for (size_t i = 0; i < token.length; i++) {
        if (start[i] == '.') {
            is_float = true;
            break;
        }
    }
    
    if (is_float) {
        token.type = TOKEN_FLOAT;
        token.value.float_value = strtod(start, NULL);
    } else {
        token.type = TOKEN_INTEGER;
        token.value.int_value = strtoll(start, NULL, 10);
    }
    
    return token;
}

// 문자열 토큰 생성
static Token make_string_token(Lexer* lexer, const char* start, char quote) {
    Token token = {0};
    token.type = TOKEN_STRING;
    token.text = (char*)start + 1; // 따옴표 제외
    token.length = (lexer->current - start) - 2; // 양쪽 따옴표 제외
    token.line = lexer->line;
    token.column = lexer->column - (lexer->current - start);
    
    return token;
}

// 에러 토큰 생성
static Token make_error_token(Lexer* lexer, const char* message) {
    Token token = {0};
    token.type = TOKEN_ERROR;
    token.text = (char*)message;
    token.length = strlen(message);
    token.line = lexer->line;
    token.column = lexer->column;
    
    lexer->has_error = true;
    strncpy(lexer->error_msg, message, sizeof(lexer->error_msg) - 1);
    
    return token;
}

// 다음 토큰 반환
Token lexer_next_token(Lexer* lexer) {
    if (!lexer) {
        return (Token){TOKEN_ERROR, "Invalid lexer", 13, 0, 0, {0}};
    }
    
    skip_whitespace(lexer);
    
    // 주석 처리
    if (peek(lexer) == '/' && (peek_next(lexer) == '/' || peek_next(lexer) == '*')) {
        skip_comment(lexer);
        skip_whitespace(lexer);
    }
    
    const char* start = lexer->current;
    
    char c = advance(lexer);
    
    // EOF
    if (c == '\0') {
        return (Token){TOKEN_EOF, "", 0, lexer->line, lexer->column, {0}};
    }
    
    // 개행
    if (c == '\n') {
        return (Token){TOKEN_NEWLINE, "\n", 1, lexer->line - 1, 1, {0}};
    }
    
    // 식별자와 키워드
    if (is_alpha_fast(c)) {
        while (is_alnum_fast(peek(lexer))) {
            advance(lexer);
        }
        return make_identifier_token(lexer, start);
    }
    
    // 숫자
    if (is_digit_fast(c)) {
        while (is_digit_fast(peek(lexer))) {
            advance(lexer);
        }
        
        // 소수점 처리
        if (peek(lexer) == '.' && is_digit_fast(peek_next(lexer))) {
            advance(lexer); // '.'
            while (is_digit_fast(peek(lexer))) {
                advance(lexer);
            }
        }
        
        return make_number_token(lexer, start);
    }
    
    // 문자열
    if (c == '"' || c == '\'') {
        char quote = c;
        while (peek(lexer) != quote && peek(lexer) != '\0') {
            if (peek(lexer) == '\n') lexer->line++;
            advance(lexer);
        }
        
        if (peek(lexer) == '\0') {
            return make_error_token(lexer, "Unterminated string");
        }
        
        advance(lexer); // 닫는 따옴표
        return make_string_token(lexer, start, quote);
    }
    
    // 단일 문자 토큰
    switch (c) {
        case '=': return (Token){TOKEN_ASSIGN, "=", 1, lexer->line, lexer->column - 1, {0}};
        case '(': return (Token){TOKEN_LPAREN, "(", 1, lexer->line, lexer->column - 1, {0}};
        case ')': return (Token){TOKEN_RPAREN, ")", 1, lexer->line, lexer->column - 1, {0}};
        case '{': return (Token){TOKEN_LBRACE, "{", 1, lexer->line, lexer->column - 1, {0}};
        case '}': return (Token){TOKEN_RBRACE, "}", 1, lexer->line, lexer->column - 1, {0}};
        case '<': return (Token){TOKEN_LANGLE, "<", 1, lexer->line, lexer->column - 1, {0}};
        case '>': return (Token){TOKEN_RANGLE, ">", 1, lexer->line, lexer->column - 1, {0}};
        case '.': return (Token){TOKEN_DOT, ".", 1, lexer->line, lexer->column - 1, {0}};
        case ';': return (Token){TOKEN_SEMICOLON, ";", 1, lexer->line, lexer->column - 1, {0}};
        case ',': return (Token){TOKEN_COMMA, ",", 1, lexer->line, lexer->column - 1, {0}};
        default:
            return make_error_token(lexer, "Unexpected character");
    }
}

// 에러 상태 확인
bool lexer_has_error(const Lexer* lexer) {
    return lexer ? lexer->has_error : true;
}

// 에러 메시지 반환
const char* lexer_get_error(const Lexer* lexer) {
    return lexer ? lexer->error_msg : "Invalid lexer";
}

// 토큰 타입을 문자열로 변환
const char* token_type_to_string(TokenType type) {
    switch (type) {
        case TOKEN_LET: return "LET";
        case TOKEN_CLAIM_SLOT: return "CLAIM_SLOT";
        case TOKEN_WRITE: return "WRITE";
        case TOKEN_READ: return "READ";
        case TOKEN_RELEASE: return "RELEASE";
        case TOKEN_WITH: return "WITH";
        case TOKEN_AS: return "AS";
        case TOKEN_PARALLEL: return "PARALLEL";
        case TOKEN_LOG: return "LOG";
        case TOKEN_ASSIGN: return "ASSIGN";
        case TOKEN_LPAREN: return "LPAREN";
        case TOKEN_RPAREN: return "RPAREN";
        case TOKEN_LBRACE: return "LBRACE";
        case TOKEN_RBRACE: return "RBRACE";
        case TOKEN_LANGLE: return "LANGLE";
        case TOKEN_RANGLE: return "RANGLE";
        case TOKEN_DOT: return "DOT";
        case TOKEN_SEMICOLON: return "SEMICOLON";
        case TOKEN_COMMA: return "COMMA";
        case TOKEN_INTEGER: return "INTEGER";
        case TOKEN_FLOAT: return "FLOAT";
        case TOKEN_STRING: return "STRING";
        case TOKEN_BOOL_TRUE: return "BOOL_TRUE";
        case TOKEN_BOOL_FALSE: return "BOOL_FALSE";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_TYPE_NAME: return "TYPE_NAME";
        case TOKEN_EOF: return "EOF";
        case TOKEN_ERROR: return "ERROR";
        case TOKEN_NEWLINE: return "NEWLINE";
        default: return "UNKNOWN";
    }
}

// 토큰 출력
void token_print(const Token* token) {
    if (!token) return;
    
    printf("Token{type: %s, text: \"", token_type_to_string(token->type));
    
    // 텍스트 출력 (길이 제한)
    for (size_t i = 0; i < token->length && i < 50; i++) {
        printf("%c", token->text[i]);
    }
    
    printf("\", line: %u, col: %u", token->line, token->column);
    
    // 값 출력
    switch (token->type) {
        case TOKEN_INTEGER:
            printf(", value: %lld", token->value.int_value);
            break;
        case TOKEN_FLOAT:
            printf(", value: %f", token->value.float_value);
            break;
        case TOKEN_BOOL_TRUE:
        case TOKEN_BOOL_FALSE:
            printf(", value: %s", token->value.bool_value ? "true" : "false");
            break;
        default:
            break;
    }
    
    printf("}\n");
}