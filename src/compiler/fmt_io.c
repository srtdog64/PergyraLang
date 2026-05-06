#include "fmt_io.h"

#include "../lexer/lexer.h"
#include "../parser/ast.h"
#include "../parser/parser.h"

#include <stdio.h>
#include <stdlib.h>

char *
fmt_read_file(const char *path)
{
    FILE *f;
    long len;
    size_t read_len;
    char *buf;

    if (path == NULL)
        return NULL;
    f = fopen(path, "rb");
    if (f == NULL)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    len = ftell(f);
    if (len < 0) {
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    buf = malloc((size_t)len + 1);
    if (buf == NULL) {
        fclose(f);
        return NULL;
    }
    read_len = fread(buf, 1, (size_t)len, f);
    buf[read_len] = '\0';
    fclose(f);
    return buf;
}

bool
fmt_source_is_parseable(const char *source)
{
    Lexer *lexer;
    Parser *parser;
    ASTNode *program;
    bool ok;

    if (source == NULL)
        return false;

    lexer = lexer_create(source);
    if (lexer == NULL)
        return false;
    parser = parser_create(lexer);
    if (parser == NULL) {
        lexer_destroy(lexer);
        return false;
    }

    program = parser_parse_program(parser);
    ok = !parser_has_error(parser);
    ast_destroy(program);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return ok;
}
