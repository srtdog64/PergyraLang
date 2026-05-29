/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend channel expression type queries.
 */

#include "transpiler_channel_type_query.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_render.h"

static bool
transpiler_channel_resolve_inner_type(TranspilerCtx *ctx,
                                      const char *type_name,
                                      char *inner_buf,
                                      size_t inner_buf_size,
                                      const char **inner_out,
                                      bool *is_channel_out,
                                      bool *unknown_payload_out)
{
    const char *resolved_type = type_name;
    char resolved_buf[128];

    if (is_channel_out != NULL)
        *is_channel_out = false;
    if (unknown_payload_out != NULL)
        *unknown_payload_out = false;

    if (resolved_type != NULL
        && !transpiler_type_name_is_channel(resolved_type)) {
        ASTNode *alias_decl = transpiler_find_type_alias_decl(ctx,
            resolved_type);
        if (alias_decl != NULL
            && ast_type_alias_target_type(alias_decl) != NULL) {
            ASTNode *target = resolve_type_alias_target(ctx,
                ast_type_alias_target_type(alias_decl));
            char *rendered = render_type_name_in_ctx(ctx, target);
            if (rendered != NULL) {
                bool copied = pergyra_str_copy(resolved_buf,
                    sizeof(resolved_buf), rendered);
                free(rendered);
                if (!copied) {
                    transpiler_set_backend_error_with_hints(ctx,
                        PGY_CODE_C_TYPE_UNSUPPORTED,
                        PGY_CAUSE_C_TYPE_UNSUPPORTED,
                        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                        "C backend: resolved Channel type is too long");
                    return false;
                }
                resolved_type = resolved_buf;
            }
        }
    }

    if (resolved_type != NULL
        && transpiler_type_name_is_channel(resolved_type)) {
        if (is_channel_out != NULL)
            *is_channel_out = true;
        if (!slot_inner_type_name_copy(resolved_type, inner_buf,
                inner_buf_size)
            || inner_buf[0] == '\0') {
            return false;
        }
        if (strcmp(inner_buf, "Unknown") == 0) {
            if (unknown_payload_out != NULL)
                *unknown_payload_out = true;
            return false;
        }
        if (inner_out != NULL)
            *inner_out = inner_buf;
        return true;
    }
    return false;
}

bool
channel_inner_type_name_copy(TranspilerCtx *ctx, ASTNode *expr,
                             char *out, size_t out_size)
{
    const char *type_name;

    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';

    type_name = transpiler_expr_infer_type_name(ctx, expr);
    if (transpiler_channel_resolve_inner_type(ctx, type_name, out, out_size,
            NULL, NULL, NULL)) {
        return true;
    }
    return pergyra_str_copy(out, out_size, "Unknown");
}

bool
transpiler_channel_expr_is_c_lvalue(ASTNode *channel)
{
    return channel != NULL && channel->type == AST_IDENTIFIER;
}

void
transpiler_set_channel_lvalue_error(TranspilerCtx *ctx,
                                    const char *operation)
{
    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_SEND,
        "C backend: %s requires a named Channel<T> binding because the runtime call takes the channel address",
        operation != NULL ? operation : "channel operation");
}

const char *
transpiler_require_channel_inner_type(TranspilerCtx *ctx, ASTNode *expr,
                                      const char *operation,
                                      char *inner_buf,
                                      size_t inner_buf_size)
{
    const char *inner = NULL;
    const char *type_name = transpiler_expr_infer_type_name(ctx, expr);
    bool is_channel = false;
    bool unknown_payload = false;

    if (transpiler_channel_resolve_inner_type(ctx, type_name, inner_buf,
            inner_buf_size, &inner, &is_channel, &unknown_payload)) {
        return inner;
    }

    if (is_channel && unknown_payload) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C backend: %s requires concrete Channel<T> payload metadata",
            operation != NULL ? operation : "Channel operation");
        return NULL;
    }

    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "C backend: %s requires concrete Channel<T> metadata",
        operation != NULL ? operation : "Channel operation");
    return NULL;
}
