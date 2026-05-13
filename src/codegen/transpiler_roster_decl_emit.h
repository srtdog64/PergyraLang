#ifndef PGY_TRANSPILER_ROSTER_DECL_EMIT_H
#define PGY_TRANSPILER_ROSTER_DECL_EMIT_H

void
emit_roster_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = ast_roster_name(node);
    ASTNode *inventory_decl = transpiler_find_decl_in_inventory_local(
        ctx, AST_ROSTER_DECL, name);

    if (inventory_decl != NULL)
        node = inventory_decl;

    codebuf_write(ctx->out, "\n/* Roster: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    for (size_t i = 0; i < ast_roster_party_count(node); i++) {
        ASTNode *slot = ast_roster_party(node, i);
        codebuf_write(ctx->out, "    %s %s;\n",
            ast_roster_slot_party_type(slot),
            ast_roster_slot_name(slot));
    }

    for (size_t i = 0; i < ast_roster_shared_count(node); i++) {
        ASTNode *shared = ast_roster_shared(node, i);
        const char *field_type = NULL;
        char surface_desc[256];
        if (!transpiler_domain_nominal_surface_desc(surface_desc,
                sizeof(surface_desc), "roster shared field", name,
                ast_party_shared_name(shared),
                NULL)) {
            transpiler_domain_nominal_surface_desc_too_long(
                ctx, "roster shared field");
            return;
        }
        field_type = transpiler_require_ast_c_type(
            ctx,
            ast_party_shared_type(shared),
            surface_desc);
        if (field_type == NULL)
            return;
        codebuf_write(ctx->out, "    %s %s;\n",
            field_type, ast_party_shared_name(shared));
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    TranspilerHostedMethodView method_view =
        transpiler_hosted_method_view_from_decl(ctx, name, node);
    if (transpiler_hosted_method_view_missing_mir_metadata(&method_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing method declaration metadata for roster '%s'",
            name != NULL ? name : "(anonymous-roster)");
        return;
    }

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

    emit_hosted_methods_from_mir_or_error_local(name, "(anonymous-roster)",
        "roster", &method_view, ctx);
}

#endif /* PGY_TRANSPILER_ROSTER_DECL_EMIT_H */
