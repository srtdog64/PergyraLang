/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend tuple ABI specialization owner.
 */

#include "transpiler_specialization_registry.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "codegen_type_mapping.h"
#include "transpiler_type_render.h"
#include "transpiler_type_require.h"

static bool
transpiler_tuple_spec_copy(char *dst, size_t dst_size, const char *value)
{
    size_t len;

    if (dst == NULL || dst_size == 0 || value == NULL)
        return false;
    len = strlen(value);
    if (len >= dst_size)
        return false;
    memcpy(dst, value, len + 1);
    return true;
}

static bool
transpiler_tuple_spec_append(char *dst, size_t dst_size, const char *value)
{
    size_t used;
    size_t len;

    if (dst == NULL || dst_size == 0 || value == NULL)
        return false;
    used = strlen(dst);
    len = strlen(value);
    if (used >= dst_size || len >= dst_size - used)
        return false;
    memcpy(dst + used, value, len + 1);
    return true;
}

static void
transpiler_tuple_spec_name_too_long(TranspilerCtx *ctx, const char *surface)
{
    transpiler_set_backend_error_with_hints(
        ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "generated tuple specialization name is too long while lowering %s",
        surface != NULL ? surface : "tuple specialization");
}

static char *
transpiler_tuple_element_trim_copy(const char *begin, size_t len)
{
    const char *end;

    if (begin == NULL)
        return NULL;
    end = begin + len;
    while (begin < end && (*begin == ' ' || *begin == '\t'))
        begin++;
    while (end > begin && (*(end - 1) == ' ' || *(end - 1) == '\t'))
        end--;
    if (end == begin)
        return NULL;
    return pergyra_strndup(begin, (size_t)(end - begin));
}

static char *
transpiler_tuple_type_name_element_copy(const char *type_name,
                                        size_t wanted_index)
{
    const char *start;
    size_t index = 0;
    int angle_depth = 0;
    int paren_depth = 0;

    if (type_name == NULL || type_name[0] != '(')
        return NULL;
    start = type_name + 1;
    while (*start == ' ' || *start == '\t')
        start++;
    for (const char *p = start; *p != '\0'; p++) {
        char c = *p;

        if (c == '<') {
            angle_depth++;
        } else if (c == '>' && angle_depth > 0) {
            angle_depth--;
        } else if (c == '(') {
            paren_depth++;
        } else if (c == ')' && angle_depth == 0) {
            if (paren_depth == 0) {
                return index == wanted_index
                    ? transpiler_tuple_element_trim_copy(start,
                        (size_t)(p - start))
                    : NULL;
            }
            paren_depth--;
        } else if (c == ',' && angle_depth == 0 && paren_depth == 0) {
            if (index == wanted_index) {
                return transpiler_tuple_element_trim_copy(start,
                    (size_t)(p - start));
            }
            index++;
            start = p + 1;
            while (*start == ' ' || *start == '\t')
                start++;
        }
    }
    return NULL;
}

