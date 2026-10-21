/* The published parameter-flow snapshot carries each formal's Array storage
 * verdict. A later stage may free a caller's Array after a declared call
 * only from this verdict; a zero Slot mask is not that proof. */
static bool
array_preservation_fact_for(const SemanticResult *result, const char *function,
                            uint32_t *preserved)
{
    uint32_t id = 0;
    for (size_t i = 0; result->annotated_ast != NULL
         && i < ast_program_statement_count(result->annotated_ast); i++) {
        ASTNode *statement = ast_program_statement(result->annotated_ast, i);
        if (statement != NULL && statement->type == AST_FUNC_DECL
            && statement->data.func_decl.name != NULL
            && strcmp(statement->data.func_decl.name, function) == 0)
            id = ast_node_stable_id(statement);
    }
    for (size_t i = 0; id != 0 && i < result->function_param_flow_fact_count; i++) {
        const PgyFunctionParamFlowFact *fact = &result->function_param_flow_facts[i];
        if (fact->function_syntax_id == id && fact->parameter_index == 0) {
            *preserved = fact->array_preserved;
            return true;
        }
    }
    return false;
}

static void
test_array_preservation_facet(void)
{
    const char *source =
        "func Count(rows: Array<String>) -> Int { return ArrayLength(rows); }\n"
        "func Peek(ref rows: Array<String>) -> Int { return ArrayLength(rows); }\n"
        "func Rewrite(inout rows: Array<String>) -> Void { rows[0] = Concat(\"b\", \"\"); }\n"
        "func Later(rows: Array<String>) -> Int {\n"
        "    defer { Log(ArrayLength(rows)); }\n"
        "    return 0;\n"
        "}\n"
        "func Take(own rows: Array<String>) -> Int { return ArrayLength(rows); }\n"
        "func Main() -> Void {\n"
        "    var n: Int = 0;\n"
        "    var total: Int = 0;\n"
        "    while n < 3 {\n"
        "        var rows: Array<String> = [];\n"
        "        ArrayPush(rows, \"x\");\n"
        "        total = total + Peek(rows);\n"
        "        n = n + 1;\n"
        "    }\n"
        "    var first: Array<String> = [];\n"
        "    ArrayPush(first, Concat(\"a\", \"\"));\n"
        "    Log(Count(first));\n"
        "    var second: Array<String> = [];\n"
        "    ArrayPush(second, Concat(\"a\", \"\"));\n"
        "    Rewrite(second);\n"
        "    var third: Array<String> = [];\n"
        "    ArrayPush(third, Concat(\"a\", \"\"));\n"
        "    Log(Later(third));\n"
        "    var fourth: Array<String> = [];\n"
        "    ArrayPush(fourth, Concat(\"a\", \"\"));\n"
        "    Log(Take(fourth) + total);\n"
        "}\n";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticResult *result = semantic_analyze(program);
    uint32_t count = 0, peek = 0, rewrite = 0, later = 0, take = 0;
    bool admitted = !parser_has_error(parser) && result != NULL && result->success;
    bool has_count = admitted && array_preservation_fact_for(result, "Count", &count);
    bool has_peek = admitted && array_preservation_fact_for(result, "Peek", &peek);
    bool has_rewrite = admitted && array_preservation_fact_for(result, "Rewrite", &rewrite);
    bool has_later = admitted && array_preservation_fact_for(result, "Later", &later);
    bool has_take = admitted && array_preservation_fact_for(result, "Take", &take);

    TEST("a read-only Array formal is published fully preserved");
    EXPECT(has_count && count == PGY_FUNCTION_PARAM_ARRAY_PRESERVED_ALL);
    TEST("a ref Array formal read in a loop call is fully preserved");
    EXPECT(has_peek && peek == PGY_FUNCTION_PARAM_ARRAY_PRESERVED_ALL);
    TEST("an Array element write is published descriptor-only");
    EXPECT(has_rewrite && rewrite == PGY_FUNCTION_PARAM_ARRAY_DESCRIPTOR_PRESERVED);
    TEST("an unproved Array formal use is published unpreserved");
    EXPECT(has_later && later == 0);
    /* An own handoff demands no summary; a missing row is unproved. */
    TEST("an own Array formal is never published as preserved");
    EXPECT(admitted && (!has_take || take == 0));
    semantic_result_destroy(result);
    ast_destroy(program);
    parser_destroy(parser);
    lexer_destroy(lexer);
}
