/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include "driver_app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../lexer/lexer.h"
#include "../semantic/semantic.h"
#include "hir.h"
#include "module_loader.h"
#include "path_utils.h"
#include "llvm_runner.h"
#include "c_runner.h"

/* Path utilities are now in path_utils.h/c */

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

int
driver_run_pipeline(const DriverFlags *flags)
{
    ASTNode *ast = NULL;
    SemanticResult *sem = NULL;
    HIRProgram *hir = NULL;
    int exit_code = 1;
    char *load_error = NULL;
    char *hir_error = NULL;

    if (flags->dump_tokens) {
        char *source = path_read_file(flags->source_path);
        if (source == NULL)
            return 1;
        int rc = run_token_dump(source, flags->source_path);
        free(source);
        return rc;
    }

    if (flags->verbose)
        printf("pgy: loading modules\n");

    ast = module_loader_load_program(flags->source_path, &load_error);
    if (ast == NULL) {
        fprintf(stderr, "pgy: %s\n",
                load_error != NULL ? load_error : "module loading failed");
        goto cleanup;
    }

    if (flags->dump_ast) {
        ast_print(ast, 0);
        exit_code = 0;
        goto cleanup;
    }

    if (flags->verbose)
        printf("pgy: semantic analysis\n");

    sem = semantic_analyze(ast);
    if (sem == NULL) {
        fprintf(stderr, "pgy: out of memory during semantic analysis\n");
        goto cleanup;
    }

    semantic_result_print(sem);
    if (!sem->success) {
        fprintf(stderr, "pgy: %zu error(s) — aborting\n", sem->error_count);
        goto cleanup;
    }

    hir = hir_lower(sem->annotated_ast, &hir_error);
    if (hir == NULL) {
        fprintf(stderr, "pgy: HIR lowering failed: %s\n",
                hir_error != NULL ? hir_error : "out of memory");
        goto cleanup;
    }

    if (flags->dump_hir) {
        hir_dump(hir, stdout);
        exit_code = 0;
        goto cleanup;
    }

    /* Dispatch to backend runner */
    if (flags->backend == BACKEND_LLVM && !flags->emit_c_only) {
        exit_code = llvm_runner_execute(flags, hir);
    } else {
        exit_code = c_runner_execute(flags, hir);
    }

cleanup:
    free(load_error);
    free(hir_error);
    hir_destroy(hir);
    semantic_result_destroy(sem);
    ast_destroy(ast);
    return exit_code;
}

void
driver_print_usage(void)
{
    printf(
        "Usage:\n"
        "  pgy <source.pgy>              compile to native binary\n"
        "  pgy <source.pgy> -o <out>     name the emitted native binary\n"
        "  pgy <source.pgy> --emit-c     stop after generating C\n"
        "  pgy <source.pgy> --emit-c -o <out.c>\n"
        "  pgy <source.pgy> --emit-llvm -o <out.ll>\n"
        "  pgy <source.pgy> --run        compile + run\n"
        "  pgy --tokens <source.pgy>     dump token stream\n"
        "  pgy --ast    <source.pgy>     dump merged/normalized AST\n"
        "  pgy --hir    <source.pgy>     dump lowered HIR summary\n"
#ifdef PGY_LLVM_ENABLED
        "  default backend: LLVM\n"
        "  pgy <source.pgy> --backend=llvm   use LLVM native backend\n"
#else
        "  default backend: C\n"
#endif
#ifdef PGY_LLVM_ENABLED
        "  pgy <source.pgy> --emit-llvm      emit LLVM IR text\n"
#endif
        "  pgy --repl                    interactive REPL\n"
        "  pgy --help\n");
}
