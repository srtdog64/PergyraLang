static SlotVarEntry *
transpiler_find_slot_var_local(TranspilerCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;

    for (int i = ctx->slot_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->slot_vars[i].name, name) == 0)
            return &ctx->slot_vars[i];
    }

    return NULL;
}

static const char *
transpiler_block_slot_address_local(const SlotVarEntry *slot,
                                    char *buf,
                                    size_t buf_size)
{
    if (slot == NULL)
        return "NULL";
    if (slot->is_indirect)
        return slot->name;
    if (buf != NULL && buf_size > 0) {
        snprintf(buf, buf_size, "&%s", slot->name);
        return buf;
    }
    return slot->name;
}

static const char *
transpiler_block_slot_token_address_local(const SlotVarEntry *slot,
                                          char *buf,
                                          size_t buf_size)
{
    const char *token_name;
    if (slot == NULL)
        return "NULL";
    token_name = slot->token_name[0] != '\0' ? slot->token_name : slot->name;
    if (buf != NULL && buf_size > 0) {
        snprintf(buf, buf_size, "&%s", token_name);
        return buf;
    }
    return token_name;
}

static bool
emit_pin_block_enter_local(ASTNode *node, TranspilerCtx *ctx)
{
    SlotVarEntry *slot;
    int pin_id;
    const char *mode;
    char slot_addr[96];
    char token_addr[96];

    if (node == NULL || ctx == NULL || node->type != AST_BLOCK
        || !node->data.block.is_pin_block
        || node->data.block.pin_source_name == NULL)
        return false;

    slot = transpiler_find_slot_var_local(ctx, node->data.block.pin_source_name);
    if (slot == NULL || slot->inner_type[0] == '\0')
        return false;

    pin_id = ctx->tmp_counter++;
    mode = node->data.block.pin_view_is_write ? "write" : "read";

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;
    write_indent(ctx);
    if (slot->is_secure) {
        codebuf_write(ctx->out,
            "PgyPinnedSecureSlotView_%s __pgy_pin_%d "
            "__attribute__((cleanup(pgy_secure_unpin_cleanup_%s))) = "
            "pgy_secure_pin_%s_%s(%s, %s);\n",
            slot->inner_type, pin_id, slot->inner_type,
            mode, slot->inner_type,
            transpiler_block_slot_address_local(slot, slot_addr, sizeof(slot_addr)),
            transpiler_block_slot_token_address_local(slot, token_addr, sizeof(token_addr)));
    } else {
        codebuf_write(ctx->out,
            "PgyPinnedSlotView_%s __pgy_pin_%d "
            "__attribute__((cleanup(pgy_unpin_cleanup_%s))) = "
            "pgy_pin_%s_%s(%s);\n",
            slot->inner_type, pin_id, slot->inner_type,
            mode, slot->inner_type,
            transpiler_block_slot_address_local(slot, slot_addr, sizeof(slot_addr)));
    }

    return true;
}

void
emit_block(ASTNode *node, TranspilerCtx *ctx)
{
    if (node == NULL)
        return;

    if (node->type == AST_BLOCK) {
        bool pin_scope_open = emit_pin_block_enter_local(node, ctx);
        int saved_slot_count = ctx->slot_var_count;
        int saved_typed_count = ctx->typed_var_count;
        int saved_alias_count = ctx->alias_var_count;
        for (size_t i = 0; i < node->data.block.count; i++)
            emit_statement(node->data.block.statements[i], ctx);

        /* Slot sugar: auto-release slot vars declared in this scope (LIFO).
         * Skip slots already explicitly released by the user. */
        for (int i = ctx->slot_var_count - 1; i >= saved_slot_count; i--) {
            SlotVarEntry *e = &ctx->slot_vars[i];
            if (e->released) continue;
            write_indent(ctx);
            if (e->is_secure) {
                codebuf_write(ctx->out,
                    "pgy_secure_release_%s(&%s, &%s_token);\n",
                    e->inner_type, e->name, e->name);
            } else {
                codebuf_write(ctx->out,
                    "pgy_release_%s(&%s);\n",
                    e->inner_type, e->name);
            }
        }

        transpiler_restore_local_binding_counts_local(ctx, saved_slot_count,
                                                      saved_typed_count,
                                                      saved_alias_count);
        if (pin_scope_open) {
            ctx->indent--;
            write_indent(ctx);
            codebuf_write(ctx->out, "}\n");
        }
    } else {
        emit_statement(node, ctx);
    }
}
