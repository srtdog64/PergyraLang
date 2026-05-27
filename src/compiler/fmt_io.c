#include "fmt_io.h"
#include "path_utils.h"

#include "../lexer/lexer.h"
#include "../parser/ast.h"
#include "../parser/parser.h"

#include <stdio.h>
#include <stdlib.h>

char *
fmt_read_file(const char *path)
{
    return path_read_file(path);
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
