/*
 * LSP diagnostic publishing.
 */

#include "pgy_lsp_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include "../semantic/semantic.h"

void
publish_diagnostics(const char *uri, const char *source_text)
{
    Lexer *lexer = lexer_create(source_text);
    if (lexer == NULL) return;

    Parser *parser = parser_create(lexer);
    if (parser == NULL) {
        lexer_destroy(lexer);
        return;
    }

    ASTNode *ast = parser_parse_program(parser);
    bool parse_err = parser_has_error(parser);
    const char *parse_msg = parse_err ? parser_get_error(parser) : NULL;

    char diag_buf[8192];
    diag_buf[0] = '\0';

    if (parse_err && parse_msg != NULL) {
        int line = 0;
        const char *at_line = strstr(parse_msg, "at line ");
        if (at_line) line = atoi(at_line + 8) - 1;
        if (line < 0) line = 0;

        char escaped[512];
        size_t ei = 0;
        for (const char *p = parse_msg; *p && ei < sizeof(escaped) - 2; p++) {
            if (*p == '"' || *p == '\\') escaped[ei++] = '\\';
            escaped[ei++] = *p;
        }
        escaped[ei] = '\0';

        snprintf(diag_buf, sizeof(diag_buf),
            "{\"range\":{\"start\":{\"line\":%d,\"character\":0},"
            "\"end\":{\"line\":%d,\"character\":100}},"
            "\"severity\":1,\"source\":\"pgy\","
            "\"message\":\"%s\"}", line, line, escaped);
    } else if (ast != NULL) {
        SemanticResult *sem = semantic_analyze(ast);
        if (sem != NULL) {
            size_t off = 0;
            size_t emitted = 0;
            for (size_t i = 0; i < sem->diagnostic_count && emitted < 20; i++) {
                Diagnostic *d = sem->diagnostics[i];
                if (d == NULL) continue;

                if (emitted > 0 && off < sizeof(diag_buf) - 1) {
                    diag_buf[off++] = ',';
                }
                int dline = (int)d->line - 1;
                if (dline < 0) dline = 0;
                int severity = (d->level == DIAG_ERROR) ? 1 : 2;

                char escaped[512];
                size_t ei = 0;
                for (const char *p = d->message;
                     *p && ei < sizeof(escaped) - 2; p++) {
                    if (*p == '"' || *p == '\\') escaped[ei++] = '\\';
                    escaped[ei++] = *p;
                }
                escaped[ei] = '\0';

                int n = snprintf(diag_buf + off, sizeof(diag_buf) - off,
                    "{\"range\":{\"start\":{\"line\":%d,\"character\":0},"
                    "\"end\":{\"line\":%d,\"character\":100}},"
                    "\"severity\":%d,\"source\":\"pgy\","
                    "\"message\":\"%s\"}", dline, dline, severity, escaped);
                if (n > 0) off += (size_t)n;
                emitted++;
            }
        }
        semantic_result_destroy(sem);
    }

    char escaped_uri[2048];
    json_escape_copy(escaped_uri, sizeof(escaped_uri), uri);

    char params[16384];
    snprintf(params, sizeof(params),
        "{\"uri\":\"%s\",\"diagnostics\":[%s]}", escaped_uri, diag_buf);
    lsp_notify("textDocument/publishDiagnostics", params);

    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
}
