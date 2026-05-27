/*
 * LSP diagnostic publishing.
 */

#include "pgy_lsp_internal.h"
#include "../common/numeric_parse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include "../semantic/diag_codes.h"
#include "../semantic/semantic.h"

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

static void
lsp_escape_json_string(char *out, size_t out_size, const char *text)
{
    size_t oi = 0;

    if (out == NULL || out_size == 0)
        return;
    if (text == NULL) {
        out[0] = '\0';
        return;
    }

    for (const char *p = text; *p != '\0' && oi < out_size - 1; p++) {
        if ((*p == '"' || *p == '\\') && oi + 1 < out_size - 1)
            out[oi++] = '\\';
        if (oi >= out_size - 1)
            break;
        out[oi++] = *p;
    }
    out[oi] = '\0';
}

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
        if (at_line) {
            int parsed = 0;
            if (pgy_parse_positive_int_prefix(at_line + 8, &parsed))
                line = parsed - 1;
        }
        if (line < 0) line = 0;

        char escaped[512];
        lsp_escape_json_string(escaped, sizeof(escaped), parse_msg);

        snprintf(diag_buf, sizeof(diag_buf),
            "{\"range\":{\"start\":{\"line\":%d,\"character\":0},"
            "\"end\":{\"line\":%d,\"character\":100}},"
            "\"severity\":1,\"source\":\"pgy\","
            "\"code\":\"" PGY_CODE_PARSE_SYNTAX "\","
            "\"data\":{\"layer\":\"syntax\","
            "\"cause_ir\":\"" PGY_CAUSE_PARSE_UNEXPECTED_TOKEN "\","
            "\"fix_source\":\"" PGY_FIX_CHECK_SYNTAX "\"},"
            "\"message\":\"%s\"}", line, line, escaped);
    } else if (ast != NULL) {
        SemanticResult *sem = semantic_analyze(ast);
        if (sem != NULL) {
            size_t off = 0;
            size_t emitted = 0;
            for (size_t i = 0; i < sem->diagnostic_count && emitted < 20; i++) {
                Diagnostic *d = sem->diagnostics[i];
                size_t before;
                if (d == NULL) continue;

                before = off;
                if (emitted > 0 && off < sizeof(diag_buf) - 1) {
                    diag_buf[off++] = ',';
                }
                int dline = (int)d->line - 1;
                if (dline < 0) dline = 0;
                int severity = (d->level == DIAG_ERROR) ? 1 : 2;

                char escaped[512];
                char code[128];
                char cause_ir[128];
                char fix_source[128];
                lsp_escape_json_string(escaped, sizeof(escaped), d->message);
                lsp_escape_json_string(code, sizeof(code), d->code);
                lsp_escape_json_string(cause_ir, sizeof(cause_ir), d->cause_ir);
                lsp_escape_json_string(fix_source, sizeof(fix_source),
                    d->fix_source);

                int n = snprintf(diag_buf + off, sizeof(diag_buf) - off,
                    "{\"range\":{\"start\":{\"line\":%d,\"character\":0},"
                    "\"end\":{\"line\":%d,\"character\":100}},"
                    "\"severity\":%d,\"source\":\"pgy\","
                    "\"code\":\"%s\","
                    "\"data\":{\"layer\":\"%s\","
                    "\"cause_ir\":\"%s\","
                    "\"fix_source\":\"%s\"},"
                    "\"message\":\"%s\"}",
                    dline,
                    dline,
                    severity,
                    code,
                    diagnostic_layer_name(d->layer),
                    cause_ir,
                    fix_source,
                    escaped);
                if (!lsp_advance_json_offset(&off, sizeof(diag_buf), n)) {
                    off = before;
                    break;
                }
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
