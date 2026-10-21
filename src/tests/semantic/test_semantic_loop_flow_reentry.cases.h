/* A loop checked again by an enclosing loop's fixed point, from another
 * entry, exports one transfer row: the one its last check used. Two rows
 * for one loop made HIR refuse the routine. */
static const PgyLoopFlowSummaryFact *
loop_flow_reentry_row(const SemanticResult *result, uint32_t loop_id, size_t *rows)
{
    const PgyLoopFlowSummaryFact *found = NULL;
    *rows = 0;
    for (size_t i = 0; i < result->loop_flow_summary_fact_count; i++) {
        if (result->loop_flow_summary_facts[i].loop_syntax_id != loop_id)
            continue;
        (*rows)++;
        found = &result->loop_flow_summary_facts[i];
    }
    return found;
}

/* The state row of the named binding in a summary's entry range. */
static const PgyLoopFlowStateFact *
loop_flow_reentry_entry_state(const SemanticResult *result,
                              const PgyLoopFlowSummaryFact *summary,
                              const char *name)
{
    size_t stable_index = SIZE_MAX;
    for (size_t i = 0; i < result->resource_flow_fact_count; i++) {
        const PgyResourceFlowFact *fact = &result->resource_flow_facts[i];
        if (fact->function_syntax_id == summary->function_syntax_id
            && fact->name != NULL && strcmp(fact->name, name) == 0)
            stable_index = fact->stable_index;
    }
    for (size_t i = 0; stable_index != SIZE_MAX && i < summary->entry_state_count; i++) {
        const PgyLoopFlowStateFact *state =
            &result->loop_flow_state_facts[summary->entry_state_start + i];
        if (state->stable_index == stable_index)
            return state;
    }
    return NULL;
}

static void
test_loop_flow_reentry(void)
{
    const char *source =
        "func Accept(inout ids: Array<Int>, id: Int) -> Bool {\n"
        "    ArrayPush(ids, id);\n"
        "    return true;\n"
        "}\n"
        "func Main() -> Void {\n"
        "    var ids: Array<Int> = [];\n"
        "    let node_id: Int = 0;\n"
        "    while node_id < 3 {\n"
        "        let actual_i: Int = 0;\n"
        "        while actual_i < 2 {\n"
        "            actual_i = actual_i + 1;\n"
        "        }\n"
        "        if !Accept(ids, node_id) { Log(0); }\n"
        "        node_id = node_id + 1;\n"
        "    }\n"
        "    Log(ArrayLength(ids));\n"
        "}\n";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticResult *result = semantic_analyze(program);
    bool admitted = !parser_has_error(parser) && result != NULL && result->success;
    ASTNode *main_decl = admitted ? ast_program_statement(result->annotated_ast, 1) : NULL;
    ASTNode *body = main_decl != NULL && main_decl->type == AST_FUNC_DECL
        ? ast_func_body(main_decl) : NULL;
    ASTNode *outer = body != NULL ? ast_block_statement(body, 2) : NULL;
    ASTNode *inner = outer != NULL && outer->type == AST_WHILE_LOOP
        ? ast_block_statement(ast_while_body(outer), 1) : NULL;
    size_t outer_rows = 0, inner_rows = 0;
    const PgyLoopFlowSummaryFact *inner_row = inner != NULL && inner->type == AST_WHILE_LOOP
        ? loop_flow_reentry_row(result, ast_node_stable_id(inner), &inner_rows) : NULL;
    if (outer != NULL)
        (void)loop_flow_reentry_row(result, ast_node_stable_id(outer), &outer_rows);
    const PgyLoopFlowStateFact *ids = inner_row != NULL
        ? loop_flow_reentry_entry_state(result, inner_row, "ids") : NULL;

    TEST("a loop an enclosing fixed point checks again exports one row");
    EXPECT(admitted && outer_rows == 1 && inner_rows == 1);
    TEST("that row is the transfer of the enclosing loop's last entry");
    EXPECT(ids != NULL && ids->is_used);
    semantic_result_destroy(result);
    ast_destroy(program);
    parser_destroy(parser);
    lexer_destroy(lexer);
}
