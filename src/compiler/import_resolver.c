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
#include <string.h>

#include "../common/string_compat.h"
#include "../lexer/lexer.h"
#include "../parser/ast_analysis.h"
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

    ast = parser_parse_program_for_module_composition(parser);
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
        if (!ast_program_splice_take(ast, i, imp_ast)) {
            ast_destroy(imp_ast);
            set_error(error_message, "failed to splice imported statements");
            goto fail;
        }
        if (imp_count > 0)
            i += imp_count - 1;
        else
            i--;

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

static bool
import_resolver_is_try_file_builtin(const char *name, void *userdata)
{
    (void)userdata;
    return name != NULL
        && (strcmp(name, "TryReadFile") == 0
            || strcmp(name, "TryWriteFile") == 0);
}

/* A program that calls TryReadFile or TryWriteFile gets the builtin IoError
 * enum (src/runtime/pgy_runtime_io_error.def) as one more root declaration,
 * appended after the composed statements so no source line moves. The
 * self-host parser composes the same declaration
 * (src/self_hosted/parser/io_error_builtin_enum_composition_owner.pgy). */
static bool
import_resolver_compose_io_error_enum(ASTNode *program,
                                      const char *source_path,
                                      char **error_message)
{
    static const char *const variants[] = {
#define PGY_IO_ERROR_VARIANT(variant_name, runtime_status) #variant_name,
#include "../runtime/pgy_runtime_io_error.def"
#undef PGY_IO_ERROR_VARIANT
    };
    char source[512];
    size_t used;
    Lexer *lexer;
    Parser *parser;
    ASTNode *decls;
    bool ok = false;

    if (!ast_contains_identifier_call(program,
            import_resolver_is_try_file_builtin, NULL))
        return true;
    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        const ASTNode *statement = ast_program_statement(program, i);
        const char *declared = ast_declaration_name(statement);
        if (declared != NULL && strcmp(declared, "IoError") == 0) {
            set_error(error_message,
                "%s:%u: this program declares IoError, which is the builtin "
                "error enum of TryReadFile and TryWriteFile; rename the "
                "declaration",
                statement->origin_path != NULL ? statement->origin_path
                                               : source_path,
                (unsigned)statement->line);
            return false;
        }
    }
    source[0] = '\0';
    used = pergyra_str_append(source, sizeof(source), "enum IoError {");
    for (size_t i = 0; i < sizeof(variants) / sizeof(variants[0]); i++)
        used = pergyra_str_appendf(source, sizeof(source), "%s %s",
                                   i == 0 ? "" : ",", variants[i]);
    used = pergyra_str_append(source, sizeof(source), " }\n");
    /* The bounded appends stop at the last byte, so a full buffer means the
     * declaration was cut short. */
    if (used >= sizeof(source) - 1) {
        set_error(error_message, "builtin IoError declaration exceeds its buffer");
        return false;
    }
    lexer = lexer_create(source);
    parser = lexer != NULL ? parser_create(lexer) : NULL;
    if (parser == NULL) {
        set_error(error_message, "out of memory while composing IoError");
        lexer_destroy(lexer);
        return false;
    }
    parser->source_path = source_path;
    decls = parser_parse_program_for_module_composition(parser);
    if (decls == NULL || parser_has_error(parser)) {
        set_error(error_message, "builtin IoError declaration did not parse: %s",
                  parser_get_error(parser));
    } else {
        /* detach leaves a NULL slot behind, so walk the indices. */
        ok = true;
        for (size_t i = 0; ok && i < ast_program_statement_count(decls); i++) {
            ASTNode *statement = ast_program_detach_statement(decls, i);
            ok = statement != NULL
                && ast_program_append_statement(program, statement);
            if (!ok)
                ast_destroy(statement);
        }
        if (!ok)
            set_error(error_message, "could not append the builtin IoError declaration");
    }
    ast_destroy(decls);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return ok;
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
    if (program != NULL
        && !import_resolver_compose_io_error_enum(program, source_path,
                                                  error_message)) {
        ast_destroy(program);
        program = NULL;
    }
    if (program != NULL) {
        char *composition_error = NULL;
        if (!parser_finalize_composed_intent_parameter_roles(
                program, &composition_error)) {
            set_error(error_message,
                "declaration composition failed: %s",
                composition_error != NULL ? composition_error
                                          : "intent parameter owner unavailable");
            free(composition_error);
            ast_destroy(program);
            program = NULL;
        }
    }
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
