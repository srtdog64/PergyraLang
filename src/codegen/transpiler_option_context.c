/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend Option<T> contextual emission helpers.
 */

#include "transpiler.h"
#include "transpiler_context.h"
#include "transpiler_format.h"

#include "../common/string_compat.h"
#include "../semantic/diag_codes.h"

#include <string.h>

const char *
transpiler_contextual_option_type_name(TranspilerCtx *ctx)
{
    if (ctx == NULL)
        return NULL;
    if (ctx->expected_type != NULL
        && strncmp(ctx->expected_type, "Option<", 7) == 0)
        return ctx->expected_type;
    if (ctx->current_return_type[0] != '\0'
        && strncmp(ctx->current_return_type, "Option<", 7) == 0)
        return ctx->current_return_type;
    return NULL;
}

bool
transpiler_contextual_option_inner_type_copy(TranspilerCtx *ctx,
                                             char *out,
                                             size_t out_size)
{
    const char *option_type = transpiler_contextual_option_type_name(ctx);

    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';
    if (option_type == NULL)
        return false;
    return slot_inner_type_name_copy(option_type, out, out_size);
}

char *
transpiler_emit_none_with_context(TranspilerCtx *ctx, ASTNode *site)
{
    char inner_buf[128];
    const char *inner = NULL;
    if (transpiler_contextual_option_inner_type_copy(ctx, inner_buf,
            sizeof(inner_buf))) {
        inner = inner_buf;
    }
    if (inner == NULL) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "None requires contextual Option<T> during C emission");
        (void)site;
        return pergyra_strdup("0");
    }
    return strdup_fmt("None_%s()", inner);
}