static void
ensure_tuple_specialization_from_element_names_to(TranspilerCtx *ctx,
                                                  CodeBuf *dst,
                                                  const char *const *elements,
                                                  size_t n)
{
    char suffix[256];
    char elem_names[512];

    if (ctx == NULL || dst == NULL || elements == NULL || n == 0)
        return;
    suffix[0] = '\0';
    elem_names[0] = '\0';
    for (size_t i = 0; i < n; i++) {
        char sane[96];
        const char *elem = elements[i];
        if (elem == NULL || elem[0] == '\0') {
            transpiler_tuple_spec_name_too_long(ctx, "tuple");
            return;
        }
        sanitize_c_suffix(elem, sane, sizeof(sane));
        if (i > 0
            && (!transpiler_tuple_spec_append(suffix, sizeof(suffix), "_")
                || !transpiler_tuple_spec_append(elem_names,
                    sizeof(elem_names), " "))) {
            transpiler_tuple_spec_name_too_long(ctx, elem);
            return;
        }
        if (!transpiler_tuple_spec_append(suffix, sizeof(suffix), sane)
            || !transpiler_tuple_spec_append(elem_names, sizeof(elem_names),
                elem)) {
            transpiler_tuple_spec_name_too_long(ctx, elem);
            return;
        }
    }

    for (int i = 0; i < ctx->tuple_spec_count; i++) {
        if (strcmp(ctx->tuple_specs_suffix[i], suffix) == 0)
            return;
    }
    if (ctx->tuple_spec_count >= 32) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "too many tuple specializations in one translation unit; limit is 32 while lowering (%s)",
            elem_names);
        return;
    }
    if (!transpiler_tuple_spec_copy(
            ctx->tuple_specs_suffix[ctx->tuple_spec_count],
            sizeof(ctx->tuple_specs_suffix[0]), suffix)
        || !transpiler_tuple_spec_copy(
            ctx->tuple_specs_elements[ctx->tuple_spec_count],
            sizeof(ctx->tuple_specs_elements[0]), elem_names)) {
        transpiler_tuple_spec_name_too_long(ctx, elem_names);
        return;
    }
    ctx->tuple_specs_arity[ctx->tuple_spec_count] = (int)n;
    ctx->tuple_spec_count++;

    codebuf_write(dst, "\n/* PGY_TUPLE_%s */\n", suffix);
    codebuf_write(dst, "typedef struct {\n");
    for (size_t i = 0; i < n; i++) {
        const char *elem = elements[i];
        char ctype_buf[128];
        const char *ctype = ctype_buf;
        if (!transpiler_copy_c_type_or_user_type_name(elem, ctype_buf,
                sizeof(ctype_buf))) {
            transpiler_tuple_spec_name_too_long(ctx, elem);
            return;
        }
        codebuf_write(dst, "    %s f%zu;\n",
            ctype != NULL ? ctype : elem, i);
    }
    codebuf_write(dst, "} PgyTuple_%s_t;\n", suffix);
}

void
ensure_tuple_specialization_from_ast_to(TranspilerCtx *ctx, CodeBuf *dst,
                                        ASTNode *tuple_type)
{
    const char *elements[64];
    char *owned_elements[64];
    size_t n;

    if (ctx == NULL || dst == NULL || tuple_type == NULL)
        return;
    n = ast_type_tuple_element_count(tuple_type);
    if (n == 0)
        return;
    if (n > 64) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "tuple arity %zu exceeds the C backend specialization limit",
            n);
        return;
    }
    for (size_t i = 0; i < n; i++) {
        owned_elements[i] = render_type_name_in_ctx(
            ctx, ast_type_tuple_element(tuple_type, i));
        elements[i] = owned_elements[i];
        if (owned_elements[i] == NULL) {
            for (size_t j = 0; j < i; j++)
                free(owned_elements[j]);
            return;
        }
    }
    ensure_tuple_specialization_from_element_names_to(ctx, dst, elements, n);
    for (size_t i = 0; i < n; i++)
        free(owned_elements[i]);
}

void
ensure_tuple_specialization_from_type_name_to(TranspilerCtx *ctx,
                                              CodeBuf *dst,
                                              const char *type_name)
{
    const char *elements[64];
    char *owned_elements[64];
    size_t n = 0;

    if (ctx == NULL || dst == NULL || type_name == NULL
        || type_name[0] != '(')
        return;
    for (;; n++) {
        if (n >= 64) {
            transpiler_set_backend_error_with_hints(
                ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "tuple arity exceeds the C backend specialization limit while lowering %s",
                type_name);
            goto cleanup;
        }
        owned_elements[n] = transpiler_tuple_type_name_element_copy(
            type_name, n);
        if (owned_elements[n] == NULL)
            break;
        elements[n] = owned_elements[n];
        ensure_type_specializations_from_type_name_to(
            ctx, dst, owned_elements[n]);
        if (ctx->backend_error != NULL) {
            n++;
            goto cleanup;
        }
    }
    ensure_tuple_specialization_from_element_names_to(ctx, dst, elements, n);

cleanup:
    for (size_t i = 0; i < n; i++)
        free(owned_elements[i]);
}
