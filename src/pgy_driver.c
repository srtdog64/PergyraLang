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
 *     → HIR      → lowered program buckets
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
#include <time.h>

#ifdef _WIN32
#include <process.h>   /* _getpid */
#define getpid _getpid
#else
#include <unistd.h>    /* getpid */
#endif

#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/semantic.h"
#include "compiler/hir.h"
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
    bool        dump_hir;
    bool        verbose;
    bool        repl;
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

    /* ---- Resolve imports: inline imported file ASTs ---- */
    {
        /* Compute base directory from source_path */
        const char *last_sep = strrchr(flags->source_path, '/');
        const char *last_bsep = strrchr(flags->source_path, '\\');
        if (last_bsep != NULL && (last_sep == NULL || last_bsep > last_sep))
            last_sep = last_bsep;

        char base_dir[512] = ".";
        if (last_sep != NULL) {
            size_t dir_len = (size_t)(last_sep - flags->source_path);
            if (dir_len >= sizeof(base_dir)) dir_len = sizeof(base_dir) - 1;
            memcpy(base_dir, flags->source_path, dir_len);
            base_dir[dir_len] = '\0';
        }

        /* Scan for AST_IMPORT_DECL, parse imported files, merge statements */
        for (size_t i = 0; i < ast->data.program.count; i++) {
            ASTNode *stmt = ast->data.program.statements[i];
            if (stmt == NULL || stmt->type != AST_IMPORT_DECL)
                continue;

            const char *import_path = stmt->data.import_decl.path;
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", base_dir, import_path);

            char *imp_source = read_file(full_path);
            if (imp_source == NULL) {
                fprintf(stderr, "pgy: cannot resolve import '%s'\n", import_path);
                ast_destroy(ast);
                parser_destroy(parser);
                lexer_destroy(lexer);
                free(source);
                return 1;
            }

            Lexer *imp_lexer = lexer_create(imp_source);
            Parser *imp_parser = parser_create(imp_lexer);
            ASTNode *imp_ast = parser_parse_program(imp_parser);
            bool imp_err = parser_has_error(imp_parser);

            if (imp_err) {
                fprintf(stderr, "pgy: parse error in import '%s': %s\n",
                        import_path, parser_get_error(imp_parser));
                ast_destroy(imp_ast);
                parser_destroy(imp_parser);
                lexer_destroy(imp_lexer);
                free(imp_source);
                ast_destroy(ast);
                parser_destroy(parser);
                lexer_destroy(lexer);
                free(source);
                return 1;
            }

            /* Replace AST_IMPORT_DECL with imported statements */
            size_t imp_count = imp_ast->data.program.count;
            if (imp_count > 0) {
                size_t old_count = ast->data.program.count;
                size_t new_count = old_count - 1 + imp_count;
                ASTNode **new_stmts = malloc(new_count * sizeof(ASTNode*));
                /* Copy statements before import */
                for (size_t j = 0; j < i; j++)
                    new_stmts[j] = ast->data.program.statements[j];
                /* Copy imported statements */
                for (size_t j = 0; j < imp_count; j++)
                    new_stmts[i + j] = imp_ast->data.program.statements[j];
                /* Copy statements after import */
                for (size_t j = i + 1; j < old_count; j++)
                    new_stmts[j - 1 + imp_count] = ast->data.program.statements[j];

                ast_destroy(ast->data.program.statements[i]); /* free import node */
                free(ast->data.program.statements);
                ast->data.program.statements = new_stmts;
                ast->data.program.count = new_count;

                /* Detach imported statements from imp_ast so they aren't freed */
                imp_ast->data.program.statements = NULL;
                imp_ast->data.program.count = 0;

                /* Adjust index to skip newly inserted statements */
                i += imp_count - 1;
            } else {
                /* Empty import — just remove the import node */
                ast_destroy(ast->data.program.statements[i]);
                for (size_t j = i; j + 1 < ast->data.program.count; j++)
                    ast->data.program.statements[j] = ast->data.program.statements[j + 1];
                ast->data.program.count--;
                i--;
            }

            ast_destroy(imp_ast);
            parser_destroy(imp_parser);
            lexer_destroy(imp_lexer);
            free(imp_source);
        }
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

    char *hir_error = NULL;
    HIRProgram *hir = hir_lower(sem->annotated_ast, &hir_error);
    if (hir == NULL) {
        fprintf(stderr, "pgy: HIR lowering failed: %s\n",
                hir_error != NULL ? hir_error : "out of memory");
        free(hir_error);
        semantic_result_destroy(sem);
        ast_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(source);
        return 1;
    }
    free(hir_error);

    if (flags->dump_hir) {
        hir_dump(hir, stdout);
        hir_destroy(hir);
        semantic_result_destroy(sem);
        ast_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(source);
        return 0;
    }

    char *output_c = flags->output_c != NULL
        ? pergyra_strdup(flags->output_c)
        : replace_extension(flags->source_path, ".c");
    if (output_c == NULL) {
        fprintf(stderr, "pgy: out of memory\n");
        hir_destroy(hir);
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

            CompilerResult *result = compiler_emit_llvm_ir(hir,
                                                            "pergyra_module");
            if (result == NULL || !result->success) {
                fprintf(stderr, "pgy: LLVM IR generation failed: %s\n",
                        result != NULL ? result->error_message : "out of memory");
                compiler_result_destroy(result);
                free(output_c);
                hir_destroy(hir);
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
                hir_destroy(hir);
                semantic_result_destroy(sem);
                ast_destroy(ast);
                parser_destroy(parser);
                lexer_destroy(lexer);
                free(source);
                return 1;
            }

            CompilerResult *result = compiler_build_native_llvm(
                hir, obj_path, bin_path, flags->verbose);
            if (result == NULL || !result->success) {
                fprintf(stderr, "pgy: LLVM compile failed: %s\n",
                        result != NULL ? result->error_message : "out of memory");
                compiler_result_destroy(result);
                free(obj_path);
                free(bin_path);
                free(output_c);
                hir_destroy(hir);
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

        CompilerResult *result = compiler_emit_c(hir, output_c);
        if (result == NULL || !result->success) {
            fprintf(stderr, "pgy: C generation failed: %s\n",
                    result != NULL ? result->error_message : "out of memory");
            compiler_result_destroy(result);
            free(output_c);
            hir_destroy(hir);
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
            hir_destroy(hir);
            semantic_result_destroy(sem);
            ast_destroy(ast);
            parser_destroy(parser);
            lexer_destroy(lexer);
            free(source);
            return 1;
        }

        CompilerResult *result = compiler_build_native(hir,
                                                       output_c,
                                                       bin_path,
                                                       flags->verbose);
        if (result == NULL || !result->success) {
            fprintf(stderr, "pgy: compile failed: %s\n",
                    result != NULL ? result->error_message : "out of memory");
            compiler_result_destroy(result);
            free(bin_path);
            free(output_c);
            hir_destroy(hir);
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
    hir_destroy(hir);
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
        "  pgy --hir    <source.pgy>     dump lowered HIR summary\n"
#ifdef PGY_LLVM_ENABLED
        "  pgy <source.pgy> --backend=llvm   use LLVM native backend\n"
        "  pgy <source.pgy> --emit-llvm      emit LLVM IR text\n"
#endif
        "  pgy --repl                    interactive REPL\n"
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
        } else if (strcmp(argv[i], "--repl") == 0) {
            f.repl = true;
        } else if (strcmp(argv[i], "--hir") == 0) {
            f.dump_hir = true;
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

    if (f.source_path == NULL && !f.repl) {
        print_usage();
        exit(1);
    }

    return f;
}

/* Generate a unique temp file path in TMPDIR (or /tmp fallback) */
static void
repl_tmp_path(char *out, size_t out_size, const char *ext)
{
    const char *tmpdir = getenv("TMPDIR");
    if (tmpdir == NULL) tmpdir = getenv("TMP");
    if (tmpdir == NULL) tmpdir = getenv("TEMP");
#ifdef _WIN32
    if (tmpdir == NULL) tmpdir = ".";
#else
    if (tmpdir == NULL) tmpdir = "/tmp";
#endif
    static unsigned repl_salt = 0;
    if (repl_salt == 0) {
        repl_salt = (unsigned)time(NULL) ^ (unsigned)getpid();
    }
    snprintf(out, out_size, "%s/pgy_repl_%u_%x%s",
             tmpdir, (unsigned)getpid(), repl_salt, ext);
}

static int
run_repl(void)
{
    printf("Pergyra REPL v0.1 — type 'exit' to quit\n");

    /* Accumulate top-level declarations (func, struct, etc.) */
    char decls[16384] = "";
    char line[2048];

    while (1) {
        printf("pgy> ");
        fflush(stdout);
        if (fgets(line, sizeof(line), stdin) == NULL)
            break;

        /* Strip trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[--len] = '\0';
        if (len > 0 && line[len - 1] == '\r')
            line[--len] = '\0';

        if (len == 0)
            continue;
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0)
            break;

        /* If line starts with func/struct/class/ability/role, accumulate */
        bool is_decl = (strncmp(line, "func ", 5) == 0
                     || strncmp(line, "struct ", 7) == 0
                     || strncmp(line, "class ", 6) == 0);

        if (is_decl) {
            /* Read until closing brace by counting {} */
            char block[4096];
            snprintf(block, sizeof(block), "%s\n", line);
            int depth = 0;
            for (const char *p = line; *p; p++) {
                if (*p == '{') depth++;
                else if (*p == '}') depth--;
            }
            while (depth > 0) {
                printf("...  ");
                fflush(stdout);
                if (fgets(line, sizeof(line), stdin) == NULL)
                    break;
                size_t l = strlen(line);
                if (l > 0 && line[l - 1] == '\n') line[--l] = '\0';
                strncat(block, line, sizeof(block) - strlen(block) - 2);
                strcat(block, "\n");
                for (const char *p = line; *p; p++) {
                    if (*p == '{') depth++;
                    else if (*p == '}') depth--;
                }
            }
            strncat(decls, block, sizeof(decls) - strlen(decls) - 1);
            printf("  (defined)\n");
            continue;
        }

        /* Build temp source: decls + func Main() { <line> } */
        char tmp_source[32768];
        snprintf(tmp_source, sizeof(tmp_source),
            "%s\nfunc Main() -> Void {\n    %s\n}\n", decls, line);

        /* Write to unique temp file */
        char tmp_pgy[512], tmp_c[512], tmp_exe[512];
        repl_tmp_path(tmp_pgy, sizeof(tmp_pgy), ".pgy");
        repl_tmp_path(tmp_c,   sizeof(tmp_c),   ".c");
        repl_tmp_path(tmp_exe, sizeof(tmp_exe),  ".exe");

        FILE *f = fopen(tmp_pgy, "w");
        if (f == NULL) {
            fprintf(stderr, "  error: cannot create temp file\n");
            continue;
        }
        fputs(tmp_source, f);
        fclose(f);

        /* Use this driver itself to compile+run */
        DriverFlags rf;
        memset(&rf, 0, sizeof(rf));
        rf.source_path = tmp_pgy;
        rf.do_run = true;
        run_pipeline(&rf);

        /* Cleanup temp files */
        remove(tmp_pgy);
        remove(tmp_c);
        remove(tmp_exe);
    }

    printf("Bye!\n");
    return 0;
}

int
main(int argc, char *argv[])
{
    DriverFlags flags = parse_args(argc, argv);
    if (flags.repl)
        return run_repl();
    return run_pipeline(&flags);
}
