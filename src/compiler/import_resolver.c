/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include "import_resolver.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include "module_normalizer.h"
#include "path_utils.h"

typedef struct
{
    char  **paths;
    size_t  count;
    size_t  capacity;
} ImportStack;

static void
set_error(char **error_message, const char *fmt, ...)
{
    char buffer[1024];
    va_list args;

    if (error_message == NULL)
        return;

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    free(*error_message);
    *error_message = pergyra_strdup(buffer);
}

/* read_file_text, path_dirname_dup, path_join_dup are now in path_utils.h */
#define read_file_text path_read_file

static bool
import_stack_push(ImportStack *stack, const char *path)
{
    if (stack->count == stack->capacity) {
        size_t next = stack->capacity == 0 ? 8 : stack->capacity * 2;
        char **grown = realloc(stack->paths, next * sizeof(char *));
        if (grown == NULL)
            return false;
        stack->paths = grown;
        stack->capacity = next;
    }
    stack->paths[stack->count] = pergyra_strdup(path);
    if (stack->paths[stack->count] == NULL)
        return false;
    stack->count++;
    return true;
}

static bool
import_stack_contains(const ImportStack *stack, const char *path)
{
    for (size_t i = 0; i < stack->count; i++) {
        if (strcmp(stack->paths[i], path) == 0)
            return true;
    }
    return false;
}

static void
import_stack_pop(ImportStack *stack)
{
    if (stack->count == 0)
        return;
    free(stack->paths[stack->count - 1]);
    stack->count--;
}

static void
import_stack_destroy(ImportStack *stack)
{
    while (stack->count > 0)
        import_stack_pop(stack);
    free(stack->paths);
    stack->paths = NULL;
    stack->capacity = 0;
}

/* path_dirname_dup and path_join_dup are now in path_utils.c */

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

static bool
path_file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        return false;
    fclose(f);
    return true;
}

static char *
path_parent_dir_dup(const char *path)
{
    char *dir = path_dirname_dup(path);
    char *parent;

    if (dir == NULL)
        return NULL;
    if (strcmp(dir, ".") == 0 || strcmp(dir, "/") == 0 || strcmp(dir, "\\") == 0)
        return dir;

    parent = path_dirname_dup(dir);
    free(dir);
    return parent;
}

static char *
resolve_stdlib_module_path(const char *source_path, const char *module_name)
{
    char *search_dir = NULL;
    char *module_file = NULL;
    char *resolved = NULL;

    if (source_path == NULL || module_name == NULL)
        return NULL;

    search_dir = path_dirname_dup(source_path);
    module_file = malloc(strlen(module_name) + 5);
    if (search_dir == NULL || module_file == NULL)
        goto cleanup;

    sprintf(module_file, "%s.pgy", module_name);

    while (search_dir != NULL) {
        char *stdlib_dir = path_join_dup(search_dir, "stdlib");
        char *candidate = stdlib_dir != NULL ? path_join_dup(stdlib_dir, module_file) : NULL;
        char *parent = NULL;

        free(stdlib_dir);

        if (candidate != NULL && path_file_exists(candidate)) {
            resolved = candidate;
            break;
        }
        free(candidate);

        parent = path_parent_dir_dup(search_dir);
        if (parent == NULL || strcmp(parent, search_dir) == 0) {
            free(parent);
            break;
        }
        free(search_dir);
        search_dir = parent;
    }

    if (resolved == NULL) {
        char *candidate = path_join_dup("stdlib", module_file);
        if (candidate != NULL && path_file_exists(candidate))
            resolved = candidate;
        else
            free(candidate);
    }

cleanup:
    free(search_dir);
    free(module_file);
    return resolved;
}

