/*
 * LSP document symbols, definition, references, and rename handlers.
 */

#include "pgy_lsp_internal.h"

#include <stdio.h>
#include <string.h>

#include "../lexer/lexer.h"
#include "../parser/ast_api.h"
#include "../parser/parser.h"

static bool
lsp_advance_json_offset(size_t *off, size_t buf_size, int written)
{
    if (off == NULL || written <= 0)
        return false;
    if (*off >= buf_size)
        return false;
    if ((size_t)written >= buf_size - *off) {
        *off = buf_size - 1;
        return false;
    }
    *off += (size_t)written;
    return true;
}

static bool
append_document_symbol(char *buf, size_t buf_size, size_t *off,
                       const char *name, int kind, int line)
{
    char escaped[256];
    size_t before;
    int n;

    if (buf == NULL || off == NULL || name == NULL || *off >= buf_size)
        return false;
    json_escape_copy(escaped, sizeof(escaped), name);
    before = *off;
    if (*off > 1 && *off < buf_size - 1)
        buf[(*off)++] = ',';
    n = snprintf(buf + *off, buf_size - *off,
        "{\"name\":\"%s\",\"kind\":%d,"
        "\"range\":{\"start\":{\"line\":%d,\"character\":0},\"end\":{\"line\":%d,\"character\":0}},"
        "\"selectionRange\":{\"start\":{\"line\":%d,\"character\":0},\"end\":{\"line\":%d,\"character\":0}}}",
        escaped, kind, line, line, line, line);
    if (!lsp_advance_json_offset(off, buf_size, n)) {
        *off = before;
        return false;
    }
    return true;
}

static int
ast_decl_symbol_kind(const ASTNode *stmt)
{
    if (stmt == NULL)
        return 0;

    switch (stmt->type) {
    case AST_FUNC_DECL:
        return 12;
    case AST_CLASS_DECL:
    case AST_PARTY_DECL:
    case AST_ROSTER_DECL:
    case AST_WORLD_DECL:
    case AST_RELATION_DECL:
    case AST_EFFECT_DECL:
    case AST_ZONE_DECL:
        return 5;
    case AST_ABILITY_DECL:
    case AST_ROLE_DECL:
        return 11;
    case AST_ENUM_DECL:
        return 10;
    case AST_TYPE_ALIAS:
        return 13;
    default:
        return 0;
    }
}

void
respond_document_symbols(int id, const char *source_text)
{
    Lexer *lexer;
    Parser *parser;
    ASTNode *ast;
    char symbols[16384];
    size_t off = 0;

    lexer = lexer_create(source_text);
    if (lexer == NULL) {
        lsp_respond(id, "[]");
        return;
    }
    parser = parser_create(lexer);
    if (parser == NULL) {
        lexer_destroy(lexer);
        lsp_respond(id, "[]");
        return;
    }

    ast = parser_parse_program(parser);
    if (parser_has_error(parser) || ast == NULL || ast->type != AST_PROGRAM) {
        ast_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        lsp_respond(id, "[]");
        return;
    }

    symbols[off++] = '[';
    for (size_t i = 0; i < ast_program_statement_count(ast); i++) {
        ASTNode *stmt = ast_program_statement(ast, i);
        const char *decl_name;
        int kind;
        int line;
        if (off >= sizeof(symbols) - 1)
            break;
        if (stmt == NULL)
            continue;
        decl_name = ast_declaration_name(stmt);
        kind = ast_decl_symbol_kind(stmt);
        if (decl_name == NULL || kind == 0)
            continue;
        line = stmt->line > 0 ? (int)stmt->line - 1 : 0;
        if (!append_document_symbol(symbols, sizeof(symbols), &off,
                                    decl_name, kind, line))
            break;
    }
    if (off < sizeof(symbols) - 1)
        symbols[off++] = ']';
    symbols[off] = '\0';

    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    lsp_respond(id, symbols);
}

static bool
ast_decl_name_and_line(ASTNode *stmt, const char **name_out, int *line_out)
{
    if (name_out == NULL || line_out == NULL || stmt == NULL)
        return false;

    *name_out = NULL;
    *line_out = stmt->line > 0 ? (int)stmt->line - 1 : 0;

    *name_out = ast_declaration_name(stmt);
    return *name_out != NULL && ast_decl_symbol_kind(stmt) != 0;
}

