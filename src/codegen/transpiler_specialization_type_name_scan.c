/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend type-name and MIR-routine specialization scan owner.
 */

#include "transpiler_specialization_registry.h"

#include <stdbool.h>
#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../parser/ast.h"
#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_generic_class_specialization.h"
#include "transpiler_generic_param_query.h"
#include "transpiler_inventory_view.h"

static char *
transpiler_specialization_trim_copy(const char *begin, size_t len)
{
    const char *end;

    if (begin == NULL)
        return NULL;
    end = begin + len;
    while (begin < end && isspace((unsigned char)*begin))
        begin++;
    while (end > begin && isspace((unsigned char)*(end - 1)))
        end--;
    if (end == begin)
        return NULL;
    return pergyra_strndup(begin, (size_t)(end - begin));
}

static bool
transpiler_specialization_type_name_has_base(const char *type_name,
                                             const char *base)
{
    size_t base_len;

    if (type_name == NULL || base == NULL)
        return false;
    base_len = strlen(base);
    return strncmp(type_name, base, base_len) == 0
        && type_name[base_len] == '<';
}

static char *
transpiler_specialization_type_name_arg_copy(const char *type_name,
                                             size_t wanted_index)
{
    const char *open;
    const char *start;
    size_t index = 0;
    int depth = 0;

    if (type_name == NULL)
        return NULL;
    open = strchr(type_name, '<');
    if (open == NULL)
        return NULL;
    start = open + 1;
    for (const char *p = start; *p != '\0'; p++) {
        if (*p == '<') {
            depth++;
        } else if (*p == '>') {
            if (depth == 0) {
                return index == wanted_index
                    ? transpiler_specialization_trim_copy(
                        start, (size_t)(p - start))
                    : NULL;
            }
            depth--;
        } else if (*p == ',' && depth == 0) {
            if (index == wanted_index) {
                return transpiler_specialization_trim_copy(
                    start, (size_t)(p - start));
            }
            index++;
            start = p + 1;
        }
    }
    return NULL;
}

static char *
transpiler_specialization_type_name_base_copy(const char *type_name)
{
    const char *open;

    if (type_name == NULL)
        return NULL;
    open = strchr(type_name, '<');
    if (open == NULL)
        return transpiler_specialization_trim_copy(type_name,
            strlen(type_name));
    return transpiler_specialization_trim_copy(type_name,
        (size_t)(open - type_name));
}

static ASTNode *
transpiler_specialization_type_ast_from_type_name(const char *type_name)
{
    char *base_name = transpiler_specialization_type_name_base_copy(type_name);
    ASTNode *node = NULL;

    if (base_name == NULL)
        return NULL;
    node = ast_create_type(base_name);
    free(base_name);
    if (node == NULL)
        return NULL;

    if (strchr(type_name, '<') == NULL)
        return node;

    for (size_t i = 0;; i++) {
        char *arg = transpiler_specialization_type_name_arg_copy(
            type_name, i);
        ASTNode *arg_type = NULL;
        const char *arg_name;

        if (arg == NULL)
            return node;
        arg_type = transpiler_specialization_type_ast_from_type_name(
            arg);
        free(arg);
        if (arg_type == NULL)
            goto fail;

        arg_name = ast_type_name(arg_type);
        if (!ast_type_append_generic_arg_owned(
                node, arg_name, arg_type, NULL)) {
            ast_destroy(arg_type);
            goto fail;
        }
    }

fail:
    ast_destroy(node);
    return NULL;
}

const char *
transpiler_ensure_generic_class_specialization_from_type_name(
    TranspilerCtx *ctx,
    const char *type_name)
{
    char *base_name;
    ASTNode *class_decl;
    ASTNode *type_ast;

    if (ctx == NULL || type_name == NULL || type_name[0] == '\0')
        return NULL;

    base_name = transpiler_specialization_type_name_base_copy(type_name);
    if (base_name == NULL)
        return NULL;
    class_decl = find_class_decl(ctx, base_name);
    free(base_name);
    if (class_decl == NULL || !transpiler_class_has_generic_params(class_decl))
        return NULL;

    type_ast = transpiler_specialization_type_ast_from_type_name(type_name);
    if (type_ast == NULL)
        return NULL;
    const char *spec_name =
        ensure_generic_class_specialization(ctx, class_decl, type_ast);
    ast_destroy(type_ast);
    return spec_name;
}

static void
transpiler_specialization_scan_type_name_args(TranspilerCtx *ctx,
                                              CodeBuf *dst,
                                              const char *type_name)
{
    for (size_t i = 0;; i++) {
        char *arg = transpiler_specialization_type_name_arg_copy(type_name, i);
        if (arg == NULL)
            return;
        ensure_type_specializations_from_type_name_to(ctx, dst, arg);
        free(arg);
    }
}

