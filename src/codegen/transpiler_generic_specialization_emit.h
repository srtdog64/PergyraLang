#ifndef PGY_SRC_CODEGEN_TRANSPILER_GENERIC_SPECIALIZATION_EMIT_H
#define PGY_SRC_CODEGEN_TRANSPILER_GENERIC_SPECIALIZATION_EMIT_H

#include "../common/string_compat.h"

static const char *
ensure_generic_specialization(TranspilerCtx *ctx, ASTNode *decl, ASTNode *call)
{
    GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
    size_t binding_count = 0;

    const char *decl_name = ast_declaration_name(decl);
    if (ctx == NULL || decl == NULL || decl->type != AST_FUNC_DECL
        || decl_name == NULL) {
        return NULL;
    }
    if (!infer_generic_call_bindings(ctx, decl, call, bindings, &binding_count))
        return NULL;

    CodeBuf *name_buf = codebuf_create();
    if (name_buf == NULL)
        return NULL;

    codebuf_write(name_buf, "%s", decl_name);
    for (size_t i = 0; i < binding_count; i++) {
        codebuf_write(name_buf, "_");
        append_mangled_type_name(name_buf, bindings[i].concrete_type);
    }

    for (int i = 0; i < ctx->generic_specialization_count; i++) {
        GenericSpecializationEntry *entry = &ctx->generic_specializations[i];
        if (entry->func_decl == decl
            && strcmp(entry->specialized_name, name_buf->data) == 0) {
            codebuf_destroy(name_buf);
            return entry->specialized_name;
        }
    }

    if (ctx->generic_specialization_count >= MAX_GENERIC_SPECIALIZATIONS) {
        codebuf_destroy(name_buf);
        return NULL;
    }

    GenericSpecializationEntry *entry =
        &ctx->generic_specializations[ctx->generic_specialization_count++];
    memset(entry, 0, sizeof(*entry));
    entry->func_decl = decl;
    pergyra_str_copy(entry->specialized_name,
        sizeof(entry->specialized_name), name_buf->data);
    entry->emitting = true;
    codebuf_destroy(name_buf);

    int saved_binding_count = ctx->generic_binding_count;
    for (size_t i = 0; i < binding_count && ctx->generic_binding_count < MAX_GENERIC_BINDINGS; i++) {
        ctx->generic_bindings[ctx->generic_binding_count++] = bindings[i];
    }

    emit_func_forward_decl_named(decl, entry->specialized_name, ctx->decls, ctx);
    emit_func_decl_named(decl, entry->specialized_name, ctx->helpers, ctx);

    ctx->generic_binding_count = saved_binding_count;
    entry->emitting = false;
    return entry->specialized_name;
}
#endif /* PGY_SRC_CODEGEN_TRANSPILER_GENERIC_SPECIALIZATION_EMIT_H */
