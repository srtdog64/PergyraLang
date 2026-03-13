/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * pgy — Pergyra compiler driver
 *
 * Pipeline:
 *   .pgy source
 *     → Lexer
 *     → Parser   → AST
 *     → Semantic → annotated AST  (errors abort here)
 *     → Transpile → .c file
 *     → GCC       → native binary  (optional, --run to also execute)
 *
 * Usage:
 *   pgy <source.pgy>              transpile only → source.c
 *   pgy <source.pgy> -o <out.c>   transpile to named .c file
 *   pgy <source.pgy> --compile    transpile + compile → binary
 *   pgy <source.pgy> --run        transpile + compile + run
 *   pgy --tokens <source.pgy>     dump token stream
 *   pgy --ast    <source.pgy>     dump AST (placeholder)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/semantic.h"
#include "codegen/transpiler.h"

/* -----------------------------------------------------------------
 * Flags
 * ----------------------------------------------------------------- */

typedef struct
{
    const char *source_path;
    const char *output_c;     /* NULL → replace .pgy with .c */
    bool        do_compile;   /* invoke gcc after transpile  */
    bool        do_run;       /* run the binary after build  */
    bool        dump_tokens;
    bool        dump_ast;
    bool        verbose;
} DriverFlags;

/* -----------------------------------------------------------------
 * Utilities
 * ----------------------------------------------------------------- */

static char *
read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "pgy: cannot open '%s'\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);

    char *buf = malloc((size_t)sz + 1);
    if (buf == NULL) {
        fclose(f);
        return NULL;
    }
    fread(buf, 1, (size_t)sz, f);
    buf[sz] = '\0';
    fclose(f);
    return buf;
}

/*
 * Replace the extension of path with new_ext.
 * Caller must free the returned string.
 */
static char *
replace_extension(const char *path, const char *new_ext)
{
    const char *dot = strrchr(path, '.');
    size_t base_len = dot ? (size_t)(dot - path) : strlen(path);
    size_t new_len  = base_len + strlen(new_ext) + 1;
    char  *result   = malloc(new_len);
    if (result == NULL)
        return NULL;
    memcpy(result, path, base_len);
    strcpy(result + base_len, new_ext);
    return result;
}

/* -----------------------------------------------------------------
 * Token dump
 * ----------------------------------------------------------------- */

static int
run_token_dump(const char *source, const char *path)
{
    Lexer *lexer = lexer_create(source);
    if (lexer == NULL) {
        fprintf(stderr, "pgy: lexer init failed for '%s'\n", path);
        return 1;
    }

    printf("=== tokens: %s ===\n", path);
    int n = 0;
    Token tok;
    do {
        tok = lexer_next_token(lexer);
        printf("%4d  ", ++n);
        token_print(&tok);
        if (tok.type == TOKEN_ERROR) {
            fprintf(stderr, "pgy: lex error: %s\n", lexer_get_error(lexer));
            lexer_destroy(lexer);
            return 1;
        }
    } while (tok.type != TOKEN_EOF);

    printf("  %d tokens total\n", n);
    lexer_destroy(lexer);
    return 0;
}

/* -----------------------------------------------------------------
 * Core pipeline
 * ----------------------------------------------------------------- */