static ASTNode *
import_resolver_load_internal(const char *source_path,
                              ImportStack *stack,
                              size_t *import_module_counter,
                              bool imported,
                              const char *private_prefix,
                              char **error_message)
{
    ASTNode *ast = NULL;
    char *base_dir = NULL;

    if (import_stack_contains(stack, source_path)) {
        set_error(error_message, "circular import detected at '%s'", source_path);
        return NULL;
    }
    if (!import_stack_push(stack, source_path)) {
        set_error(error_message, "out of memory while tracking imports");
        return NULL;
    }

    ast = parse_program_file(source_path, error_message);
    if (ast == NULL)
        goto fail;

    if (!module_normalize_ast(ast, imported, private_prefix)) {
        set_error(error_message, "failed to normalize module '%s'", source_path);
        goto fail;
    }

    base_dir = path_dirname_dup(source_path);
    if (base_dir == NULL) {
        set_error(error_message, "out of memory while resolving imports");
        goto fail;
    }

    for (size_t i = 0; i < ast->data.program.count; i++) {
        ASTNode *stmt = ast->data.program.statements[i];
        if (stmt == NULL || (stmt->type != AST_IMPORT_DECL && stmt->type != AST_USE_DECL))
            continue;

        const char *import_path = NULL;
        char *full_path = NULL;
        ASTNode *imp_ast = NULL;

        if (stmt->type == AST_IMPORT_DECL) {
            import_path = stmt->data.import_decl.path;
            full_path = path_join_dup(base_dir, import_path);
            if (full_path == NULL) {
                set_error(error_message, "out of memory while resolving import '%s'", import_path);
                goto fail;
            }
        } else {
            import_path = stmt->data.use_decl.module_name;
            full_path = resolve_stdlib_module_path(source_path, import_path);
            if (full_path == NULL) {
                set_error(error_message, "cannot resolve stdlib module '%s'", import_path);
                goto fail;
            }
        }

        {
            char nested_prefix[64];
            snprintf(nested_prefix, sizeof(nested_prefix),
                     "__imp%zu_", (*import_module_counter)++);
            imp_ast = import_resolver_load_internal(full_path,
                                                    stack,
                                                    import_module_counter,
                                                    true,
                                                    nested_prefix,
                                                    error_message);
        }
        free(full_path);

        if (imp_ast == NULL)
            goto fail;

        size_t imp_count = imp_ast->data.program.count;
        if (imp_count > 0) {
            size_t old_count = ast->data.program.count;
            size_t new_count = old_count - 1 + imp_count;
            ASTNode **new_stmts = malloc(new_count * sizeof(ASTNode *));
            if (new_stmts == NULL) {
                ast_destroy(imp_ast);
                set_error(error_message, "out of memory while merging imports");
                goto fail;
            }

            for (size_t j = 0; j < i; j++)
                new_stmts[j] = ast->data.program.statements[j];
            for (size_t j = 0; j < imp_count; j++)
                new_stmts[i + j] = imp_ast->data.program.statements[j];
            for (size_t j = i + 1; j < old_count; j++)
                new_stmts[j - 1 + imp_count] = ast->data.program.statements[j];

            ast_destroy(ast->data.program.statements[i]);
            free(ast->data.program.statements);
            ast->data.program.statements = new_stmts;
            ast->data.program.count = new_count;

            imp_ast->data.program.statements = NULL;
            imp_ast->data.program.count = 0;
            i += imp_count - 1;
        } else {
            ast_destroy(ast->data.program.statements[i]);
            for (size_t j = i; j + 1 < ast->data.program.count; j++)
                ast->data.program.statements[j] = ast->data.program.statements[j + 1];
            ast->data.program.count--;
            i--;
        }

        ast_destroy(imp_ast);
    }

    free(base_dir);
    import_stack_pop(stack);
    return ast;

fail:
    free(base_dir);
    ast_destroy(ast);
    import_stack_pop(stack);
    return NULL;
}

ASTNode *
import_resolver_load_program(const char *source_path, char **error_message)
{
    ImportStack stack = {0};
    size_t import_module_counter = 0;
    ASTNode *program;

    if (error_message != NULL)
        *error_message = NULL;

    program = import_resolver_load_internal(source_path,
                                            &stack,
                                            &import_module_counter,
                                            false,
                                            "",
                                            error_message);
    import_stack_destroy(&stack);
    return program;
}
