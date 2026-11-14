/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * C backend type/name mangling helpers.
 */

#include "transpiler_mangled_name.h"

#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"

void
append_mangled_type_name(CodeBuf *buf, const char *type_name)
{
    bool wrote = false;

    if (buf == NULL || type_name == NULL) {
        if (buf != NULL)
            codebuf_write(buf, "Type");
        return;
    }

    for (const unsigned char *p = (const unsigned char *)type_name; *p != '\0'; p++) {
        if ((*p >= 'a' && *p <= 'z')
            || (*p >= 'A' && *p <= 'Z')
            || (*p >= '0' && *p <= '9')) {
            codebuf_write(buf, "%c", *p);
            wrote = true;
        } else if (wrote && buf->len > 0 && buf->data[buf->len - 1] != '_') {
            codebuf_write(buf, "_");
        }
    }

    if (!wrote)
        codebuf_write(buf, "Type");
}

const char *
transpiler_c_user_callable_symbol(TranspilerCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL || name[0] == '\0')
        return NULL;
    return transpiler_scratch_fmt(ctx, "pgy_u_%s", name);
}

const char *
transpiler_c_user_callable_reference_symbol(TranspilerCtx *ctx,
                                            const char *name)
{
    ASTNode *decl;

    if (ctx == NULL || name == NULL)
        return NULL;
    decl = transpiler_find_named_decl_local(ctx, AST_FUNC_DECL, name);
    if (decl != NULL && !transpiler_decl_is_extern_function(ctx, decl))
        return transpiler_c_user_callable_symbol(ctx, name);
    if (transpiler_find_named_decl_local(ctx, AST_INTENT_DECL, name) != NULL)
        return transpiler_c_user_callable_symbol(ctx, name);
    return NULL;
}