void
respond_definition(int id, const char *uri, const char *source_text,
                   int line, int character)
{
    Lexer *lexer;
    Parser *parser;
    ASTNode *ast;
    char word[128];

    if (uri == NULL || source_text == NULL
        || !extract_word_at_position(source_text, line, character,
                                     word, sizeof(word))) {
        lsp_respond(id, "null");
        return;
    }

    lexer = lexer_create(source_text);
    if (lexer == NULL) {
        lsp_respond(id, "null");
        return;
    }
    parser = parser_create(lexer);
    if (parser == NULL) {
        lexer_destroy(lexer);
        lsp_respond(id, "null");
        return;
    }

    ast = parser_parse_program(parser);
    if (parser_has_error(parser) || ast == NULL || ast->type != AST_PROGRAM) {
        ast_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        lsp_respond(id, "null");
        return;
    }

    for (size_t i = 0; i < ast_program_statement_count(ast); i++) {
        const char *decl_name = NULL;
        int decl_line = 0;
        char escaped_uri[2048];

        if (!ast_decl_name_and_line(ast_program_statement(ast, i),
                                    &decl_name, &decl_line))
            continue;
        if (strcmp(decl_name, word) != 0)
            continue;

        json_escape_copy(escaped_uri, sizeof(escaped_uri), uri);
        snprintf(lsp_response_buf, sizeof(lsp_response_buf),
            "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":"
            "{\"uri\":\"%s\",\"range\":{\"start\":{\"line\":%d,\"character\":0},"
            "\"end\":{\"line\":%d,\"character\":0}}}}",
            id, escaped_uri, decl_line, decl_line);
        ast_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        lsp_send(lsp_response_buf);
        return;
    }

    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    lsp_respond(id, "null");
}

void
respond_references(int id, const char *uri, const char *source_text,
                   int line, int character)
{
    char word[128];
    char escaped_uri[2048];
    char refs[32768];
    size_t off = 0;
    const char *p;
    int cur_line = 0;
    int cur_col = 0;
    size_t word_len;

    if (uri == NULL || source_text == NULL
        || !extract_word_at_position(source_text, line, character,
                                     word, sizeof(word))) {
        lsp_respond(id, "[]");
        return;
    }

    json_escape_copy(escaped_uri, sizeof(escaped_uri), uri);
    word_len = strlen(word);
    refs[off++] = '[';
    p = source_text;

    while (*p && off < sizeof(refs) - 256) {
        if ((p == source_text || is_word_boundary_char(*(p - 1)))
            && strncmp(p, word, word_len) == 0
            && is_word_boundary_char(*(p + word_len))) {
            size_t before = off;
            int n;
            if (off > 1)
                refs[off++] = ',';
            n = snprintf(refs + off, sizeof(refs) - off,
                "{\"uri\":\"%s\",\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
                "\"end\":{\"line\":%d,\"character\":%d}}}",
                escaped_uri, cur_line, cur_col, cur_line, cur_col + (int)word_len);
            if (!lsp_advance_json_offset(&off, sizeof(refs), n)) {
                off = before;
                break;
            }
        }

        if (*p == '\n') {
            cur_line++;
            cur_col = 0;
        } else {
            cur_col++;
        }
        p++;
    }

    if (off < sizeof(refs) - 1)
        refs[off++] = ']';
    refs[off] = '\0';
    lsp_respond(id, refs);
}

void
respond_rename(int id, const char *uri, const char *source_text,
               int line, int character, const char *new_name)
{
    char word[128];
    char escaped_uri[2048];
    char escaped_new[512];
    char edits[32768];
    size_t off = 0;
    const char *p;
    int cur_line = 0;
    int cur_col = 0;
    size_t word_len;

    if (uri == NULL || source_text == NULL || new_name == NULL
        || !extract_word_at_position(source_text, line, character,
                                     word, sizeof(word))) {
        lsp_respond(id, "null");
        return;
    }

    if (word[0] == '\0') {
        lsp_respond(id, "null");
        return;
    }

    json_escape_copy(escaped_uri, sizeof(escaped_uri), uri);
    json_escape_copy(escaped_new, sizeof(escaped_new), new_name);
    word_len = strlen(word);
    edits[off++] = '[';
    p = source_text;

    while (*p && off < sizeof(edits) - 256) {
        if ((p == source_text || is_word_boundary_char(*(p - 1)))
            && strncmp(p, word, word_len) == 0
            && is_word_boundary_char(*(p + word_len))) {
            size_t before = off;
            int n;
            if (off > 1)
                edits[off++] = ',';
            n = snprintf(edits + off, sizeof(edits) - off,
                "{\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
                "\"end\":{\"line\":%d,\"character\":%d}},"
                "\"newText\":\"%s\"}",
                cur_line, cur_col, cur_line, cur_col + (int)word_len,
                escaped_new);
            if (!lsp_advance_json_offset(&off, sizeof(edits), n)) {
                off = before;
                break;
            }
        }

        if (*p == '\n') {
            cur_line++;
            cur_col = 0;
        } else {
            cur_col++;
        }
        p++;
    }

    if (off < sizeof(edits) - 1)
        edits[off++] = ']';
    edits[off] = '\0';

    snprintf(lsp_response_buf, sizeof(lsp_response_buf),
        "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":{\"changes\":{\"%s\":%s}}}",
        id, escaped_uri, edits);
    lsp_send(lsp_response_buf);
}
