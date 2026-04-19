/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Semantic Analyzer — unified entry point
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../common/string_compat.h"
#include "semantic.h"
#include "type_checker.h"
#include "slot_analyzer.h"
#include "../compiler/import_resolver.h"

static bool
semantic_program_append_statement(ASTNode *program, ASTNode *stmt)
{
    ASTNode **grown;

    if (program == NULL || program->type != AST_PROGRAM || stmt == NULL)
        return false;

    grown = realloc(program->data.program.statements,
        sizeof(ASTNode *) * (program->data.program.count + 1));
    if (grown == NULL)
        return false;

    program->data.program.statements = grown;
    program->data.program.statements[program->data.program.count++] = stmt;
    return true;
}

static bool
semantic_program_has_module_name(char **module_names, size_t module_count,
                                 const char *module_name)
{
    for (size_t i = 0; i < module_count; i++) {
        if (module_names[i] != NULL && module_name != NULL
            && strcmp(module_names[i], module_name) == 0) {
            return true;
        }
    }

    return false;
}

static void
semantic_preload_stdlib_uses(ASTNode *ast)
{
    char  **loaded_modules = NULL;
    size_t  loaded_count = 0;

    if (ast == NULL || ast->type != AST_PROGRAM)
        return;

    for (size_t i = 0; i < ast->data.program.count; i++) {
        ASTNode *stmt = ast->data.program.statements[i];
        char *module_path = NULL;
        ASTNode *loaded = NULL;
        char *error_message = NULL;
        size_t module_path_len;

        if (stmt == NULL || stmt->type != AST_USE_DECL
            || stmt->data.use_decl.module_name == NULL) {
            continue;
        }

        if (semantic_program_has_module_name(loaded_modules, loaded_count,
                stmt->data.use_decl.module_name)) {
            continue;
        }

        module_path_len = strlen(PGY_PROJECT_ROOT) + strlen("/stdlib/")
            + strlen(stmt->data.use_decl.module_name) + strlen(".pgy") + 1;
        module_path = malloc(module_path_len);
        if (module_path == NULL)
            continue;

        snprintf(module_path, module_path_len, "%s/stdlib/%s.pgy",
            PGY_PROJECT_ROOT, stmt->data.use_decl.module_name);
        loaded = import_resolver_load_program(module_path, &error_message);
        free(module_path);
        free(error_message);
        if (loaded == NULL || loaded->type != AST_PROGRAM) {
            ast_destroy(loaded);
            continue;
        }

        {
            char **grown = realloc(loaded_modules, sizeof(char *) * (loaded_count + 1));
            if (grown != NULL) {
                loaded_modules = grown;
                loaded_modules[loaded_count] =
                    pergyra_strdup(stmt->data.use_decl.module_name);
                if (loaded_modules[loaded_count] != NULL)
                    loaded_count++;
            }
        }

        for (size_t j = 0; j < loaded->data.program.count; j++) {
            ASTNode *imported_stmt = loaded->data.program.statements[j];
            bool explicit_private = false;

            if (imported_stmt == NULL
                || imported_stmt->type == AST_IMPORT_DECL
                || imported_stmt->type == AST_USE_DECL) {
                continue;
            }

            explicit_private = imported_stmt->has_explicit_access
                && (imported_stmt->access == ACCESS_PRIVATE
                    || imported_stmt->access == ACCESS_PROTECTED);
            imported_stmt->is_exported = !explicit_private;

            if (!semantic_program_append_statement(ast, imported_stmt)) {
                break;
            }
            loaded->data.program.statements[j] = NULL;
        }

        ast_destroy(loaded);
    }

    for (size_t i = 0; i < loaded_count; i++)
        free(loaded_modules[i]);
    free(loaded_modules);
}

