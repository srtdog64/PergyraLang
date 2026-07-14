/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include "import_resolver.h"
#include "import_resolver_internal.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../common/string_compat.h"
#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include "module_normalizer.h"
#include "path_utils.h"

static void
set_error(char **error_message, const char *fmt, ...)
{
    va_list args;
    char *formatted;

    if (error_message == NULL)
        return;

    /* Heap-exact: these messages nest a full parser diagnostic inside a
     * module path, so a fixed buffer would silently drop the tail of the
     * very thing the reader needs. NULL only on OOM, which every caller
     * already renders as its own generic fallback. */
    va_start(args, fmt);
    formatted = pergyra_strdup_vprintf(fmt, args);
    va_end(args);

    free(*error_message);
    *error_message = formatted;
}

/* read_file_text, path_dirname_dup, path_join_dup are now in path_utils.h */
#define read_file_text path_read_file

static ASTNode *
parse_program_file(const char *path, char **error_message)
{
    char *source = read_file_text(path);
    if (source == NULL) {
        set_error(error_message, "cannot open '%s'", path);
        return NULL;
    }

    Lexer *lexer = lexer_create(source);
    Parser *parser = lexer != NULL ? parser_create(lexer) : NULL;
    ASTNode *ast = NULL;

    if (lexer == NULL || parser == NULL) {
        set_error(error_message, "out of memory while loading '%s'", path);
        goto cleanup;
    }
    parser->source_path = path;

    ast = parser_parse_program(parser);
    if (parser_has_error(parser)) {
        set_error(error_message, "parse error in '%s': %s",
                  path, parser_get_error(parser));
        ast_destroy(ast);
        ast = NULL;
    }

cleanup:
    parser_destroy(parser);
    lexer_destroy(lexer);
    free(source);
    return ast;
}

