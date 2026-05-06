#ifndef PGY_SRC_CODEGEN_TRANSPILER_ZONE_STRUCT_EMIT_H
#define PGY_SRC_CODEGEN_TRANSPILER_ZONE_STRUCT_EMIT_H

static bool
transpiler_emit_zone_struct_decl(TranspilerCtx *ctx, ASTNode *node, const char *name)
{
    codebuf_write(ctx->out, "\n/* Zone: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    for (size_t i = 0; i < node->data.zone_decl.slot_count; i++) {
        ASTNode *slot = node->data.zone_decl.slots[i];
        const char *ft = NULL;
        char surface_desc[256];
        snprintf(surface_desc, sizeof(surface_desc),
            "zone slot '%s.%s'",
            name != NULL ? name : "(anonymous)",
            slot != NULL && slot->data.domain_slot.slot_name != NULL
                ? slot->data.domain_slot.slot_name
                : "(anonymous)");
        ft = transpiler_require_ast_c_type(
            ctx,
            slot != NULL ? slot->data.domain_slot.type : NULL,
            surface_desc);
        if (ft == NULL)
            return false;
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

    for (size_t i = 0; i < node->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = node->data.zone_decl.layer_slots[i];
        if (slot->data.zone_layer_slot.is_pool) {
            int cap = slot->data.zone_layer_slot.pool_capacity;
            if (cap <= 0)
                cap = 1;
            codebuf_write(ctx->out,
                "    struct { %s items[%d]; bool active[%d]; uint8_t count; uint8_t cap; } %s;\n",
                slot->data.zone_layer_slot.layer_type,
                cap,
                cap,
                slot->data.zone_layer_slot.slot_name);
        } else {
            codebuf_write(ctx->out, "    %s %s;\n",
                slot->data.zone_layer_slot.layer_type,
                slot->data.zone_layer_slot.slot_name);
        }
        codebuf_write(ctx->out, "    bool __layer_active_%s;\n",
            slot->data.zone_layer_slot.slot_name);
        emit_hidden_provenance_fields(ctx, "layer",
            slot->data.zone_layer_slot.slot_name);
    }

    for (size_t i = 0; i < node->data.zone_decl.shared_count; i++) {
        ASTNode *shared = node->data.zone_decl.shared_fields[i];
        const char *ft = NULL;
        char surface_desc[256];
        snprintf(surface_desc, sizeof(surface_desc),
            "zone shared field '%s.%s'",
            name != NULL ? name : "(anonymous)",
            shared != NULL && shared->data.party_shared.name != NULL
                ? shared->data.party_shared.name
                : "(anonymous)");
        ft = transpiler_require_ast_c_type(
            ctx,
            shared != NULL ? shared->data.party_shared.type : NULL,
            surface_desc);
        if (ft == NULL)
            return false;
        codebuf_write(ctx->out, "    %s %s;\n", ft, shared->data.party_shared.name);
    }

    for (size_t i = 0; i < node->data.zone_decl.state_count; i++) {
        ASTNode *state = node->data.zone_decl.states[i];
        codebuf_write(ctx->out, "    bool __state_%s;\n",
            state->data.zone_state.state_name);
        emit_hidden_provenance_fields(ctx, "state",
            state->data.zone_state.state_name);
    }

    codebuf_write(ctx->out, "    PGY_ZONE_LOCK_FIELD\n");
    codebuf_write(ctx->out, "    PGY_ZONE_GENERATION_FIELD\n");
    codebuf_write(ctx->out, "} %s;\n", name);
    return true;
}

static void
transpiler_emit_zone_layer_accessors(TranspilerCtx *ctx, ASTNode *node, const char *name)
{
    for (size_t i = 0; i < node->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = node->data.zone_decl.layer_slots[i];
        const char *slot_name = slot->data.zone_layer_slot.slot_name;

        if (slot_name == NULL)
            continue;

        codebuf_write(ctx->out,
            "\nstatic inline bool\n%s_has_layer_%s(%s *self, uint32_t expected_gen)\n{\n",
            name, slot_name, name);
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "bool result;\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "PGY_ZONE_RDLOCK(self);\n");
        write_indent(ctx);
        codebuf_write(ctx->out,
            "PGY_ZONE_GENERATION_WARN_IF_STALE(self, expected_gen, \"%s.%s\");\n",
            name, slot_name);
        write_indent(ctx);
        codebuf_write(ctx->out, "result = self->__layer_active_%s;\n", slot_name);
        write_indent(ctx);
        codebuf_write(ctx->out, "PGY_ZONE_UNLOCK(self);\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "return result;\n");
        ctx->indent--;
        codebuf_write(ctx->out, "}\n");
    }
}
#endif /* PGY_SRC_CODEGEN_TRANSPILER_ZONE_STRUCT_EMIT_H */
