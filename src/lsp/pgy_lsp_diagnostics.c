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
#include "../common/squiggle_class.h"
#include "../semantic/diag_codes.h"
#include "../semantic/semantic.h"
#include "../compiler/dir.h"
#include "../compiler/hir.h"
#include "../compiler/rir.h"
#include "../compiler/air.h"
#include "../compiler/air_erasure_squiggle.h"
#include "../compiler/driver_app.h"   /* hir_lower, rir_lower */

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

/*
 * docs/140 slice 5c: BLUE erasure squiggles. Lower the (clean) program to AIR —
 * the only stage that measures erasure — collect the fully-erased nodes, and
 * append a BLUE advisory at each one's source line. LSP-only: the batch compiler
 * never lowers to AIR for diagnostics, so its cost stays here in the editor.
 */
static void
lsp_append_erasure_diagnostics(ASTNode *ast, char *diag_buf, size_t buf_size,
                               size_t *off, size_t *emitted)
{
    DIRProgram *dir = NULL;
    HIRProgram *hir = NULL;
    RIRProgram *rir = NULL;
    AIRProgram *air = NULL;
    char *error = NULL;
    AIRErasureSquiggle sites[20];
    size_t site_count = 0;

    if (ast == NULL)
        return;

    dir = dir_lower(ast, &error);
    hir = hir_lower(ast, &error);
    rir = rir_lower(ast, &error);
    if (hir != NULL && rir != NULL)
        (void)rir_enrich_with_hir_flow(rir, hir, &error);
    if (dir != NULL && hir != NULL && rir != NULL)
        air = air_synthesize(hir, dir, rir, &error);
    if (air != NULL)
        site_count = air_collect_erasure_squiggles(air, sites,
            sizeof(sites) / sizeof(sites[0]));

    for (size_t i = 0; i < site_count && *emitted < 20; i++) {
        size_t before = *off;
        char reason[512];
        int dline;
        int n;

        if (sites[i].line == 0u)
            continue;   /* synthetic node — no editor location */

        if (*emitted > 0 && *off < buf_size - 1)
            diag_buf[(*off)++] = ',';

        dline = (int)sites[i].line - 1;
        if (dline < 0) dline = 0;
        lsp_escape_json_string(reason, sizeof(reason),
            sites[i].reason != NULL ? sites[i].reason
                                    : "compressed to a runtime summary");

        n = snprintf(diag_buf + *off, buf_size - *off,
            "{\"range\":{\"start\":{\"line\":%d,\"character\":0},"
            "\"end\":{\"line\":%d,\"character\":100}},"
            "\"severity\":3,\"source\":\"pgy\","
            "\"code\":\"PGY_AIR_MEANING_ERASABLE\","
            "\"data\":{\"layer\":\"domain\",\"squiggleClass\":\"blue\"},"
            "\"message\":\"domain meaning is erasable at runtime: %s\"}",
            dline, dline, reason);
        if (!lsp_advance_json_offset(off, buf_size, n)) {
            *off = before;
            break;
        }
        (*emitted)++;
    }

    air_destroy(air);
    rir_destroy(rir);
    hir_destroy(hir);
    dir_destroy(dir);
    free(error);
}

bool
lsp_build_diagnostics_params(const char *uri, const char *source_text,
                             char *params, size_t params_size)
{
    if (params == NULL || params_size == 0)
        return false;
    params[0] = '\0';
    Lexer *lexer = lexer_create(source_text);
    if (lexer == NULL)
        return false;

    Parser *parser = parser_create(lexer);
    if (parser == NULL) {
        lexer_destroy(lexer);
        return false;
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
        /* Editor path: advisories on (docs/140). The batch compiler keeps them
         * off via plain semantic_analyze, so only the LSP pays their cost. */
        SemanticResult *sem = semantic_analyze_ex(ast, true);
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
                /* LSP severity: 1 Error, 2 Warning, 3 Information. Advisory ->
                 * Information so it shows without an error/warning badge; the
                 * squiggleClass in `data` lets a decoration client recolour it
                 * (amber/violet/blue). docs/140. */
                int severity = d->level == DIAG_ERROR
                    ? 1
                    : (d->level == DIAG_WARNING ? 2 : 3);
                SquiggleClass sq = squiggle_class_classify(
                    d->level == DIAG_ERROR, d->layer, d->code);

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
                    "\"squiggleClass\":\"%s\","
                    "\"cause_ir\":\"%s\","
                    "\"fix_source\":\"%s\"},"
                    "\"message\":\"%s\"}",
                    dline,
                    dline,
                    severity,
                    code,
                    diagnostic_layer_name(d->layer),
                    squiggle_class_name(sq),
                    cause_ir,
                    fix_source,
                    escaped);
                if (!lsp_advance_json_offset(&off, sizeof(diag_buf), n)) {
                    off = before;
                    break;
                }
                emitted++;
            }
            /* BLUE erasure squiggles, only for a clean parse/analyze (docs/140
             * slice 5c). Incomplete editor buffers skip AIR lowering. */
            if (sem->success)
                lsp_append_erasure_diagnostics(sem->annotated_ast, diag_buf,
                    sizeof(diag_buf), &off, &emitted);
        }
        semantic_result_destroy(sem);
    }

    char escaped_uri[2048];
    json_escape_copy(escaped_uri, sizeof(escaped_uri), uri);

    snprintf(params, params_size,
        "{\"uri\":\"%s\",\"diagnostics\":[%s]}", escaped_uri, diag_buf);

    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return true;
}

void
publish_diagnostics(const char *uri, const char *source_text)
{
    char params[16384];
    if (!lsp_build_diagnostics_params(uri, source_text, params,
                                      sizeof(params)))
        return;
    lsp_notify("textDocument/publishDiagnostics", params);
}
