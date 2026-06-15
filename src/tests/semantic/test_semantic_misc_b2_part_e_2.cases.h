    TEST("generic ability default type argument resolves in impl reference");
    {
        const char *source =
            "ability Bufferable<T = Int> {\n"
            "    fields item: T;\n"
            "}\n"
            "subject Bag {\n"
            "    let item: Int;\n"
            "}\n"
            "role IntBuffer for Bag {\n"
            "    impl ability Bufferable {\n"
            "    }\n"
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

    TEST("action requires may omit trailing generic default arguments");
    {
        const char *source =
            "ability Bufferable<T = Int> { func Put(value: T) -> Void; }\n"
            "subject Bag {\n"
            "    let item: Int;\n"
            "    action Save(self) -> Void requires Bufferable {\n"
            "        return;\n"
            "    }\n"
            "}\n"
            "role IntBuffer for Bag {\n"
            "    impl ability Bufferable { func Put(value: Int) -> Void { return; } }\n"
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

    TEST("intent step requires may omit trailing generic default arguments");
    {
        const char *source =
            "ability Bufferable<T = Int> { func Put(value: T) -> Void; }\n"
            "subject Bag { action Save(self) -> Void { return; } }\n"
            "zone StorageZone { subject slot bag: Bag }\n"
            "role IntBuffer for Bag {\n"
            "    impl ability Bufferable { func Put(value: Int) -> Void { return; } }\n"
            "}\n"
            "intent Persist(storage: StorageZone, bag: Bag) {\n"
            "    step Save {\n"
            "        where: StorageZone;\n"
            "        using: storage;\n"
            "        who: bag;\n"
            "        requires: Bufferable;\n"
            "        on: bag.Save();\n"
            "    }\n"
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

    TEST("zone authority may omit trailing generic default arguments");
    {
        const char *source =
            "ability Bufferable<T = Int> { func Put(value: T) -> Void; }\n"
            "subject Bag { let item: Int; }\n"
            "role IntBuffer for Bag {\n"
            "    impl ability Bufferable { func Put(value: Int) -> Void { return; } }\n"
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

    TEST("party role slot may omit trailing generic default arguments");
    {
        const char *source =
            "ability Bufferable<T = Int> { func Put(value: T) -> Void; }\n"
            "subject Bag { let item: Int; }\n"
            "role IntBuffer for Bag {\n"
            "    impl ability Bufferable { func Put(value: Int) -> Void { return; } }\n"
            "}\n"
            "party StorageParty {\n"
            "    role slot buffer: Bufferable\n"
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

    TEST("cross-module consumers may omit trailing generic default arguments");
    {
        const char *module_path = "test_generic_default_contract_module_a.pgy";
        const char *main_path = "test_generic_default_contract_main_a.pgy";
        const char *module_source =
            "ability Bufferable<T = Int> { func Put(value: T) -> Void; }\n";
        const char *main_source =
            "import \"test_generic_default_contract_module_a.pgy\";\n"
            "subject Bag { let item: Int; }\n"
            "role IntBuffer for Bag {\n"
            "    impl ability Bufferable<Int> { func Put(value: Int) -> Void { return; } }\n"
            "}\n"
            "party StorageParty {\n"
            "    role slot buffer: Bufferable\n"
            "}\n"
            "zone StorageZone {\n"
            "    subject slot bag: Bag\n"
            "    authority bag requires Bufferable\n"
            "}\n";
        FILE *module_file = fopen(module_path, "wb");
        FILE *main_file = fopen(main_path, "wb");
        char *error_message = NULL;
        ASTNode *program = NULL;
        SemanticResult *result = NULL;

        EXPECT(module_file != NULL && main_file != NULL);
        if (module_file != NULL) {
            fputs(module_source, module_file);
            fclose(module_file);
        }
        if (main_file != NULL) {
            fputs(main_source, main_file);
            fclose(main_file);
        }

        program = import_resolver_load_program(main_path, &error_message);
        EXPECT(program != NULL);
        if (program != NULL)
            result = semantic_analyze(program);
        EXPECT(error_message == NULL);
        EXPECT(result != NULL && result->error_count == 0);

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(module_path);
    }

    TEST("cross-module action requires may omit trailing generic default arguments");
    {
        const char *module_path = "test_generic_default_contract_module_b.pgy";
        const char *main_path = "test_generic_default_contract_main_b.pgy";
        const char *module_source =
            "ability Bufferable<T = Int> { func Put(value: T) -> Void; }\n";
        const char *main_source =
            "import \"test_generic_default_contract_module_b.pgy\";\n"
            "subject Bag {\n"
            "    let item: Int;\n"
            "    action Store(self) -> Void requires Bufferable { return; }\n"
            "}\n"
            "role IntBuffer for Bag {\n"
            "    impl ability Bufferable<Int> { func Put(value: Int) -> Void { return; } }\n"
            "}\n";
        FILE *module_file = fopen(module_path, "wb");
        FILE *main_file = fopen(main_path, "wb");
        char *error_message = NULL;
        ASTNode *program = NULL;
        SemanticResult *result = NULL;

        EXPECT(module_file != NULL && main_file != NULL);
        if (module_file != NULL) {
            fputs(module_source, module_file);
            fclose(module_file);
        }
        if (main_file != NULL) {
            fputs(main_source, main_file);
            fclose(main_file);
        }

        program = import_resolver_load_program(main_path, &error_message);
        EXPECT(program != NULL);
        if (program != NULL)
            result = semantic_analyze(program);
        EXPECT(error_message == NULL);
        EXPECT(result != NULL && result->error_count == 0);

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(module_path);
    }

    TEST("cross-module intent step requires may omit trailing generic default arguments");
    {
        const char *module_path = "test_generic_default_contract_module_c.pgy";
        const char *main_path = "test_generic_default_contract_main_c.pgy";
        const char *module_source =
            "ability Bufferable<T = Int> { func Put(value: T) -> Void; }\n";
        const char *main_source =
            "import \"test_generic_default_contract_module_c.pgy\";\n"
            "subject Bag {\n"
            "    let item: Int;\n"
            "    action Store(self) -> Void { return; }\n"
            "}\n"
            "zone StorageZone { subject slot bag: Bag }\n"
            "role IntBuffer for Bag {\n"
            "    impl ability Bufferable<Int> { func Put(value: Int) -> Void { return; } }\n"
            "}\n"
            "intent Persist(storage: StorageZone, bag: Bag) {\n"
            "    step store {\n"
            "        where: StorageZone;\n"
            "        using: storage;\n"
            "        who: bag;\n"
            "        requires: Bufferable;\n"
            "        on: bag.Store();\n"
            "    }\n"
            "}\n";
        FILE *module_file = fopen(module_path, "wb");
        FILE *main_file = fopen(main_path, "wb");
        char *error_message = NULL;
        ASTNode *program = NULL;
        SemanticResult *result = NULL;

        EXPECT(module_file != NULL && main_file != NULL);
        if (module_file != NULL) {
            fputs(module_source, module_file);
            fclose(module_file);
        }
        if (main_file != NULL) {
            fputs(main_source, main_file);
            fclose(main_file);
        }

        program = import_resolver_load_program(main_path, &error_message);
        EXPECT(program != NULL);
        if (program != NULL)
            result = semantic_analyze(program);
        EXPECT(error_message == NULL);
        EXPECT(result != NULL && result->error_count == 0);

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(module_path);
    }

    TEST("cross-module role impl may omit trailing generic default arguments");
    {
        const char *module_path = "test_generic_default_contract_module_d.pgy";
        const char *main_path = "test_generic_default_contract_main_d.pgy";
        const char *module_source =
            "ability Bufferable<T = Int> { func Put(value: T) -> Void; }\n";
        const char *main_source =
            "import \"test_generic_default_contract_module_d.pgy\";\n"
            "subject Bag { let item: Int; }\n"
            "role IntBuffer for Bag {\n"
            "    impl ability Bufferable { func Put(value: Int) -> Void { return; } }\n"
            "}\n";
        FILE *module_file = fopen(module_path, "wb");
        FILE *main_file = fopen(main_path, "wb");
        char *error_message = NULL;
        ASTNode *program = NULL;
        SemanticResult *result = NULL;

        EXPECT(module_file != NULL && main_file != NULL);
        if (module_file != NULL) {
            fputs(module_source, module_file);
            fclose(module_file);
        }
        if (main_file != NULL) {
            fputs(main_source, main_file);
            fclose(main_file);
        }

        program = import_resolver_load_program(main_path, &error_message);
        EXPECT(program != NULL);
        if (program != NULL)
            result = semantic_analyze(program);
        EXPECT(error_message == NULL);
        EXPECT(result != NULL && result->error_count == 0);

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(module_path);
    }

    TEST("cross-module zone authority rejects imported generic ability reference that violates one of multiple where bounds");
    {
        const char *module_path = "test_generic_multibound_contract_module_a.pgy";
        const char *main_path = "test_generic_multibound_contract_main_a.pgy";
        const char *module_source =
            "ability Comparable { }\n"
            "ability Cloneable { }\n"
            "ability Bufferable<T> where T: Comparable + Cloneable {\n"
            "    func Put(value: T) -> Void;\n"
            "}\n";
        const char *main_source =
            "import \"test_generic_multibound_contract_module_a.pgy\";\n"
            "subject Item { let hp: Int; }\n"
            "role ItemComparable for Item {\n"
            "    impl ability Comparable { }\n"
            "}\n"
            "zone StorageZone {\n"
            "    subject slot item: Item\n"
            "    authority item requires Bufferable<Item>\n"
            "}\n";
        FILE *module_file = fopen(module_path, "wb");
        FILE *main_file = fopen(main_path, "wb");
        char *error_message = NULL;
        ASTNode *program = NULL;
        SemanticResult *result = NULL;

        EXPECT(module_file != NULL && main_file != NULL);
        if (module_file != NULL) {
            fputs(module_source, module_file);
            fclose(module_file);
        }
        if (main_file != NULL) {
            fputs(main_source, main_file);
            fclose(main_file);
        }

        program = import_resolver_load_program(main_path, &error_message);
        EXPECT(program != NULL);
        if (program != NULL)
            result = semantic_analyze(program);
        EXPECT(error_message == NULL);
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "does not satisfy bound 'Cloneable'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "consumer path is Zone authority 'item'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "expected type args are 'Bufferable<Item>'"));

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(module_path);
    }

    TEST("cross-module action requires rejects imported generic ability reference that violates one of multiple where bounds");
    {
        const char *module_path = "test_generic_multibound_contract_module_b.pgy";
        const char *main_path = "test_generic_multibound_contract_main_b.pgy";
        const char *module_source =
            "ability Comparable { }\n"
            "ability Cloneable { }\n"
            "ability Bufferable<T> where T: Comparable + Cloneable {\n"
            "    func Put(value: T) -> Void;\n"
            "}\n";
        const char *main_source =
            "import \"test_generic_multibound_contract_module_b.pgy\";\n"
            "subject Item {\n"
            "    let hp: Int;\n"
            "    action Save(self) -> Void requires Bufferable<Item> { return; }\n"
            "}\n"
            "role ItemComparable for Item {\n"
            "    impl ability Comparable { }\n"
            "}\n";
        FILE *module_file = fopen(module_path, "wb");
        FILE *main_file = fopen(main_path, "wb");
        char *error_message = NULL;
        ASTNode *program = NULL;
        SemanticResult *result = NULL;

        EXPECT(module_file != NULL && main_file != NULL);
        if (module_file != NULL) {
            fputs(module_source, module_file);
            fclose(module_file);
        }
        if (main_file != NULL) {
            fputs(main_source, main_file);
            fclose(main_file);
        }

        program = import_resolver_load_program(main_path, &error_message);
        EXPECT(program != NULL);
        if (program != NULL)
            result = semantic_analyze(program);
        EXPECT(error_message == NULL);
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "does not satisfy bound 'Cloneable'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "consumer path is action 'Save'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "expected type args are 'Bufferable<Item>'"));

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(module_path);
    }
