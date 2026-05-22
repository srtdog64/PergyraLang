/*
 * Copyright (c) 2026 Pergyra Language Project
 * Generic function specialization emission for the C backend.
 */

#include "transpiler_generic_specialization_emit.h"

#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"

#include "transpiler_context.h"
#include "transpiler_func_forward_helpers.h"
#include "transpiler_generic_binding_query.h"
#include "transpiler_mangled_name.h"

static bool
transpiler_generic_specialization_copy_name(char *out, size_t out_size,
                                            const char *name)
{
    size_t len;

    if (out == NULL || out_size == 0 || name == NULL)
        return false;
    len = strlen(name);
    if (len >= out_size)
        return false;
    memcpy(out, name, len + 1);
    return true;
}

static void
transpiler_generic_specialization_name_too_long(TranspilerCtx *ctx,
                                                const char *decl_name)
{
    transpiler_set_backend_error_with_hints(
        ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "C backend: generic function specialization name is too long for '%s'",
        decl_name != NULL ? decl_name : "(anonymous)");
}

const char *
ensure_generic_specialization(TranspilerCtx *ctx, ASTNode *decl, ASTNode *call)
{
    GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
    size_t binding_count = 0;
    const char *decl_name;
    CodeBuf *name_buf;
    GenericSpecializationEntry *entry;
    int saved_binding_count;

    if (ctx == NULL || decl == NULL || decl->type != AST_FUNC_DECL)
        return NULL;

    decl_name = ast_declaration_name(decl);
    if (decl_name == NULL)
        return NULL;
    if (!transpiler_infer_generic_call_bindings(ctx, decl, call, bindings,
            &binding_count))
        return NULL;

    name_buf = codebuf_create();
    if (name_buf == NULL)
        return NULL;

    codebuf_write(name_buf, "%s", decl_name);
    for (size_t i = 0; i < binding_count; i++) {
        codebuf_write(name_buf, "_");
        append_mangled_type_name(name_buf, bindings[i].concrete_type);
    }

    for (int i = 0; i < ctx->generic_specialization_count; i++) {
        entry = &ctx->generic_specializations[i];
        if (entry->func_decl == decl
            && strcmp(entry->specialized_name, name_buf->data) == 0) {
            const char *result = entry->specialized_name;
            codebuf_destroy(name_buf);
            return result;
        }
    }

    if (ctx->generic_specialization_count >= MAX_GENERIC_SPECIALIZATIONS) {
        codebuf_destroy(name_buf);
        return NULL;
    }

    entry = &ctx->generic_specializations[ctx->generic_specialization_count++];
    memset(entry, 0, sizeof(*entry));
    entry->func_decl = decl;
    if (!transpiler_generic_specialization_copy_name(
            entry->specialized_name, sizeof(entry->specialized_name),
            name_buf->data)) {
        ctx->generic_specialization_count--;
        transpiler_generic_specialization_name_too_long(ctx, decl_name);
        codebuf_destroy(name_buf);
        return NULL;
    }
    entry->emitting = true;
    codebuf_destroy(name_buf);

    saved_binding_count = ctx->generic_binding_count;
    for (size_t i = 0; i < binding_count
         && ctx->generic_binding_count < MAX_GENERIC_BINDINGS; i++) {
        ctx->generic_bindings[ctx->generic_binding_count++] = bindings[i];
    }

    emit_func_forward_decl_named(decl, entry->specialized_name, ctx->decls, ctx);
    emit_func_decl_named(decl, entry->specialized_name, ctx->helpers, ctx);

    ctx->generic_binding_count = saved_binding_count;
    entry->emitting = false;
    return entry->specialized_name;
}
