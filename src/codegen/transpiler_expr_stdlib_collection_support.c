/*
 * Copyright (c) 2026 Pergyra Language Project
 * Shared support for C backend collection stdlib lowering.
 */

#include "transpiler_expr_stdlib_collection_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_enum.h"
#include "transpiler_format.h"
#include "transpiler_nominal.h"
#include "transpiler_projection.h"
#include "transpiler_symbols.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_render.h"

static bool
transpiler_collection_copy_spec_name(char *dst, size_t dst_size,
                                     const char *value)
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
transpiler_collection_copy_type_name(char *out, size_t out_size,
                                     const char *type_name)
{
    size_t len;

    if (out == NULL || out_size == 0 || type_name == NULL)
        return false;

    len = strlen(type_name);
    if (len >= out_size)
        return false;

    memcpy(out, type_name, len + 1);
    return true;
}

bool
transpiler_require_hashmap_type(TranspilerCtx *ctx, const char *map_type,
                                const char *operation,
                                char *key_buf, size_t key_buf_size,
                                char *value_buf, size_t value_buf_size,
                                const char **key_out,
                                const char **value_out)
{
    const char *resolved_type = map_type;
    char resolved_buf[128];

    if (resolved_type != NULL && strncmp(resolved_type, "HashMap<", 8) != 0) {
        ASTNode *alias_decl = transpiler_find_type_alias_decl(ctx, resolved_type);
        if (alias_decl != NULL && ast_type_alias_target_type(alias_decl) != NULL) {
            ASTNode *target = resolve_type_alias_target(
                ctx, ast_type_alias_target_type(alias_decl));
            char *rendered = render_type_name(target);
            if (rendered != NULL) {
                bool copied = transpiler_collection_copy_type_name(
                    resolved_buf, sizeof(resolved_buf), rendered);
                free(rendered);
                if (!copied) {
                    transpiler_set_backend_error_with_hints(ctx,
                        PGY_CODE_C_TYPE_UNSUPPORTED,
                        PGY_CAUSE_C_TYPE_UNSUPPORTED,
                        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                        "C backend: %s resolved HashMap type is too long",
                        operation != NULL ? operation : "HashMap operation");
                    return false;
                }
                resolved_type = resolved_buf;
            }
        }
    }

    if (resolved_type != NULL && strncmp(resolved_type, "HashMap<", 8) == 0) {
        copy_constructed_arg_name_at(resolved_type, 0, key_buf, key_buf_size);
        copy_constructed_arg_name_at(resolved_type, 1, value_buf, value_buf_size);
        if (key_buf[0] != '\0' && value_buf[0] != '\0') {
            if (key_out != NULL)
                *key_out = key_buf;
            if (value_out != NULL)
                *value_out = value_buf;
            return true;
        }
    }

    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "C backend: %s requires concrete HashMap<K, V> metadata",
        operation != NULL ? operation : "HashMap operation");
    return false;
}

bool
transpiler_require_unary_collection_type(TranspilerCtx *ctx,
                                         const char *type_name,
                                         const char *family,
                                         const char *operation,
                                         char *inner_buf,
                                         size_t inner_buf_size,
                                         const char **inner_out)
{
    const char *resolved_type = type_name;
    char resolved_buf[128];
    size_t family_len = family != NULL ? strlen(family) : 0;

    if (resolved_type != NULL
        && !(family_len > 0
             && strncmp(resolved_type, family, family_len) == 0
             && resolved_type[family_len] == '<')) {
        ASTNode *alias_decl = transpiler_find_type_alias_decl(ctx, resolved_type);
        if (alias_decl != NULL && ast_type_alias_target_type(alias_decl) != NULL) {
            ASTNode *target = resolve_type_alias_target(
                ctx, ast_type_alias_target_type(alias_decl));
            char *rendered = render_type_name(target);
            if (rendered != NULL) {
                bool copied = transpiler_collection_copy_type_name(
                    resolved_buf, sizeof(resolved_buf), rendered);
                free(rendered);
                if (!copied) {
                    transpiler_set_backend_error_with_hints(ctx,
                        PGY_CODE_C_TYPE_UNSUPPORTED,
                        PGY_CAUSE_C_TYPE_UNSUPPORTED,
                        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                        "C backend: %s resolved %s type is too long",
                        operation != NULL ? operation : "collection operation",
                        family != NULL ? family : "collection");
                    return false;
                }
                resolved_type = resolved_buf;
            }
        }
    }

    if (resolved_type != NULL
        && family_len > 0
        && strncmp(resolved_type, family, family_len) == 0
        && resolved_type[family_len] == '<') {
        if (slot_inner_type_name_copy(resolved_type, inner_buf,
                inner_buf_size)
            && inner_buf[0] != '\0') {
            if (inner_out != NULL)
                *inner_out = inner_buf;
            return true;
        }
    }

    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "C backend: %s requires concrete %s<T> metadata",
        operation != NULL ? operation : "collection operation",
        family != NULL ? family : "collection");
    return false;
}

