/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Semantic Analyzer — unified entry point
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "../common/string_compat.h"
#include "semantic.h"
#include "diag_payload.h"
#include "type_checker.h"
#include "slot_analyzer.h"
#include "lifecycle_analyze.h"
#include "../compiler/import_resolver.h"

static bool
semantic_loaded_modules_append(char ***module_names,
                               size_t *count,
                               size_t *capacity,
                               const char *module_name)
{
    char **grown;
    size_t new_capacity;
    char *copy;

    if (module_names == NULL || count == NULL || capacity == NULL
        || module_name == NULL) {
        return false;
    }

    if (*count >= *capacity) {
        if (*capacity == 0) {
            new_capacity = 8;
        } else {
            if (*capacity > SIZE_MAX / 2)
                return false;
            new_capacity = *capacity * 2;
        }
        if (new_capacity > SIZE_MAX / sizeof(char *)) {
            return false;
        }
        grown = realloc(*module_names, sizeof(char *) * new_capacity);
        if (grown == NULL)
            return false;
        *module_names = grown;
        *capacity = new_capacity;
    }

    copy = pergyra_strdup(module_name);
    if (copy == NULL)
        return false;
    (*module_names)[(*count)++] = copy;
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
    size_t  loaded_capacity = 0;
    PgyArena path_arena;

    if (ast == NULL || ast->type != AST_PROGRAM)
        return;

    /* Per-pass scratch arena for module path assembly.  Each iteration's
     * path string is short-lived and never escapes: it is consumed by
     * import_resolver_load_program and then unused.  Using a function-local
     * arena batches the allocations and removes N malloc/free pairs. */
    pgy_arena_init(&path_arena, 0);

    for (size_t i = 0; i < ast_program_statement_count(ast); i++) {
        ASTNode *stmt = ast_program_statement(ast, i);
        char *module_path = NULL;
        ASTNode *loaded = NULL;
        char *error_message = NULL;

        if (stmt == NULL || stmt->type != AST_USE_DECL
            || ast_use_module_name(stmt) == NULL) {
            continue;
        }

        if (semantic_program_has_module_name(loaded_modules, loaded_count,
                ast_use_module_name(stmt))) {
            continue;
        }

        module_path = pgy_arena_fmt(&path_arena, "%s/stdlib/%s.pgy",
            PGY_PROJECT_ROOT, ast_use_module_name(stmt));
        if (module_path == NULL)
            continue;

        loaded = import_resolver_load_program(module_path, &error_message);
        /* module_path is arena-owned: no free here. */
        free(error_message);
        if (loaded == NULL || loaded->type != AST_PROGRAM) {
            ast_destroy(loaded);
            continue;
        }

        (void)semantic_loaded_modules_append(&loaded_modules,
                                             &loaded_count,
                                             &loaded_capacity,
                                             ast_use_module_name(stmt));

        for (size_t j = 0; j < ast_program_statement_count(loaded); j++) {
            ASTNode *imported_stmt = ast_program_statement(loaded, j);
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

            if (!ast_program_append_statement(ast, imported_stmt)) {
                break;
            }
            (void)ast_program_detach_statement(loaded, j);
        }

        ast_destroy(loaded);
    }

    for (size_t i = 0; i < loaded_count; i++)
        free(loaded_modules[i]);
    free(loaded_modules);
    pgy_arena_destroy(&path_arena);
}

