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
 *     → C backend → .c file
 *     → GCC       → native binary
 *
 * Usage:
 *   pgy <source.pgy>              compile → binary
 *   pgy <source.pgy> --emit-c     stop after generating C
 *   pgy <source.pgy> -o <out.c>   name the generated C file
 *   pgy <source.pgy> --run        compile + run
 *   pgy --tokens <source.pgy>     dump token stream
 *   pgy --ast    <source.pgy>     dump AST
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/semantic.h"
#include "compiler/compiler.h"
#include "common/string_compat.h"

typedef enum
{
    BACKEND_C,
    BACKEND_LLVM
} BackendKind;

typedef struct
{
    const char *source_path;
    const char *output_c;
    bool        emit_c_only;
    bool        emit_llvm_ir;
    bool        do_run;
    bool        dump_tokens;
    bool        dump_ast;
    bool        verbose;
    BackendKind backend;
} DriverFlags;

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

    size_t read_len = fread(buf, 1, (size_t)sz, f);
    if (read_len != (size_t)sz) {
        fclose(f);
        free(buf);
        fprintf(stderr, "pgy: failed to read '%s'\n", path);
        return NULL;
    }
    buf[sz] = '\0';
    fclose(f);
    return buf;
}

static char *
replace_extension(const char *path, const char *new_ext)
{
    const char *dot = strrchr(path, '.');
    size_t base_len = dot ? (size_t)(dot - path) : strlen(path);
    size_t new_len = base_len + strlen(new_ext) + 1;
    char *result = malloc(new_len);
    if (result == NULL)
        return NULL;

    memcpy(result, path, base_len);
    strcpy(result + base_len, new_ext);
    return result;
}

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

static int
run_pipeline(const DriverFlags *flags)
{
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

    Lexer *lexer = lexer_create(source);
    if (lexer == NULL) {
        fprintf(stderr, "pgy: out of memory\n");
        free(source);
        return 1;
    }

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
        ast_print(ast, 0);
        ast_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(source);
        return 0;
    }

    if (flags->verbose)
        printf("pgy: semantic analysis\n");

    SemanticResult *sem = semantic_analyze(ast);
    if (sem == NULL) {
        fprintf(stderr, "pgy: out of memory during semantic analysis\n");
        ast_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(source);
        return 1;
    }

    semantic_result_print(sem);
    if (!sem->success) {
        fprintf(stderr, "pgy: %zu error(s) — aborting\n", sem->error_count);
        semantic_result_destroy(sem);
        ast_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(source);
        return 1;
    }

    char *output_c = flags->output_c != NULL
        ? pergyra_strdup(flags->output_c)
        : replace_extension(flags->source_path, ".c");
    if (output_c == NULL) {
        fprintf(stderr, "pgy: out of memory\n");
        semantic_result_destroy(sem);
        ast_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(source);
        return 1;
    }

    int exit_code = 0;

#ifdef PGY_LLVM_ENABLED
    /* ---- LLVM backend ---- */
    if (flags->backend == BACKEND_LLVM) {
        if (flags->emit_llvm_ir) {
            if (flags->verbose)
                printf("pgy: emitting LLVM IR\n");

            CompilerResult *result = compiler_emit_llvm_ir(sem->annotated_ast,
                                                            "pergyra_module");
            if (result == NULL || !result->success) {
                fprintf(stderr, "pgy: LLVM IR generation failed: %s\n",
                        result != NULL ? result->error_message : "out of memory");
                compiler_result_destroy(result);
                free(output_c);
                semantic_result_destroy(sem);
                ast_destroy(ast);
                parser_destroy(parser);
                lexer_destroy(lexer);
                free(source);
                return 1;
            }
            compiler_result_destroy(result);
        } else {
            char *obj_path = replace_extension(flags->source_path, ".o");
#ifdef _WIN32
            char *bin_path = replace_extension(flags->source_path, ".exe");
#else
            char *bin_path = replace_extension(flags->source_path, "");
#endif
            if (obj_path == NULL || bin_path == NULL) {
                fprintf(stderr, "pgy: out of memory\n");
                free(obj_path);
                free(bin_path);
                free(output_c);
                semantic_result_destroy(sem);
                ast_destroy(ast);
                parser_destroy(parser);
                lexer_destroy(lexer);
                free(source);
                return 1;
            }

            CompilerResult *result = compiler_build_native_llvm(
                sem->annotated_ast, obj_path, bin_path, flags->verbose);
            if (result == NULL || !result->success) {
                fprintf(stderr, "pgy: LLVM compile failed: %s\n",
                        result != NULL ? result->error_message : "out of memory");
                compiler_result_destroy(result);
                free(obj_path);
                free(bin_path);
                free(output_c);
                semantic_result_destroy(sem);
                ast_destroy(ast);
                parser_destroy(parser);
                lexer_destroy(lexer);
                free(source);
                return 1;
            }

            printf("pgy: compiled (LLVM) → %s\n", bin_path);
            if (flags->do_run) {
                exit_code = compiler_run_binary(bin_path, flags->verbose);
                if (exit_code != 0)
                    fprintf(stderr, "pgy: program exited with code %d\n",
                            exit_code);
            }

            compiler_result_destroy(result);
            free(obj_path);
            free(bin_path);
        }
    } else
