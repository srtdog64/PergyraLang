static void
test_resource_flow_symbol_lifetime(void)
{
    TEST("resource-flow facts outlive inner-block Symbol storage");
    {
        const char *source =
            "func Track() -> Void {\n"
            "    if true {\n"
            "        let slot: Slot<Int> = ClaimSlot<Int>();\n"
            "        if true {\n"
            "            Write(slot, 1);\n"
            "        }\n"
            "        Release(slot);\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = NULL;
        bool found_slot = false;

        if (program != NULL && ast_assign_stable_ids(program))
            result = semantic_analyze(program);
        if (result != NULL) {
            for (size_t i = 0; i < result->resource_flow_fact_count; i++) {
                const PgyResourceFlowFact *fact =
                    &result->resource_flow_facts[i];
                if (fact->name != NULL && strcmp(fact->name, "slot") == 0) {
                    found_slot = !fact->is_parameter
                        && fact->declaration_syntax_id != 0;
                    break;
                }
            }
        }

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(found_slot);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}
