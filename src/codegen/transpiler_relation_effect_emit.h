#ifndef PGY_TRANSPILER_RELATION_EFFECT_EMIT_H
#define PGY_TRANSPILER_RELATION_EFFECT_EMIT_H

static bool
transpiler_relation_effect_surface_desc(char *out, size_t out_size,
                                        const char *surface_kind,
                                        const char *owner_name,
                                        const char *member_name)
{
    int written;

    if (out == NULL || out_size == 0 || surface_kind == NULL)
        return false;

    written = snprintf(out, out_size, "%s '%s.%s'",
        surface_kind,
        owner_name != NULL ? owner_name : "(anonymous)",
        member_name != NULL ? member_name : "(anonymous)");

    return written >= 0 && (size_t)written < out_size;
}

static void
transpiler_relation_effect_surface_desc_too_long(TranspilerCtx *ctx,
                                                 const char *surface_kind)
{
    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "%s diagnostic surface is too long for C backend emission",
        surface_kind != NULL ? surface_kind : "relation/effect");
}

void
emit_relation_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.relation_decl.name;
    ASTNode *inventory_decl = transpiler_find_decl_in_inventory_local(
        ctx, AST_RELATION_DECL, name);

    if (inventory_decl != NULL)
        node = inventory_decl;

    codebuf_write(ctx->out, "\n/* Relation: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    for (size_t i = 0; i < node->data.relation_decl.slot_count; i++) {
        ASTNode *slot = node->data.relation_decl.slots[i];
        const char *ft = NULL;
        char surface_desc[256];
        if (!transpiler_relation_effect_surface_desc(surface_desc,
                sizeof(surface_desc), "relation slot", name,
                slot != NULL ? slot->data.domain_slot.slot_name : NULL)) {
            transpiler_relation_effect_surface_desc_too_long(
                ctx, "relation slot");
            return;
        }
        ft = transpiler_require_ast_c_type(
            ctx,
            slot != NULL ? slot->data.domain_slot.type : NULL,
            surface_desc);
        if (ft == NULL)
            return;
        codebuf_write(ctx->out, "    %s %s;\n", ft, slot->data.domain_slot.slot_name);
        if (!slot->data.domain_slot.is_subject) {
            codebuf_write(ctx->out, "    bool __projection_ready_%s;\n",
                slot->data.domain_slot.slot_name);
            codebuf_write(ctx->out, "    bool __projection_dirty_%s;\n",
                slot->data.domain_slot.slot_name);
            emit_hidden_provenance_fields(ctx, "projection",
                slot->data.domain_slot.slot_name);
        }
    }

    for (size_t i = 0; i < node->data.relation_decl.shared_count; i++) {
        ASTNode *shared = node->data.relation_decl.shared_fields[i];
        const char *ft = NULL;
        char surface_desc[256];
        if (!transpiler_relation_effect_surface_desc(surface_desc,
                sizeof(surface_desc), "relation shared field", name,
                shared != NULL ? shared->data.party_shared.name : NULL)) {
            transpiler_relation_effect_surface_desc_too_long(
                ctx, "relation shared field");
            return;
        }
        ft = transpiler_require_ast_c_type(
            ctx,
            shared != NULL ? shared->data.party_shared.type : NULL,
            surface_desc);
        if (ft == NULL)
            return;
        codebuf_write(ctx->out, "    %s %s;\n", ft, shared->data.party_shared.name);
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    codebuf_write(ctx->out, "\nstatic inline void\n%s_sync(%s *self)\n{\n",
                  name, name);
    ctx->indent++;
    emit_domain_projection_sync_loop(ctx,
        node->data.relation_decl.slots,
        node->data.relation_decl.slot_count,
        node->data.relation_decl.refreshes,
        node->data.relation_decl.refresh_count,
        "relation_projection",
        true);
    ctx->indent--;
    codebuf_write(ctx->out, "}\n");

    TranspilerHostedMethodView method_view =
        transpiler_hosted_method_view_from_decl(ctx, name, node);

    for (size_t i = 0; i < method_view.count; i++) {
        const MIRDeclMethod *method_meta =
            transpiler_hosted_method_view_metadata(&method_view, i);
        ASTNode *method =
            transpiler_hosted_method_view_source_ast(&method_view, i);
        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;
        emit_hosted_method_forward_decl_from_metadata(name, method_meta,
            method, true, ctx->out, ctx);
    }

    emit_hosted_methods_from_mir_or_error_local(name, "(anonymous-relation)",
        "relation", &method_view, ctx);
}

