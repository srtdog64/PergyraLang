/*
 * pgy fmt — Pergyra source code formatter
 *
 * Strategy: lex → reformat tokens → output
 * Does not parse to AST — operates on token stream for safety.
 * Preserves comments. Normalizes indentation to 4 spaces.
 * Enforces BSD (Allman) brace style.
 */

#include "fmt.h"
#include "../lexer/lexer.h"
#include "../common/string_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static char *
read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)len + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, (size_t)len, f);
    buf[len] = '\0';
    fclose(f);
    return buf;
}

static bool
is_keyword(const char *text)
{
    static const char *keywords[] = {
        "subject", "class", "struct", "vessel", "object", "tobject",
        "zone", "world", "ability", "role", "relation", "effect",
        "party", "roster", "intent", "step",
        "func", "action", "let", "if", "else", "while", "for", "in",
        "return", "match", "case", "default", "break", "continue",
        "import", "use", "async", "await", "spawn", "parallel",
        "select", "channel", "with", "enum",
        NULL
    };
    for (int i = 0; keywords[i]; i++) {
        if (strcmp(text, keywords[i]) == 0) return true;
    }
    return false;
}

static bool
is_block_opener(const char *text)
{
    static const char *openers[] = {
        "subject", "class", "struct", "vessel", "object", "tobject",
        "zone", "world", "ability", "role", "relation", "effect",
        "party", "roster", "intent", "step",
        "func", "action", "if", "else", "while", "for",
        "match", "case", "parallel", "enum",
        NULL
    };
    for (int i = 0; openers[i]; i++) {
        if (strcmp(text, openers[i]) == 0) return true;
    }
    return false;
}

typedef struct {
    FILE *out;
    int indent;
    bool at_line_start;
    bool needs_blank_before_block;
} FmtCtx;

static void
fmt_indent(FmtCtx *ctx)
{
    for (int i = 0; i < ctx->indent; i++)
        fprintf(ctx->out, "    ");
    ctx->at_line_start = false;
}

static void
fmt_newline(FmtCtx *ctx)
{
    fprintf(ctx->out, "\n");
    ctx->at_line_start = true;
}

int
driver_run_fmt_command(int argc, char *argv[])
{
    bool write_inplace = false;
    const char *path = NULL;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--write") == 0 || strcmp(argv[i], "-w") == 0) {
            write_inplace = true;
        } else if (argv[i][0] != '-') {
            path = argv[i];
        }
    }

    if (!path) {
        fprintf(stderr, "Usage: pgy fmt <file.pgy> [--write]\n");
        return 1;
    }

    char *source = read_file(path);
    if (!source) {
        fprintf(stderr, "pgy fmt: cannot read '%s'\n", path);
        return 1;
    }

    /* Format to memory buffer first */
    FILE *out;
    char *buf = NULL;
    size_t buf_size = 0;

    if (write_inplace) {
        out = open_memstream(&buf, &buf_size);
        if (!out) {
            /* Fallback for Windows where open_memstream may not exist */
            char tmppath[512];
            snprintf(tmppath, sizeof(tmppath), "%s.fmt.tmp", path);
            out = fopen(tmppath, "w");
            if (!out) {
                fprintf(stderr, "pgy fmt: cannot create temp file\n");
                free(source);
                return 1;
            }
        }
    } else {
        out = stdout;
    }

    /* Token-based reformatting */
    Lexer *lexer = lexer_create(source);
    FmtCtx ctx = { .out = out, .indent = 0, .at_line_start = true,
                    .needs_blank_before_block = false };

    Token prev = { .type = TOKEN_EOF };
    Token tok;
    int prev_line = 1;

    do {
        tok = lexer_next_token(lexer);
        if (tok.type == TOKEN_ERROR) break;

        /* Handle blank lines between top-level declarations */
        if (tok.line > prev_line + 1 && ctx.indent == 0 && !ctx.at_line_start) {
            fmt_newline(&ctx);
        }

        /* Handle newlines from source */
        if (tok.line > prev_line && tok.type != TOKEN_EOF) {
            if (!ctx.at_line_start) fmt_newline(&ctx);
        }

        switch (tok.type) {
        case TOKEN_LBRACE:
            /* BSD style: brace on new line */
            if (!ctx.at_line_start) fmt_newline(&ctx);
            fmt_indent(&ctx);
            fprintf(out, "{");
            fmt_newline(&ctx);
            ctx.indent++;
            break;

        case TOKEN_RBRACE:
            ctx.indent--;
            if (ctx.indent < 0) ctx.indent = 0;
            if (!ctx.at_line_start) fmt_newline(&ctx);
            fmt_indent(&ctx);
            fprintf(out, "}");
            fmt_newline(&ctx);
            break;

        case TOKEN_SEMICOLON:
            fprintf(out, ";");
            fmt_newline(&ctx);
            break;

        case TOKEN_COMMENT: {
            if (ctx.at_line_start) fmt_indent(&ctx);
            fprintf(out, "%s", tok.text ? tok.text : "");
            fmt_newline(&ctx);
            break;
        }

        case TOKEN_STRING:
            if (ctx.at_line_start) fmt_indent(&ctx);
            else if (prev.type != TOKEN_LPAREN && prev.type != TOKEN_LBRACKET
                     && prev.type != TOKEN_COMMA)
                fprintf(out, " ");
            fprintf(out, "\"%s\"", tok.text ? tok.text : "");
            break;

        case TOKEN_COMMA:
            fprintf(out, ",");
            break;

        case TOKEN_COLON:
            fprintf(out, ":");
            break;

        case TOKEN_LPAREN:
            fprintf(out, "(");
            break;

        case TOKEN_RPAREN:
            fprintf(out, ")");
            break;

        case TOKEN_LBRACKET:
            fprintf(out, "[");
            break;

        case TOKEN_RBRACKET:
            fprintf(out, "]");
            break;

        default:
            if (ctx.at_line_start) fmt_indent(&ctx);
            else if (tok.text && prev.type != TOKEN_LPAREN
                     && prev.type != TOKEN_DOT
                     && tok.type != TOKEN_DOT
                     && prev.type != TOKEN_LBRACKET
                     && tok.type != TOKEN_RBRACKET)
                fprintf(out, " ");
            if (tok.text) fprintf(out, "%s", tok.text);
            break;
        }

        prev_line = tok.line;
        prev = tok;
    } while (tok.type != TOKEN_EOF);

    if (!ctx.at_line_start) fmt_newline(&ctx);

    lexer_destroy(lexer);
    free(source);

    if (write_inplace && buf != NULL) {
        fclose(out);
        FILE *wf = fopen(path, "w");
        if (wf) {
            fwrite(buf, 1, buf_size, wf);
            fclose(wf);
            printf("pgy fmt: formatted '%s'\n", path);
        }
        free(buf);
    } else if (write_inplace && out != stdout) {
        /* tmpfile fallback path */
        fclose(out);
        char tmppath[512];
        snprintf(tmppath, sizeof(tmppath), "%s.fmt.tmp", path);
        remove(path);
        rename(tmppath, path);
        printf("pgy fmt: formatted '%s'\n", path);
    }

    return 0;
}