static int
run_pipeline(const DriverFlags *flags)
{
    /* 1 — read source */
    char *source = read_file(flags->source_path);
    if (source == NULL)
        return 1;

    if (flags->dump_tokens) {
        int rc = run_token_dump(source, flags->source_path);
        free(source);
        return rc;
    }

    if (flags->verbose)
        printf("pgy: lexing %s\n", flags->source_path);

    /* 2 — lex */
    Lexer *lexer = lexer_create(source);
    if (lexer == NULL) {
        fprintf(stderr, "pgy: out of memory\n");
        free(source);
        return 1;
    }

    /* 3 — parse */
    if (flags->verbose)
        printf("pgy: parsing\n");

    Parser *parser = parser_create(lexer);
    if (parser == NULL) {
        fprintf(stderr, "pgy: out of memory\n");
        lexer_destroy(lexer);
        free(source);
        return 1;
    }

    ASTNode *ast = parser_parse_program(parser);

    if (parser_has_error(parser)) {
        fprintf(stderr, "pgy: parse error: %s\n", parser_get_error(parser));
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(source);
        return 1;
    }

    if (flags->dump_ast) {
        /* Placeholder — ast_print not yet implemented */
        printf("pgy: AST dump not yet implemented\n");
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(source);
        return 0;
    }

    /* 4 — semantic analysis */
    if (flags->verbose)
        printf("pgy: semantic analysis\n");

    SemanticResult *sem = semantic_analyze(ast);
    if (sem == NULL) {
        fprintf(stderr, "pgy: out of memory during semantic analysis\n");
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(source);
        return 1;
    }

    semantic_result_print(sem);

    if (!sem->success) {
        fprintf(stderr, "pgy: %zu error(s) — aborting\n", sem->error_count);
        semantic_result_destroy(sem);
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(source);
        return 1;
    }

    /* 5 — transpile */
    char *output_c = flags->output_c
                     ? strdup(flags->output_c)
                     : replace_extension(flags->source_path, ".c");

    if (flags->verbose)
        printf("pgy: transpiling → %s\n", output_c);

    TranspileResult *trans = transpile(sem->annotated_ast, output_c);

    semantic_result_destroy(sem);
    parser_destroy(parser);
    lexer_destroy(lexer);
    free(source);

    if (trans == NULL || !trans->success) {
        fprintf(stderr, "pgy: transpile failed: %s\n",
                trans ? trans->error_message : "out of memory");
        transpile_result_destroy(trans);
        free(output_c);
        return 1;
    }

    transpile_result_destroy(trans);
    printf("pgy: wrote %s\n", output_c);

    int exit_code = 0;

    /* 6 — compile (optional) */
    if (flags->do_compile || flags->do_run) {
#ifdef _WIN32
        char *bin_path = replace_extension(flags->source_path, ".exe");
#else
        char *bin_path = replace_extension(flags->source_path, "");
#endif

        /*
         * Locate the runtime header relative to the output .c file.
         * We assume the driver is run from the project root so that
         * src/runtime is findable.
         */
        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
                 "gcc -std=c11 -Wall -O2 "
                 "-I src "
                 "-I src/runtime "
                 "%s "
                 "-o %s "
                 "-lpthread",
                 output_c, bin_path);

        if (flags->verbose)
            printf("pgy: %s\n", cmd);

        int rc = system(cmd);
        if (rc != 0) {
            fprintf(stderr, "pgy: gcc exited with code %d\n", rc);
            exit_code = 1;
        } else {
            printf("pgy: compiled → %s\n", bin_path);

            /* 7 — run (optional) */
            if (flags->do_run && exit_code == 0) {
                char run_cmd[512];
#ifdef _WIN32
                /* Convert forward slashes to backslashes for cmd.exe */
                snprintf(run_cmd, sizeof(run_cmd), "%s", bin_path);
                for (char *p = run_cmd; *p; p++)
                    if (*p == '/') *p = '\\';
#else
                snprintf(run_cmd, sizeof(run_cmd), "./%s", bin_path);
#endif
                printf("pgy: running %s\n--- output ---\n", bin_path);
                exit_code = system(run_cmd);
                printf("--- end ---\n");
            }
        }

        free(bin_path);
    }

    free(output_c);
    return exit_code;
}

/* -----------------------------------------------------------------
 * Argument parsing
 * ----------------------------------------------------------------- */

static void
print_usage(void)
{
    printf(
        "Usage:\n"
        "  pgy <source.pgy>              transpile only  → source.c\n"
        "  pgy <source.pgy> -o <out.c>   transpile to named output\n"
        "  pgy <source.pgy> --compile    transpile + compile\n"
        "  pgy <source.pgy> --run        transpile + compile + run\n"
        "  pgy --tokens <source.pgy>     dump token stream\n"
        "  pgy --ast    <source.pgy>     dump AST\n"
        "  pgy --help\n");
}

static DriverFlags
parse_args(int argc, char *argv[])
{
    DriverFlags f;
    memset(&f, 0, sizeof(f));

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage();
            exit(0);
        } else if (strcmp(argv[i], "--compile") == 0) {
            f.do_compile = true;
        } else if (strcmp(argv[i], "--run") == 0) {
            f.do_run     = true;
            f.do_compile = true;
        } else if (strcmp(argv[i], "--tokens") == 0) {
            f.dump_tokens = true;
        } else if (strcmp(argv[i], "--ast") == 0) {
            f.dump_ast = true;
        } else if (strcmp(argv[i], "-v") == 0
                || strcmp(argv[i], "--verbose") == 0) {
            f.verbose = true;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "pgy: -o requires an argument\n");
                exit(1);
            }
            f.output_c = argv[++i];
        } else if (argv[i][0] != '-') {
            f.source_path = argv[i];
        } else {
            fprintf(stderr, "pgy: unknown option '%s'\n", argv[i]);
            exit(1);
        }
    }

    if (f.source_path == NULL) {
        print_usage();
        exit(1);
    }

    return f;
}

/* -----------------------------------------------------------------
 * Entry point
 * ----------------------------------------------------------------- */

int
main(int argc, char *argv[])
{
    DriverFlags flags = parse_args(argc, argv);
    return run_pipeline(&flags);
}