SemanticResult *
semantic_analyze(ASTNode *ast)
{
    SemanticResult *result = calloc(1, sizeof(SemanticResult));
    if (result == NULL)
        return NULL;

    SemanticContext *ctx = semantic_context_create();
    if (ctx == NULL) {
        free(result);
        return NULL;
    }

    semantic_preload_stdlib_uses(ast);

    /* Pass 1 + 2: Symbol collection and type checking */
    type_check_program(ast, ctx);

    /* Pass 3: Slot lifetime analysis */
    if (!ctx->has_error) {
        SlotAnalyzer *sa = slot_analyzer_create(ctx);
        if (sa != NULL) {
            slot_analyze_program(ast, sa);
            slot_analyzer_destroy(sa);
        }
    }

    /* Transfer diagnostics to result */
    result->success          = !ctx->has_error;
    result->annotated_ast    = ast;
    result->diagnostic_count = ctx->diagnostic_count;
    result->diagnostics      = ctx->diagnostics;

    for (size_t i = 0; i < ctx->diagnostic_count; i++) {
        if (ctx->diagnostics[i]->level == DIAG_ERROR)
            result->error_count++;
        else
            result->warning_count++;
    }

    /*
     * Transfer ownership of diagnostics array to result.
     * Set ctx pointer to NULL so context_destroy doesn't double-free.
     */
    ctx->diagnostics      = NULL;
    ctx->diagnostic_count = 0;
    ctx->diagnostic_capacity = 0;

    semantic_context_destroy(ctx);
    return result;
}

void
semantic_result_destroy(SemanticResult *result)
{
    if (result == NULL)
        return;

    for (size_t i = 0; i < result->diagnostic_count; i++) {
        free(result->diagnostics[i]->message);
        free(result->diagnostics[i]);
    }
    free(result->diagnostics);
    free(result);
}

void
semantic_result_print(const SemanticResult *result)
{
    if (result == NULL)
        return;

    for (size_t i = 0; i < result->diagnostic_count; i++) {
        Diagnostic *d = result->diagnostics[i];
        const char *level = (d->level == DIAG_ERROR) ? "ERROR" : "WARNING";
        fprintf(stderr, "[%s] %u:%u - %s\n",
                level, d->line, d->col, d->message);
    }

    fprintf(stderr, "\n%zu error(s), %zu warning(s)\n",
            result->error_count, result->warning_count);
}

/* Emit a JSON-escaped string literal including the surrounding quotes.
 * Handles ASCII control characters, backslash, and double-quote. Non-ASCII
 * bytes are passed through (source is assumed UTF-8). */
static void
json_emit_string(FILE *out, const char *s)
{
    fputc('"', out);
    if (s == NULL) {
        fputc('"', out);
        return;
    }
    for (const unsigned char *p = (const unsigned char *)s; *p != '\0'; p++) {
        unsigned char c = *p;
        switch (c) {
        case '"':  fputs("\\\"", out); break;
        case '\\': fputs("\\\\", out); break;
        case '\b': fputs("\\b", out);  break;
        case '\f': fputs("\\f", out);  break;
        case '\n': fputs("\\n", out);  break;
        case '\r': fputs("\\r", out);  break;
        case '\t': fputs("\\t", out);  break;
        default:
            if (c < 0x20) {
                fprintf(out, "\\u%04x", c);
            } else {
                fputc((int)c, out);
            }
        }
    }
    fputc('"', out);
}

void
semantic_result_print_json(const SemanticResult *result)
{
    FILE *out = stderr;
    if (result == NULL || result->diagnostic_count == 0) {
        fputs("[]\n", out);
        return;
    }

    fputc('[', out);
    for (size_t i = 0; i < result->diagnostic_count; i++) {
        Diagnostic *d = result->diagnostics[i];
        const char *severity = (d->level == DIAG_ERROR) ? "error" : "warning";
        if (i > 0)
            fputc(',', out);
        fputs("{\"severity\":", out);
        json_emit_string(out, severity);
        fputs(",\"stage\":\"semantic\"", out);
        if (d->code != NULL) {
            fputs(",\"code\":", out);
            json_emit_string(out, d->code);
        }
        if (d->cause_ir != NULL) {
            fputs(",\"cause_ir\":", out);
            json_emit_string(out, d->cause_ir);
        }
        if (d->fix_source != NULL) {
            fputs(",\"fix_source\":", out);
            json_emit_string(out, d->fix_source);
        }
        fputs(",\"location\":{\"line\":", out);
        fprintf(out, "%u,\"column\":%u}", d->line, d->col);
        fputs(",\"message\":", out);
        json_emit_string(out, d->message != NULL ? d->message : "");
        fputc('}', out);
    }
    fputs("]\n", out);
}
