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

const char *
transpiler_contextual_option_inner_type_name(TranspilerCtx *ctx)
{
    const char *option_type = transpiler_contextual_option_type_name(ctx);
    return option_type != NULL ? slot_inner_type_name(option_type) : NULL;
}

char *
transpiler_emit_none_with_context(TranspilerCtx *ctx, ASTNode *site)
{
    const char *inner = transpiler_contextual_option_inner_type_name(ctx);
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
