static void
test_slot_sugar(void)
{
    printf("\n[slot_sugar]\n");

    TranspilerCtx *ctx;

    TEST("let x: Slot<Int> = 42 -> claim + write");
    {
        ASTNode *node = make_let("x", make_type_node("Slot<Int>"),
                                  make_number(42, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT(strstr(out, "pgy_claim_Int()") != NULL);
        EXPECT(strstr(out, "pgy_write_Int(&x, 42)") != NULL);
        transpiler_ctx_destroy(ctx);
    }

    TEST("slot sugar: identifier auto-read via Log");
    {
        /* Build: func Main() -> Void { let x: Slot<Int> = 42; Log(x); } */
        ASTNode *let_node = make_let("x", make_type_node("Slot<Int>"),
                                      make_number(42, 1), 1);

        ASTNode *x_ident = calloc(1, sizeof(ASTNode));
        x_ident->type = AST_IDENTIFIER; x_ident->line = 2;
        x_ident->data.identifier.name = pergyra_strdup("x");

        ASTNode *log_call = calloc(1, sizeof(ASTNode));
        log_call->type = AST_CALL; log_call->line = 2;
        ASTNode *log_id = calloc(1, sizeof(ASTNode));
        log_id->type = AST_IDENTIFIER; log_id->data.identifier.name = pergyra_strdup("Log");
        log_call->data.call.callee = log_id;
        log_call->data.call.arguments = malloc(sizeof(ASTNode*));
        log_call->data.call.arguments[0] = x_ident;
        log_call->data.call.arg_count = 1;
        ctx = transpiler_ctx_create();
        emit_statement(let_node, ctx);
        emit_statement(log_call, ctx);
        EXPECT(strstr(ctx->out->data, "pgy_read_Int(&") != NULL);
        transpiler_ctx_destroy(ctx);
    }

    TEST("slot sugar: x = 5 auto-write");
    {
        ASTNode *let_node = make_let("x", make_type_node("Slot<Int>"),
                                      make_number(42, 1), 1);

        ASTNode *assign = calloc(1, sizeof(ASTNode));
        assign->type = AST_ASSIGNMENT; assign->line = 2;
        ASTNode *tgt = calloc(1, sizeof(ASTNode));
        tgt->type = AST_IDENTIFIER; tgt->data.identifier.name = pergyra_strdup("x");
        assign->data.assignment.target = tgt;
        assign->data.assignment.value  = make_number(5, 2);
        ctx = transpiler_ctx_create();
        emit_statement(let_node, ctx);
        emit_statement(assign, ctx);
        EXPECT(strstr(ctx->out->data, "pgy_write_Int(&") != NULL);
        transpiler_ctx_destroy(ctx);
    }

    TEST("explicit Release prevents double release");
    {
        /* let a: Slot<Int> = ClaimSlot<Int>(); Write(a,10); Release(a); */
        ASTNode *args0[0];
        ASTNode *init = make_call("ClaimSlot", args0, 0, 1);
        ASTNode *let_node = make_let("a", make_type_node("Slot<Int>"), init, 1);

        ASTNode *a_id = calloc(1, sizeof(ASTNode));
        a_id->type = AST_IDENTIFIER; a_id->data.identifier.name = pergyra_strdup("a");
        ASTNode *w_args[] = { a_id, make_number(10, 2) };
        ASTNode *write_call = make_call("Write", w_args, 2, 2);

        ASTNode *a_id2 = calloc(1, sizeof(ASTNode));
        a_id2->type = AST_IDENTIFIER; a_id2->data.identifier.name = pergyra_strdup("a");
        ASTNode *r_args[] = { a_id2 };
        ASTNode *rel_call = make_call("Release", r_args, 1, 3);
        ctx = transpiler_ctx_create();
        emit_statement(let_node, ctx);
        emit_statement(write_call, ctx);
        emit_statement(rel_call, ctx);

        int count = 0;
        const char *p = ctx->out->data;
        while ((p = strstr(p, "pgy_release_Int")) != NULL) { count++; p++; }
        EXPECT(count == 1);
        transpiler_ctx_destroy(ctx);
    }

    TEST("pin block emits typed runtime pin wrapper with cleanup");
    {
        ASTNode *let_node = make_let("x", make_type_node("Slot<Int>"),
                                      make_number(42, 1), 1);
        ASTNode *pin_block = calloc(1, sizeof(ASTNode));
        pin_block->type = AST_BLOCK;
        pin_block->line = 2;
        pin_block->data.block.statements = NULL;
        pin_block->data.block.count = 0;
        pin_block->data.block.is_pin_block = true;
        pin_block->data.block.pin_view_is_write = true;
        pin_block->data.block.pin_source_name = pergyra_strdup("x");
        pin_block->data.block.pin_view_name = pergyra_strdup("view");

        ctx = transpiler_ctx_create();
        emit_statement(let_node, ctx);
        emit_block(pin_block, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "PgyPinnedSlotView_Int");
        EXPECT_STR_CONTAINS(ctx->out->data, "cleanup(pgy_unpin_cleanup_Int)");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_pin_write_Int(&x)");

        ast_destroy(pin_block);
        transpiler_ctx_destroy(ctx);
    }
}