static void
transpiler_collection_ensure_specialization_to(TranspilerCtx *ctx, CodeBuf *dst,
                                               const char *kind,
                                               const char *inner_type)
{
    char suffix[128];
    char ctype_buf[128];

    if (ctx == NULL || dst == NULL || kind == NULL || inner_type == NULL)
        return;

    if ((strcmp(kind, "List") == 0 || strcmp(kind, "Queue") == 0)
        && (strcmp(inner_type, "Int") == 0
            || strcmp(inner_type, "String") == 0)) {
        return;
    }
    if ((strcmp(kind, "Map") == 0 || strcmp(kind, "Set") == 0)
        && (strcmp(inner_type, "Int") == 0
            || strcmp(inner_type, "String") == 0)) {
        return;
    }

    sanitize_c_suffix(inner_type, suffix, sizeof(suffix));
    for (int i = 0; i < ctx->collection_spec_count; i++) {
        if (strcmp(ctx->collection_specs[i].kind, kind) == 0
            && strcmp(ctx->collection_specs[i].suffix, suffix) == 0) {
            return;
        }
    }

    if (ctx->collection_spec_count >= MAX_COLLECTION_SPECIALIZATIONS)
        return;
    if (!pergyra_type_to_c_copy(inner_type, ctype_buf, sizeof(ctype_buf)))
        return;
    if (!transpiler_collection_copy_spec_name(
            ctx->collection_specs[ctx->collection_spec_count].kind,
            sizeof(ctx->collection_specs[ctx->collection_spec_count].kind),
            kind)
        || !transpiler_collection_copy_spec_name(
            ctx->collection_specs[ctx->collection_spec_count].suffix,
            sizeof(ctx->collection_specs[ctx->collection_spec_count].suffix),
            suffix)) {
        return;
    }
    ctx->collection_spec_count++;

    if (strcmp(kind, "List") == 0) {
        codebuf_write(dst,
            "\n/* PGY_COLLECTION_LIST_%s */\n"
            "PGY_LIST_DEFINE(%s, %s)\n",
            suffix, suffix, ctype_buf);
    } else if (strcmp(kind, "Queue") == 0) {
        codebuf_write(dst,
            "\n/* PGY_COLLECTION_QUEUE_%s */\n"
            "PGY_QUEUE_DEFINE(%s, %s)\n",
            suffix, suffix, ctype_buf);
    } else if (strcmp(kind, "Map") == 0) {
        codebuf_write(dst,
            "\n/* PGY_COLLECTION_MAP_%s */\n"
            "PGY_HASHMAP_DEFINE(%s, %s)\n",
            suffix, suffix, ctype_buf);
    } else if (strcmp(kind, "Set") == 0) {
        codebuf_write(dst,
            "\n/* PGY_COLLECTION_SET_%s */\n"
            "PGY_SET_DEFINE(%s, %s)\n",
            suffix, suffix, ctype_buf);
    }
}

void
transpiler_collection_ensure_specialization(TranspilerCtx *ctx,
                                            const char *kind,
                                            const char *inner_type)
{
    transpiler_collection_ensure_specialization_to(ctx,
        ctx != NULL ? ctx->decls : NULL, kind, inner_type);
}
