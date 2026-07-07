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
#include "transpiler_inventory_view.h"
#include "transpiler_nominal.h"
#include "transpiler_projection.h"
#include "transpiler_symbols.h"
#include "codegen_type_mapping.h"
#include "transpiler_specialization_registry.h"
#include "transpiler_type_render.h"
#include "transpiler_type_require.h"

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

    if (resolved_type != NULL
        && !transpiler_type_name_is_hashmap(resolved_type)) {
        const char *target_type_name =
            transpiler_type_alias_target_type_name_from_headers(
                ctx, resolved_type);
        if (target_type_name != NULL) {
            bool copied = transpiler_collection_copy_type_name(
                resolved_buf, sizeof(resolved_buf), target_type_name);
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

    if (transpiler_type_name_is_hashmap(resolved_type)) {
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
        const char *target_type_name =
            transpiler_type_alias_target_type_name_from_headers(
                ctx, resolved_type);
        if (target_type_name != NULL) {
            bool copied = transpiler_collection_copy_type_name(
                resolved_buf, sizeof(resolved_buf), target_type_name);
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

bool
transpiler_expr_is_c_addressable_storage(ASTNode *expr)
{
    if (expr == NULL)
        return false;
    if (expr->type == AST_IDENTIFIER)
        return true;
    if (expr->type == AST_MEMBER_ACCESS)
        return transpiler_expr_is_c_addressable_storage(
            ast_member_object(expr));
    return false;
}

bool
transpiler_require_c_addressable_storage(TranspilerCtx *ctx,
                                         ASTNode *expr,
                                         const char *operation,
                                         const char *storage_kind)
{
    if (transpiler_expr_is_c_addressable_storage(expr))
        return true;

    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_SEND,
        "C backend: %s requires addressable %s storage because the runtime call takes its address",
        operation != NULL ? operation : "collection operation",
        storage_kind != NULL ? storage_kind : "collection");
    return false;
}

void
transpiler_collection_ensure_specialization(TranspilerCtx *ctx,
                                            const char *kind,
                                            const char *inner_type)
{
    ensure_collection_specialization(ctx, kind, inner_type);
}