#endif /* PGY_LLVM_ENABLED */

    /* ---- C transpiler backend (default) ---- */
    if (flags->emit_c_only) {
        if (flags->verbose)
            printf("pgy: generating C → %s\n", output_c);

        CompilerResult *result = compiler_emit_c(sem->annotated_ast, output_c);
        if (result == NULL || !result->success) {
            fprintf(stderr, "pgy: C generation failed: %s\n",
                    result != NULL ? result->error_message : "out of memory");
            compiler_result_destroy(result);
            free(output_c);
            semantic_result_destroy(sem);
            ast_destroy(ast);
            parser_destroy(parser);
            lexer_destroy(lexer);
            free(source);
            return 1;
        }

        printf("pgy: wrote %s\n", output_c);
        compiler_result_destroy(result);
    } else {
        if (flags->verbose)
            printf("pgy: generating C → %s\n", output_c);

#ifdef _WIN32
        char *bin_path = replace_extension(flags->source_path, ".exe");
#else
        char *bin_path = replace_extension(flags->source_path, "");
#endif
        if (bin_path == NULL) {
            fprintf(stderr, "pgy: out of memory\n");
            free(output_c);
            semantic_result_destroy(sem);
            ast_destroy(ast);
            parser_destroy(parser);
            lexer_destroy(lexer);
            free(source);
            return 1;
        }

        CompilerResult *result = compiler_build_native(sem->annotated_ast,
                                                       output_c,
                                                       bin_path,
                                                       flags->verbose);
        if (result == NULL || !result->success) {
            fprintf(stderr, "pgy: compile failed: %s\n",
                    result != NULL ? result->error_message : "out of memory");
            compiler_result_destroy(result);
            free(bin_path);
            free(output_c);
            semantic_result_destroy(sem);
            ast_destroy(ast);
            parser_destroy(parser);
            lexer_destroy(lexer);
            free(source);
            return 1;
        }

        printf("pgy: compiled → %s\n", bin_path);
        if (flags->do_run) {
            exit_code = compiler_run_binary(bin_path, flags->verbose);
            if (exit_code != 0)
                fprintf(stderr, "pgy: program exited with code %d\n", exit_code);
        }

        compiler_result_destroy(result);
        free(bin_path);
    }

    free(output_c);
    semantic_result_destroy(sem);
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    free(source);
    return exit_code;
}

static void
print_usage(void)
{
    printf(
        "Usage:\n"
        "  pgy <source.pgy>              compile to native binary\n"
        "  pgy <source.pgy> --emit-c     stop after generating C\n"
        "  pgy <source.pgy> -o <out.c>   name the generated C file\n"
        "  pgy <source.pgy> --run        compile + run\n"
        "  pgy --tokens <source.pgy>     dump token stream\n"
        "  pgy --ast    <source.pgy>     dump AST\n"
#ifdef PGY_LLVM_ENABLED
        "  pgy <source.pgy> --backend=llvm   use LLVM native backend\n"
        "  pgy <source.pgy> --emit-llvm      emit LLVM IR text\n"
#endif
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
            continue;
        } else if (strcmp(argv[i], "--emit-c") == 0) {
            f.emit_c_only = true;
        } else if (strcmp(argv[i], "--backend=llvm") == 0) {
            f.backend = BACKEND_LLVM;
        } else if (strcmp(argv[i], "--backend=c") == 0) {
            f.backend = BACKEND_C;
        } else if (strcmp(argv[i], "--emit-llvm") == 0) {
            f.emit_llvm_ir = true;
            f.backend = BACKEND_LLVM;
        } else if (strcmp(argv[i], "--run") == 0) {
            f.do_run = true;
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

int
main(int argc, char *argv[])
{
    DriverFlags flags = parse_args(argc, argv);
    return run_pipeline(&flags);
}