static ASTNode *
import_resolver_load_internal(const char *source_path,
                              ImportStack *stack,
                              ImportStack *loaded,
                              ImportStack *loaded_stdlib_modules,
                              size_t *import_module_counter,
                              bool imported,
                              bool is_stdlib_module,
                              const char *stdlib_module_name,
                              const char *private_prefix,
                              char **error_message)
{
    ASTNode *ast = NULL;
    char *base_dir = NULL;
    char *canonical_source = import_resolver_canonicalize_path_dup(source_path);

    if (canonical_source == NULL) {
        set_error(error_message, "out of memory while canonicalizing path '%s'",
                  source_path != NULL ? source_path : "(null)");
        return NULL;
    }

    if (imported
        && (import_stack_contains(loaded, canonical_source)
            || (is_stdlib_module
                && loaded_stdlib_modules != NULL
                && stdlib_module_name != NULL
                && import_stack_contains(loaded_stdlib_modules, stdlib_module_name)))) {
        free(canonical_source);
        return ast_create_program();
    }

    if (import_stack_contains(stack, canonical_source)) {
        set_error(error_message, "circular import detected at '%s'", source_path);
        free(canonical_source);
        return NULL;
    }
    if (!import_stack_push(stack, canonical_source)) {
        set_error(error_message, "out of memory while tracking imports");
        free(canonical_source);
        return NULL;
    }

    ast = parse_program_file(canonical_source, error_message);
    if (ast == NULL)
        goto fail;

    if (!module_normalize_ast(ast, imported, private_prefix)) {
        set_error(error_message, "failed to normalize module '%s'", source_path);
        goto fail;
    }

    if (imported) {
        if (!import_stack_push(loaded, canonical_source)) {
            set_error(error_message, "out of memory while tracking loaded modules");
            goto fail;
        }
        if (is_stdlib_module
            && loaded_stdlib_modules != NULL
            && stdlib_module_name != NULL
            && !import_stack_contains(loaded_stdlib_modules, stdlib_module_name)
            && !import_stack_push(loaded_stdlib_modules, stdlib_module_name)) {
            set_error(error_message, "out of memory while tracking loaded stdlib modules");
            goto fail;
        }
    }

    base_dir = path_dirname_dup(canonical_source);
    if (base_dir == NULL) {
        set_error(error_message, "out of memory while resolving imports");
        goto fail;
    }

    for (size_t i = 0; i < ast_program_statement_count(ast); i++) {
        ASTNode *stmt = ast_program_statement(ast, i);
        if (stmt == NULL || (stmt->type != AST_IMPORT_DECL && stmt->type != AST_USE_DECL))
            continue;

        const char *import_path = NULL;
        char *full_path = NULL;
        ASTNode *imp_ast = NULL;

        if (stmt->type == AST_IMPORT_DECL) {
            import_path = ast_import_path(stmt);
            full_path = path_join_dup(base_dir, import_path);
            if (full_path == NULL) {
                set_error(error_message, "out of memory while resolving import '%s'", import_path);
                goto fail;
            }
        } else {
            import_path = ast_use_module_name(stmt);
            full_path = import_resolver_resolve_stdlib_module_path(canonical_source,
                import_path);
            if (full_path == NULL) {
                set_error(error_message, "cannot resolve stdlib module '%s'", import_path);
                goto fail;
            }
        }

        {
            char *canonical_full_path = import_resolver_canonicalize_path_dup(full_path);
            if (canonical_full_path == NULL) {
                free(full_path);
                set_error(error_message, "out of memory while canonicalizing import '%s'", import_path);
                goto fail;
            }
            free(full_path);
            full_path = canonical_full_path;
        }

        if (import_stack_contains(stack, full_path)) {
            set_error(error_message, "circular import detected at '%s'", full_path);
            free(full_path);
            goto fail;
        }

        {
            char nested_prefix[64];
            snprintf(nested_prefix, sizeof(nested_prefix),
                     "__imp%zu_", (*import_module_counter)++);
            imp_ast = import_resolver_load_internal(full_path,
                                                    stack,
                                                    loaded,
                                                    loaded_stdlib_modules,
                                                    import_module_counter,
                                                    true,
                                                    stmt->type == AST_USE_DECL,
                                                    stmt->type == AST_USE_DECL ? import_path : NULL,
                                                    nested_prefix,
                                                    error_message);
        }
        free(full_path);

        if (imp_ast == NULL)
            goto fail;

        size_t imp_count = ast_program_statement_count(imp_ast);
        if (imp_count > 0) {
            size_t old_count = ast_program_statement_count(ast);
            ASTNode **old_statements = ast_program_statements(ast, NULL);
            ASTNode **imported_statements = ast_program_statements(imp_ast, NULL);
            ASTNode *import_stmt = ast_program_statement(ast, i);
            size_t new_count;
            if (old_count == 0 || imp_count > SIZE_MAX - (old_count - 1)) {
                ast_destroy(imp_ast);
                set_error(error_message, "import merge statement count overflow");
                goto fail;
            }
            new_count = old_count - 1 + imp_count;
            if (new_count > SIZE_MAX / sizeof(ASTNode *)) {
                ast_destroy(imp_ast);
                set_error(error_message, "import merge allocation size overflow");
                goto fail;
            }
            ASTNode **new_stmts = malloc(new_count * sizeof(ASTNode *));
            if (new_stmts == NULL) {
                ast_destroy(imp_ast);
                set_error(error_message, "out of memory while merging imports");
                goto fail;
            }

            for (size_t j = 0; j < i; j++)
                new_stmts[j] = old_statements[j];
            for (size_t j = 0; j < imp_count; j++)
                new_stmts[i + j] = imported_statements[j];
            for (size_t j = i + 1; j < old_count; j++)
                new_stmts[j - 1 + imp_count] = old_statements[j];

            if (!ast_program_replace_statements(ast,
                                                new_stmts,
                                                new_count,
                                                new_count)) {
                free(new_stmts);
                ast_destroy(imp_ast);
                set_error(error_message, "failed to replace import statements");
                goto fail;
            }
            ast_destroy(import_stmt);

            (void)ast_program_replace_statements(imp_ast, NULL, 0, 0);
            i += imp_count - 1;
        } else {
            size_t old_count = ast_program_statement_count(ast);
            ASTNode **old_statements = ast_program_statements(ast, NULL);
            ASTNode *import_stmt = ast_program_statement(ast, i);
            size_t new_count = old_count > 0 ? old_count - 1 : 0;
            ASTNode **new_stmts = NULL;
            if (new_count > 0) {
                new_stmts = malloc(new_count * sizeof(ASTNode *));
                if (new_stmts == NULL) {
                    ast_destroy(imp_ast);
                    set_error(error_message,
                              "out of memory while removing import");
                    goto fail;
                }
                for (size_t j = 0; j < i; j++)
                    new_stmts[j] = old_statements[j];
                for (size_t j = i + 1; j < old_count; j++)
                    new_stmts[j - 1] = old_statements[j];
            }
            if (!ast_program_replace_statements(ast,
                                                new_stmts,
                                                new_count,
                                                new_count)) {
                free(new_stmts);
                ast_destroy(imp_ast);
                set_error(error_message, "failed to remove import statement");
                goto fail;
            }
            ast_destroy(import_stmt);
            i--;
        }

        ast_destroy(imp_ast);
    }

    free(base_dir);
    free(canonical_source);
    import_stack_pop(stack);
    return ast;

fail:
    free(base_dir);
    free(canonical_source);
    ast_destroy(ast);
    import_stack_pop(stack);
    return NULL;
}

ASTNode *
import_resolver_load_program(const char *source_path, char **error_message)
{
    ImportStack stack = {0};
    ImportStack loaded = {0};
    ImportStack loaded_stdlib_modules = {0};
    size_t import_module_counter = 0;
    ASTNode *program;

    if (error_message != NULL)
        *error_message = NULL;

    program = import_resolver_load_internal(source_path,
                                            &stack,
                                            &loaded,
                                            &loaded_stdlib_modules,
                                            &import_module_counter,
                                            false,
                                            false,
                                            NULL,
                                            "",
                                            error_message);
    if (program != NULL && !ast_assign_stable_ids(program)) {
        set_error(error_message,
                  "syntax node identity space exhausted after import merge");
        ast_destroy(program);
        program = NULL;
    }
    import_stack_destroy(&stack);
    import_stack_destroy(&loaded);
    import_stack_destroy(&loaded_stdlib_modules);
    return program;
}
