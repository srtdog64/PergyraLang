#include "lexer/lexer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void check_unclosed(const char *source, unsigned line, unsigned column,
                           unsigned prefix_tokens)
{
    Lexer *lexer = lexer_create(source);
    assert(lexer != NULL);
    for (unsigned i = 0; i < prefix_tokens; ++i) {
        Token prefix = lexer_next_token(lexer);
        assert(prefix.type != TOKEN_ERROR && prefix.type != TOKEN_EOF);
    }
    Token error = lexer_next_token(lexer);
    assert(error.type == TOKEN_ERROR);
    assert(lexer->hasError);
    assert(strcmp(error.text, "Unterminated block comment") == 0);
    assert(strstr(lexer->errorMsg, "Code: PGY_LEX_INVALID_TOKEN") != NULL);
    assert(error.line == line && error.column == column);
    assert(lexer_token_stream_handle_equal(error.stream, lexer_token_stream_handle(lexer)));
    assert(error.ordinal == prefix_tokens);
    Token eof = lexer_next_token(lexer);
    assert(eof.type == TOKEN_EOF && eof.ordinal == prefix_tokens + 1);
    lexer_destroy(lexer);
}

static void check_closed(const char *source, PgyTokenType expected,
                         unsigned line, unsigned column)
{
    Lexer *lexer = lexer_create(source);
    assert(lexer != NULL);
    Token token = lexer_next_token(lexer);
    assert(token.type == expected && !lexer->hasError);
    assert(token.line == line && token.column == column);
    assert(token.ordinal == 0);
    lexer_destroy(lexer);
}

int main(void)
{
    check_unclosed("/*", 1, 3, 0);
    check_unclosed("/* text", 1, 8, 0);
    check_unclosed("/* trailing *", 1, 14, 0);
    check_unclosed("\n  /* a\nb", 3, 2, 0);
    check_unclosed("let x = 1; /* missing close", 1, 28, 5);
    check_unclosed("/**/ /*", 1, 8, 0);
    check_closed("/**/", TOKEN_EOF, 1, 5);
    check_closed("/* a\nb */let", TOKEN_LET, 2, 5);
    check_closed("/* one */ /* two */let", TOKEN_LET, 1, 20);
    check_closed("// /*", TOKEN_EOF, 1, 6);
    check_closed("\"/*\"", TOKEN_STRING, 1, 1);
    check_closed("/// /*", TOKEN_DOC_COMMENT, 1, 5);
    /* Error tokens already emitted for literals also retain stream identity. */
    Lexer *lexer = lexer_create("\"unterminated");
    assert(lexer != NULL);
    Token error = lexer_next_token(lexer);
    assert(error.type == TOKEN_ERROR && error.ordinal == 0);
    assert(lexer_token_stream_handle_equal(error.stream, lexer_token_stream_handle(lexer)));
    assert(lexer_next_token(lexer).ordinal == 1);
    lexer_destroy(lexer);
    puts("[lexer-block-comment] 6 refusals, 6 valid controls, literal error anchor: PASS");
    return 0;
}
