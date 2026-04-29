/*
 * LSP document symbols, definition, references, and rename handlers.
 */

#include "pgy_lsp_internal.h"

#include <stdio.h>
#include <string.h>

#include "../lexer/lexer.h"
#include "../parser/parser.h"

static void
append_document_symbol(char *buf, size_t buf_size, size_t *off,
                       const char *name, int kind, int line)
{
    char escaped[256];
    int n;

    if (buf == NULL || off == NULL || name == NULL || *off >= buf_size)
        return;
    json_escape_copy(escaped, sizeof(escaped), name);
    if (*off > 1 && *off < buf_size - 1)
        buf[(*off)++] = ',';
    n = snprintf(buf + *off, buf_size - *off,
        "{\"name\":\"%s\",\"kind\":%d,"
        "\"range\":{\"start\":{\"line\":%d,\"character\":0},\"end\":{\"line\":%d,\"character\":0}},"
        "\"selectionRange\":{\"start\":{\"line\":%d,\"character\":0},\"end\":{\"line\":%d,\"character\":0}}}",
        escaped, kind, line, line, line, line);
    if (n > 0)
        *off += (size_t)n;
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
    for (size_t i = 0; i < ast->data.program.count; i++) {
        ASTNode *stmt = ast->data.program.statements[i];
        int line;
        if (stmt == NULL)
            continue;
        line = stmt->line > 0 ? (int)stmt->line - 1 : 0;
        switch (stmt->type) {
        case AST_FUNC_DECL:
            append_document_symbol(symbols, sizeof(symbols), &off,
                stmt->data.func_decl.name, 12, line);
            break;
        case AST_CLASS_DECL:
            append_document_symbol(symbols, sizeof(symbols), &off,
                stmt->data.class_decl.name, 5, line);
            break;
        case AST_ABILITY_DECL:
            append_document_symbol(symbols, sizeof(symbols), &off,
                stmt->data.ability_decl.name, 11, line);
            break;
        case AST_ROLE_DECL:
            append_document_symbol(symbols, sizeof(symbols), &off,
                stmt->data.role_decl.name, 11, line);
            break;
        case AST_PARTY_DECL:
            append_document_symbol(symbols, sizeof(symbols), &off,
                stmt->data.party_decl.name, 5, line);
            break;
        case AST_ROSTER_DECL:
            append_document_symbol(symbols, sizeof(symbols), &off,
                stmt->data.roster_decl.name, 5, line);
            break;
        case AST_WORLD_DECL:
            append_document_symbol(symbols, sizeof(symbols), &off,
                stmt->data.world_decl.name, 5, line);
            break;
        case AST_RELATION_DECL:
            append_document_symbol(symbols, sizeof(symbols), &off,
                stmt->data.relation_decl.name, 5, line);
            break;
        case AST_EFFECT_DECL:
            append_document_symbol(symbols, sizeof(symbols), &off,
                stmt->data.effect_decl.name, 5, line);
            break;
        case AST_ZONE_DECL:
            append_document_symbol(symbols, sizeof(symbols), &off,
                stmt->data.zone_decl.name, 5, line);
            break;
        case AST_ENUM_DECL:
            append_document_symbol(symbols, sizeof(symbols), &off,
                stmt->data.enum_decl.name, 10, line);
            break;
        case AST_TYPE_ALIAS:
            append_document_symbol(symbols, sizeof(symbols), &off,
                stmt->data.type_alias.name, 13, line);
            break;
        default:
            break;
        }
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

    switch (stmt->type) {
    case AST_FUNC_DECL:
        *name_out = stmt->data.func_decl.name;
        return *name_out != NULL;
    case AST_CLASS_DECL:
        *name_out = stmt->data.class_decl.name;
        return *name_out != NULL;
    case AST_ABILITY_DECL:
        *name_out = stmt->data.ability_decl.name;
        return *name_out != NULL;
    case AST_ROLE_DECL:
        *name_out = stmt->data.role_decl.name;
        return *name_out != NULL;
    case AST_PARTY_DECL:
        *name_out = stmt->data.party_decl.name;
        return *name_out != NULL;
    case AST_ROSTER_DECL:
        *name_out = stmt->data.roster_decl.name;
        return *name_out != NULL;
    case AST_WORLD_DECL:
        *name_out = stmt->data.world_decl.name;
        return *name_out != NULL;
    case AST_RELATION_DECL:
        *name_out = stmt->data.relation_decl.name;
        return *name_out != NULL;
    case AST_EFFECT_DECL:
        *name_out = stmt->data.effect_decl.name;
        return *name_out != NULL;
    case AST_ZONE_DECL:
        *name_out = stmt->data.zone_decl.name;
        return *name_out != NULL;
    case AST_ENUM_DECL:
        *name_out = stmt->data.enum_decl.name;
        return *name_out != NULL;
    case AST_TYPE_ALIAS:
        *name_out = stmt->data.type_alias.name;
        return *name_out != NULL;
    default:
        return false;
    }
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

    for (size_t i = 0; i < ast->data.program.count; i++) {
        const char *decl_name = NULL;
        int decl_line = 0;
        char escaped_uri[2048];

        if (!ast_decl_name_and_line(ast->data.program.statements[i], &decl_name, &decl_line))
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
            int n;
            if (off > 1)
                refs[off++] = ',';
            n = snprintf(refs + off, sizeof(refs) - off,
                "{\"uri\":\"%s\",\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
                "\"end\":{\"line\":%d,\"character\":%d}}}",
                escaped_uri, cur_line, cur_col, cur_line, cur_col + (int)word_len);
            if (n > 0)
                off += (size_t)n;
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
            int n;
            if (off > 1)
                edits[off++] = ',';
            n = snprintf(edits + off, sizeof(edits) - off,
                "{\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
                "\"end\":{\"line\":%d,\"character\":%d}},"
                "\"newText\":\"%s\"}",
                cur_line, cur_col, cur_line, cur_col + (int)word_len,
                escaped_new);
            if (n > 0)
                off += (size_t)n;
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