void
emit_effect_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.effect_decl.name;
    ASTNode *inventory_decl = transpiler_find_decl_in_inventory_local(
        ctx, AST_EFFECT_DECL, name);

    if (inventory_decl != NULL)
        node = inventory_decl;

    codebuf_write(ctx->out, "\n/* Effect: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    for (size_t i = 0; i < node->data.effect_decl.slot_count; i++) {
        ASTNode *slot = node->data.effect_decl.slots[i];
        const char *ft = NULL;
        char surface_desc[256];
        if (!transpiler_relation_effect_surface_desc(surface_desc,
                sizeof(surface_desc), "effect slot", name,
                slot != NULL ? slot->data.domain_slot.slot_name : NULL)) {
            transpiler_relation_effect_surface_desc_too_long(
                ctx, "effect slot");
            return;
        }
        ft = transpiler_require_ast_c_type(
            ctx,
            slot != NULL ? slot->data.domain_slot.type : NULL,
            surface_desc);
        if (ft == NULL)
            return;
        codebuf_write(ctx->out, "    %s %s;\n", ft, slot->data.domain_slot.slot_name);
        if (!slot->data.domain_slot.is_subject) {
            codebuf_write(ctx->out, "    bool __projection_ready_%s;\n",
                slot->data.domain_slot.slot_name);
            codebuf_write(ctx->out, "    bool __projection_dirty_%s;\n",
                slot->data.domain_slot.slot_name);
            emit_hidden_provenance_fields(ctx, "projection",
                slot->data.domain_slot.slot_name);
        }
    }

    for (size_t i = 0; i < node->data.effect_decl.shared_count; i++) {
        ASTNode *shared = node->data.effect_decl.shared_fields[i];
        const char *ft = NULL;
        char surface_desc[256];
        if (!transpiler_relation_effect_surface_desc(surface_desc,
                sizeof(surface_desc), "effect shared field", name,
                shared != NULL ? shared->data.party_shared.name : NULL)) {
            transpiler_relation_effect_surface_desc_too_long(
                ctx, "effect shared field");
            return;
        }
        ft = transpiler_require_ast_c_type(
            ctx,
            shared != NULL ? shared->data.party_shared.type : NULL,
            surface_desc);
        if (ft == NULL)
            return;
        codebuf_write(ctx->out, "    %s %s;\n", ft, shared->data.party_shared.name);
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    codebuf_write(ctx->out, "\nstatic inline void\n%s_sync(%s *self)\n{\n",
                  name, name);
    ctx->indent++;
    emit_domain_projection_sync_loop(ctx,
        node->data.effect_decl.slots,
        node->data.effect_decl.slot_count,
        node->data.effect_decl.refreshes,
        node->data.effect_decl.refresh_count,
        "effect_projection",
        true);
    ctx->indent--;
    codebuf_write(ctx->out, "}\n");

    TranspilerHostedMethodView method_view =
        transpiler_hosted_method_view_from_decl(ctx, name, node);

    for (size_t i = 0; i < method_view.count; i++) {
        const MIRDeclMethod *method_meta =
            transpiler_hosted_method_view_metadata(&method_view, i);
        ASTNode *method =
            transpiler_hosted_method_view_source_ast(&method_view, i);
        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;
        emit_hosted_method_forward_decl_from_metadata(name, method_meta,
            method, true, ctx->out, ctx);
    }

    emit_hosted_methods_from_mir_or_error_local(name, "(anonymous-effect)",
        "effect", &method_view, ctx);
}

#endif /* PGY_TRANSPILER_RELATION_EFFECT_EMIT_H */