static void
semantic_run_legacy_slot_resource_analysis(ASTNode *ast, SemanticContext *ctx)
{
    SlotAnalyzer *sa;

    if (ctx == NULL || ctx->has_error)
        return;

    /*
     * Domain-lifecycle analysis (N-state engine). Consumes parser-owned
     * AST_LIFECYCLE_DECL nodes; lifecycle-free programs remain a no-op.
     */
    if (!lifecycle_analyze_program(ast, ctx))
        return;

    /*
     * Compatibility seam: CFG/MIR owns beta body-safety truth. The legacy
     * slot analyzer remains here only for conservative slot escape/leak
     * provenance until those warnings are fully backed by CFG/MIR facts.
     */
    sa = slot_analyzer_create(ctx);
    if (sa != NULL) {
        if (!slot_analyze_program(ast, sa) && !ctx->has_error) {
            semantic_error(ctx, ast,
                "Slot resource-boundary analysis failed before it could "
                "produce a specific diagnostic");
        }
        slot_analyzer_destroy(sa);
        return;
    }

    semantic_error(ctx, ast,
        "Slot resource-boundary analysis could not allocate state");
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

    semantic_run_legacy_slot_resource_analysis(ast, ctx);

    /* Transfer diagnostics to result */
    result->success          = !ctx->has_error;
    result->annotated_ast    = ast;
    result->diagnostic_count = ctx->diagnostic_count;
    result->diagnostics      = ctx->diagnostics;
    result->type_resolution_metadata_entries =
        ctx->type_resolution_metadata.count;
    result->type_resolution_metadata_hits =
        ctx->type_resolution_metadata_hits;
    result->type_resolution_metadata_dead_ends =
        ctx->type_resolution_metadata_dead_ends;
    result->type_resolution_dag_generic_contract_evidence_count =
        ctx->type_resolution_dag_generic_contract_evidence_count;
    result->type_resolution_dag_ability_consumer_evidence_count =
        ctx->type_resolution_dag_ability_consumer_evidence_count;
    result->boundary_witness_summary = ctx->boundary_witness_summary;
    result->program_capabilities = ctx->program_capabilities;
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
        diag_payload_snapshot_destroy(result->diagnostics[i]->payload);
        free(result->diagnostics[i]);
    }
    free(result->diagnostics);
    free(result);
}

size_t
semantic_result_type_resolution_metadata_entries(
        const SemanticResult *result)
{
    return result != NULL ? result->type_resolution_metadata_entries : 0;
}

size_t
semantic_result_type_resolution_metadata_hits(
        const SemanticResult *result)
{
    return result != NULL ? result->type_resolution_metadata_hits : 0;
}

size_t
semantic_result_type_resolution_metadata_dead_ends(
        const SemanticResult *result)
{
    return result != NULL ? result->type_resolution_metadata_dead_ends : 0;
}

size_t
semantic_result_dag_generic_contract_evidence_count(
        const SemanticResult *result)
{
    return result != NULL
        ? result->type_resolution_dag_generic_contract_evidence_count
        : 0;
}

size_t
semantic_result_dag_ability_consumer_evidence_count(
        const SemanticResult *result)
{
    return result != NULL
        ? result->type_resolution_dag_ability_consumer_evidence_count
        : 0;
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
        fputs(",\"layer\":", out);
        json_emit_string(out, diagnostic_layer_name(d->layer));
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
        if (d->payload != NULL) {
            bool wrote = false;
            fputs(",\"payload\":{", out);
#define PGY_JSON_PAYLOAD_FIELD(key, value) \
            do { \
                if ((value) != NULL) { \
                    if (wrote) \
                        fputc(',', out); \
                    json_emit_string(out, (key)); \
                    fputc(':', out); \
                    json_emit_string(out, (value)); \
                    wrote = true; \
                } \
            } while (0)
            PGY_JSON_PAYLOAD_FIELD("value_label", d->payload->value_label);
            PGY_JSON_PAYLOAD_FIELD("provenance_label", d->payload->provenance_label);
            PGY_JSON_PAYLOAD_FIELD("replacement_label", d->payload->replacement_label);
            PGY_JSON_PAYLOAD_FIELD("transfer_label", d->payload->transfer_label);
            PGY_JSON_PAYLOAD_FIELD("borrowed_name", d->payload->borrowed_name);
            PGY_JSON_PAYLOAD_FIELD("consumer_name", d->payload->consumer_name);
            PGY_JSON_PAYLOAD_FIELD("secondary_name", d->payload->secondary_name);
            PGY_JSON_PAYLOAD_FIELD("kind_label", d->payload->kind_label);
            PGY_JSON_PAYLOAD_FIELD("extra", d->payload->extra);
#undef PGY_JSON_PAYLOAD_FIELD
            fputc('}', out);
        }
        fputc('}', out);
    }
    fputs("]\n", out);
}
