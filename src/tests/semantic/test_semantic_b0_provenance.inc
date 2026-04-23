static void
test_b0_provenance_closure_diagnostics(void)
{
    printf("\n[b0_provenance]\n");

    TEST("branch effect conflict warning reports reason and fix");
    {
        const char *source =
            "/// @effects secure, collapse\n"
            "func Mix() -> Void {\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "combines effect classes that are currently treated as conflicting"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "authority-sensitive work and boundary/resource work"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("if branch effect conflict warning reports branch provenance");
    {
        const char *source =
            "/// @effects secure\n"
            "func SecureWork() -> Void {\n"
            "    return;\n"
            "}\n"
            "/// @effects remote\n"
            "func RemoteWork() -> Void {\n"
            "    return;\n"
            "}\n"
            "func Mix(flag: Bool) -> Void {\n"
            "    if flag {\n"
            "        SecureWork();\n"
            "    } else {\n"
            "        RemoteWork();\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "then branch contributes 'secure'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "else branch contributes 'remote'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("action causes within zone missing authorized by reports reason and fix");
    {
        const char *source =
            "subject Hero {\n"
            "    let hp: Int;\n"
            "    action Guard(self) -> Void\n"
            "        causes Poisoned\n"
            "        within BattleZone\n"
            "    {\n"
            "        return;\n"
            "    }\n"
            "}\n"
            "effect Poisoned for bearer: Hero { }\n"
            "zone BattleZone {\n"
            "    subject slot hero: Hero\n"
            "    effect slot poison: Poisoned\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "must declare 'authorized by'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "action contract declares causes 'Poisoned'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "approval provenance for that state change is missing"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("action requires default generic ability resolves to effective contract");
    {
        const char *source =
            "ability Bufferable<T = Int> {\n"
            "    func Put(value: T) -> Void;\n"
            "}\n"
            "subject Bag {\n"
            "    let count: Int;\n"
            "    action Save(self) -> Void requires Bufferable {\n"
            "        return;\n"
            "    }\n"
            "}\n"
            "role BagBuffer for Bag {\n"
            "    impl ability Bufferable {\n"
            "        func Put(value: Int) -> Void {\n"
            "            return;\n"
            "        }\n"
            "    }\n"
            "}\n"
            "zone StorageZone {\n"
            "    subject slot bag: Bag\n"
            "    authority bag requires Bufferable\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("action requires generic mismatch reports effective expected and actual type args");
    {
        const char *source =
            "ability Bufferable<T = Int> {\n"
            "    func Put(value: T) -> Void;\n"
            "}\n"
            "subject Bag {\n"
            "    action Save(self) -> Void requires Bufferable {\n"
            "        return;\n"
            "    }\n"
            "}\n"
            "role BagBuffer for Bag {\n"
            "    impl ability Bufferable<String> {\n"
            "        func Put(value: String) -> Void {\n"
            "            return;\n"
            "        }\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "requires ability 'Bufferable<Int>'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "actual implementation is 'Bufferable<String>'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "actual type args are 'Bufferable<String>'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ability default generic multi-bound failure reports broken bound provenance");
    {
        const char *source =
            "ability Comparable { }\n"
            "ability Cloneable { }\n"
            "subject Item {\n"
            "    let value: Int;\n"
            "}\n"
            "role ItemComparable for Item {\n"
            "    impl ability Comparable { }\n"
            "}\n"
            "ability Packable<T = Item> where T: Comparable + Cloneable {\n"
            "    func Accept(value: T) -> Void;\n"
            "}\n"
            "subject Bag {\n"
            "    action Save(self) -> Void requires Packable {\n"
            "        return;\n"
            "    }\n"
            "}\n"
            "role BagPackable for Bag {\n"
            "    impl ability Packable {\n"
            "        func Accept(value: Item) -> Void {\n"
            "            return;\n"
            "        }\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "full bound set is 'T: Comparable + Cloneable'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "actual type args are"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("class omitted default generic multi-bound failure reports instantiation provenance");
    {
        const char *source =
            "ability Comparable { }\n"
            "ability Cloneable { }\n"
            "subject Item {\n"
            "    let value: Int;\n"
            "}\n"
            "role ItemComparable for Item {\n"
            "    impl ability Comparable { }\n"
            "}\n"
            "class Box<T = Item> where T: Comparable + Cloneable {\n"
            "    let value: T;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let box: Box = Box(Item(1));\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Contract source:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "class declaration contract is 'Box<T>'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "validation path is 'Box<Item>'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "full bound set is 'T: Comparable + Cloneable'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "expected type args are 'Box<T>'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "actual type args are 'Box<Item>'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "instantiated type argument is 'Item'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone authority omitted default generic multi-bound failure reports consumer provenance");
    {
        const char *source =
            "ability Comparable { }\n"
            "ability Cloneable { }\n"
            "subject Item {\n"
            "    let value: Int;\n"
            "}\n"
            "role ItemComparable for Item {\n"
            "    impl ability Comparable { }\n"
            "}\n"
            "ability Packable<T = Item> where T: Comparable + Cloneable {\n"
            "    func Accept(value: T) -> Void;\n"
            "}\n"
            "zone StorageZone {\n"
            "    subject slot item: Item\n"
            "    authority item requires Packable\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "consumer path is Zone authority 'item'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "full bound set is 'T: Comparable + Cloneable'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "actual type args are"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("action authorized by zone authority mismatch reports reason and fix");
    {
        const char *source =
            "subject Healer {\n"
            "    let hp: Int;\n"
            "}\n"
            "subject Guard {\n"
            "    action Protect(self, healer: Healer) -> Void\n"
            "        within BattleZone\n"
            "        authorized by healer\n"
            "    {\n"
            "        return;\n"
            "    }\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot guard: Guard\n"
            "    subject slot healer: Healer\n"
            "    authority guard\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "declares no matching authority"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "within-zone contract comes from action clause 'within BattleZone'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "derives authority provenance from binding 'healer'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "authority check edge is action 'Protect' -> zone 'BattleZone' -> binding 'healer'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "zone 'BattleZone' has a subject slot for that type but no authority contract"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}