static bool
transpiler_specialization_scan_type_alias(TranspilerCtx *ctx,
                                          CodeBuf *dst,
                                          const char *type_name)
{
    const char *target_type_name;
    ASTNode *alias_decl;

    if (ctx == NULL || type_name == NULL || strchr(type_name, '<') != NULL)
        return false;
    target_type_name =
        transpiler_type_alias_target_type_name_from_headers(ctx, type_name);
    if (target_type_name != NULL) {
        ensure_type_specializations_from_type_name_to(
            ctx, dst, target_type_name);
        return true;
    }
    if (transpiler_active_has_mir(ctx))
        return false;

    alias_decl = transpiler_find_type_alias_decl(ctx, type_name);
    if (alias_decl == NULL || ast_type_alias_target_type(alias_decl) == NULL)
        return false;
    ensure_type_specializations_from_ast_to(
        ctx, dst, ast_type_alias_target_type(alias_decl));
    return true;
}

void
ensure_type_specializations_from_type_name_to(TranspilerCtx *ctx,
                                              CodeBuf *dst,
                                              const char *type_name)
{
    if (ctx == NULL || dst == NULL || type_name == NULL
        || type_name[0] == '\0') {
        return;
    }
    if (type_name[0] == '(') {
        ensure_tuple_specialization_from_type_name_to(ctx, dst, type_name);
        return;
    }
    if (transpiler_specialization_scan_type_alias(ctx, dst, type_name))
        return;

    transpiler_specialization_scan_type_name_args(ctx, dst, type_name);
    if (transpiler_ensure_generic_class_specialization_from_type_name(
            ctx, type_name) != NULL) {
        return;
    }

    if (transpiler_specialization_type_name_has_base(type_name, "List")) {
        char *inner = transpiler_specialization_type_name_arg_copy(type_name, 0);
        ensure_collection_specialization_to(ctx, dst, "List", inner);
        free(inner);
    } else if (transpiler_specialization_type_name_has_base(
                   type_name, "Queue")) {
        char *inner = transpiler_specialization_type_name_arg_copy(type_name, 0);
        ensure_collection_specialization_to(ctx, dst, "Queue", inner);
        free(inner);
    } else if (transpiler_specialization_type_name_has_base(
                   type_name, "HashMap")) {
        char *value = transpiler_specialization_type_name_arg_copy(type_name, 1);
        ensure_collection_specialization_to(ctx, dst, "Map", value);
        free(value);
    } else if (transpiler_specialization_type_name_has_base(
                   type_name, "Result")) {
        char *ok = transpiler_specialization_type_name_arg_copy(type_name, 0);
        char *err = transpiler_specialization_type_name_arg_copy(type_name, 1);
        ensure_result_specialization_to(ctx, dst, ok, err);
        free(ok);
        free(err);
    } else if (transpiler_specialization_type_name_has_base(
                   type_name, "Option")) {
        char *inner = transpiler_specialization_type_name_arg_copy(type_name, 0);
        ensure_option_specialization_to(ctx, dst, inner);
        free(inner);
    } else if (transpiler_specialization_type_name_has_base(
                   type_name, "Array")) {
        char *inner = transpiler_specialization_type_name_arg_copy(type_name, 0);
        ensure_collection_specialization_to(ctx, dst, "Array", inner);
        free(inner);
    }
}

void
ensure_collection_specializations_from_mir_routine_to(TranspilerCtx *ctx,
                                                      CodeBuf *dst,
                                                      const MIRRoutine *routine)
{
    if (ctx == NULL || dst == NULL || routine == NULL)
        return;

    const char *return_type_name =
        transpiler_mir_routine_return_type_name(routine);
    if (return_type_name != NULL) {
        ensure_type_specializations_from_type_name_to(ctx, dst,
            return_type_name);
    } else {
        ensure_type_specializations_from_ast_to(ctx, dst,
            transpiler_mir_routine_return_type(routine));
    }

    for (size_t i = 0; i < transpiler_mir_routine_param_count(routine); i++) {
        const char *param_type_name =
            transpiler_mir_routine_param_type_name(routine, i);
        if (param_type_name != NULL) {
            ensure_type_specializations_from_type_name_to(ctx, dst,
                param_type_name);
        } else {
            FuncParam *param = transpiler_mir_routine_param(routine, i);
            if (param != NULL)
                ensure_type_specializations_from_ast_to(ctx, dst, param->type);
        }
    }

    for (size_t i = 0;
         i < transpiler_mir_routine_source_local_type_count(routine);
         i++) {
        ensure_type_specializations_from_type_name_to(
            ctx,
            dst,
            transpiler_mir_routine_source_local_type_name_at(routine, i));
    }
}
