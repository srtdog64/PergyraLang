static int
run_intent_step_using_derivation_ast_print_test(void)
{
    const char *code =
        "subject Courier {\n"
        "    let level: Int;\n"
        "}\n"
        "zone DeliveryZone {\n"
        "    subject slot courier: Courier\n"
        "}\n"
        "intent MoveCargo(deliver: DeliveryZone, courier: Courier) {\n"
        "    step Deliver {\n"
        "        using: deliver;\n"
        "        who: courier;\n"
        "        expect: true;\n"
        "    }\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = NULL;
    ASTNode *ast = NULL;
    ASTNode *intent_decl = NULL;
    ASTNode *step = NULL;

    printf("\n=== Test: Intent Step Using-Derived Provenance In AST Print ===\n");

    if (lexer == NULL) {
        printf("[FAIL] Failed to create lexer\n");
        return 1;
    }

    parser = parser_create(lexer);
    if (parser == NULL) {
        printf("[FAIL] Failed to create parser\n");
        lexer_destroy(lexer);
        return 1;
    }

    ast = parser_parse_program(parser);
    if (parser_has_error(parser)) {
        printf("[FAIL] Parse error: %s\n", parser_get_error(parser));
        failed = 1;
        goto cleanup;
    }

    if (ast == NULL || ast->type != AST_PROGRAM || ast->data.program.count < 3) {
        printf("[FAIL] Expected parsed program with subject, zone, and intent\n");
        failed = 1;
        goto cleanup;
    }

    intent_decl = ast->data.program.statements[2];
    if (intent_decl == NULL || intent_decl->type != AST_INTENT_DECL
        || intent_decl->data.intent_decl.step_count != 1) {
        printf("[FAIL] Expected parsed intent with one step\n");
        failed = 1;
        goto cleanup;
    }

    step = intent_decl->data.intent_decl.steps[0];
    if (step == NULL || step->type != AST_INTENT_STEP) {
        printf("[FAIL] Expected intent step node\n");
        failed = 1;
        goto cleanup;
    }

    step->data.intent_step.derived_where_from_using = true;
    step->data.intent_step.derived_using_from_where = true;

    if (!ast_print_contains(ast, "derived zone from using binding")) {
        printf("[FAIL] Expected AST print to include using-derived zone provenance\n");
        failed = 1;
        goto cleanup;
    }
    if (!ast_print_contains(ast, "derived using from zone type")) {
        printf("[FAIL] Expected AST print to include where-derived using provenance\n");
        failed = 1;
        goto cleanup;
    }

    printf("AST print includes using-derived zone and where-derived using provenance.\n");

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_intent_header_value_param_ast_test(void)
{
    const char *code =
        "subject Buyer {\n"
        "    let hp: Int;\n"
        "}\n"
        "zone PaymentZone {\n"
        "    subject slot buyer: Buyer\n"
        "}\n"
        "intent Purchase(payment: PaymentZone, buyer: Buyer, price: Int) {\n"
        "    step pay {\n"
        "        where: PaymentZone;\n"
        "        using: payment;\n"
        "        who: buyer;\n"
        "        on: price > 0;\n"
        "    }\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = NULL;
    ASTNode *ast = NULL;
    ASTNode *intent_decl = NULL;
    ASTNode *value = NULL;

    printf("\n=== Test: Intent Header Value Parameter Classification ===\n");

    if (lexer == NULL) {
        printf("[FAIL] Failed to create lexer\n");
        return 1;
    }

    parser = parser_create(lexer);
    if (parser == NULL) {
        printf("[FAIL] Failed to create parser\n");
        lexer_destroy(lexer);
        return 1;
    }

    ast = parser_parse_program(parser);
    if (parser_has_error(parser)) {
        printf("[FAIL] Parse error: %s\n", parser_get_error(parser));
        failed = 1;
        goto cleanup;
    }

    if (ast == NULL || ast->type != AST_PROGRAM || ast->data.program.count < 3) {
        printf("[FAIL] Expected parsed program with subject, zone, and intent\n");
        failed = 1;
        goto cleanup;
    }

    intent_decl = ast->data.program.statements[2];
    if (intent_decl == NULL || intent_decl->type != AST_INTENT_DECL) {
        printf("[FAIL] Expected intent declaration node\n");
        failed = 1;
        goto cleanup;
    }

    if (intent_decl->data.intent_decl.involve_count != 2
        || intent_decl->data.intent_decl.value_count != 1) {
        printf("[FAIL] Expected 2 intent bindings and 1 value binding, got %zu and %zu\n",
               intent_decl->data.intent_decl.involve_count,
               intent_decl->data.intent_decl.value_count);
        failed = 1;
        goto cleanup;
    }

    value = intent_decl->data.intent_decl.values[0];
    if (value == NULL || value->type != AST_INTENT_VALUE
        || value->data.intent_value.alias == NULL
        || strcmp(value->data.intent_value.alias, "price") != 0) {
        printf("[FAIL] Expected trailing Int header parameter to become intent value binding\n");
        failed = 1;
        goto cleanup;
    }

    printf("Intent header value parameter is classified as an intent value binding.\n");

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_intent_interleaved_header_binding_order_ast_test(void)
{
    const char *code =
        "subject Buyer {\n"
        "    let hp: Int;\n"
        "}\n"
        "struct PriceQuote {\n"
        "    amount: Int;\n"
        "}\n"
        "zone CheckoutZone {\n"
        "    subject slot buyer: Buyer\n"
        "}\n"
        "intent Checkout(checkout: CheckoutZone, quote: PriceQuote, buyer: Buyer, price: Int, adjustments: Array<Int>) {\n"
        "    step pay {\n"
        "        where: CheckoutZone;\n"
        "        using: checkout;\n"
        "        who: buyer;\n"
        "        on: quote.amount >= price;\n"
        "        expect: ArrayLength(adjustments) == 2;\n"
        "    }\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = NULL;
    ASTNode *ast = NULL;
    ASTNode *intent_decl = NULL;
    ASTNode *binding0 = NULL;
    ASTNode *binding1 = NULL;
    ASTNode *binding2 = NULL;
    ASTNode *binding3 = NULL;

    printf("\n=== Test: Intent Interleaved Header Binding Order ===\n");

    if (lexer == NULL) {
        printf("[FAIL] Failed to create lexer\n");
        return 1;
    }

    parser = parser_create(lexer);
    if (parser == NULL) {
        printf("[FAIL] Failed to create parser\n");
        lexer_destroy(lexer);
        return 1;
    }

    ast = parser_parse_program(parser);
    if (parser_has_error(parser)) {
        printf("[FAIL] Parse error: %s\n", parser_get_error(parser));
        failed = 1;
        goto cleanup;
    }

    if (ast == NULL || ast->type != AST_PROGRAM || ast->data.program.count < 4) {
        printf("[FAIL] Expected parsed program with subject, struct, zone, and intent\n");
        failed = 1;
        goto cleanup;
    }

    intent_decl = ast->data.program.statements[3];
    if (intent_decl == NULL || intent_decl->type != AST_INTENT_DECL) {
        printf("[FAIL] Expected intent declaration node\n");
        failed = 1;
        goto cleanup;
    }

    if (intent_decl->data.intent_decl.binding_count != 5
        || intent_decl->data.intent_decl.involve_count != 2
        || intent_decl->data.intent_decl.value_count != 3) {
        printf("[FAIL] Expected 5 total bindings (2 involves, 3 values), got %zu (%zu involves, %zu values)\n",
               intent_decl->data.intent_decl.binding_count,
               intent_decl->data.intent_decl.involve_count,
               intent_decl->data.intent_decl.value_count);
        failed = 1;
        goto cleanup;
    }

    binding0 = intent_decl->data.intent_decl.bindings[0];
    binding1 = intent_decl->data.intent_decl.bindings[1];
    binding2 = intent_decl->data.intent_decl.bindings[2];
    binding3 = intent_decl->data.intent_decl.bindings[3];
    ASTNode *binding4 = intent_decl->data.intent_decl.bindings[4];

    if (binding0 == NULL || binding0->type != AST_INTENT_INVOLVES
        || strcmp(binding0->data.intent_involves.alias, "checkout") != 0
        || binding1 == NULL || binding1->type != AST_INTENT_VALUE
        || strcmp(binding1->data.intent_value.alias, "quote") != 0
        || binding2 == NULL || binding2->type != AST_INTENT_INVOLVES
        || strcmp(binding2->data.intent_involves.alias, "buyer") != 0
        || binding3 == NULL || binding3->type != AST_INTENT_VALUE
        || strcmp(binding3->data.intent_value.alias, "price") != 0
        || binding4 == NULL || binding4->type != AST_INTENT_VALUE
        || strcmp(binding4->data.intent_value.alias, "adjustments") != 0) {
        printf("[FAIL] Expected bindings[] to preserve declared order: checkout, quote, buyer, price, adjustments\n");
        failed = 1;
        goto cleanup;
    }

    printf("Intent header bindings preserve declared order and classify passive nominals as values.\n");

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_vessel_keyword_alias_test(void)
{
    const char *code =
        "vessel HealthState {\n"
        "    current: Int;\n"
        "    max: Int;\n"
        "    func IsDead(self) -> Bool {\n"
        "        return current <= 0;\n"
        "    }\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = NULL;
    ASTNode *ast = NULL;
    ASTNode *decl = NULL;

    printf("\n=== Test: Vessel Keyword Surface ===\n");

    if (lexer == NULL) {
        printf("[FAIL] Failed to create lexer\n");
        return 1;
    }

    parser = parser_create(lexer);
    if (parser == NULL) {
        printf("[FAIL] Failed to create parser\n");
        lexer_destroy(lexer);
        return 1;
    }

    ast = parser_parse_program(parser);
    if (parser_has_error(parser)) {
        printf("[FAIL] Parse error: %s\n", parser_get_error(parser));
        failed = 1;
        goto cleanup;
    }

    if (ast == NULL || ast->type != AST_PROGRAM || ast->data.program.count != 1) {
        printf("[FAIL] Expected program with one declaration\n");
        failed = 1;
        goto cleanup;
    }

    decl = ast->data.program.statements[0];
    if (decl == NULL || decl->type != AST_CLASS_DECL || !decl->data.class_decl.is_struct
        || decl->data.class_decl.nominal_kind != NOMINAL_DECL_VESSEL) {
        printf("[FAIL] Expected 'vessel' to parse as vessel declaration\n");
        failed = 1;
        goto cleanup;
    }

    if (strcmp(decl->data.class_decl.name, "HealthState") != 0
        || decl->data.class_decl.field_count != 2
        || decl->data.class_decl.method_count != 1) {
        printf("[FAIL] Vessel declaration members were not parsed correctly\n");
        failed = 1;
        goto cleanup;
    }

    printf("Vessel keyword parsed successfully as vessel declaration!\n");

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_lexical_zone_context_test(void)
{
    const char *code =
        "within BattleZone {\n"
        "    subject Hero {\n"
        "        let hp: Int;\n"
        "        action Guard(self) authorized by self {\n"
        "            return;\n"
        "        }\n"
        "    }\n"
        "}\n"
        "zone BattleZone {\n"
        "    subject slot hero: Hero\n"
        "    authority hero\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = NULL;
    ASTNode *ast = NULL;
    ASTNode *subject_decl = NULL;
    ASTNode *method = NULL;

    printf("\n=== Test: Lexical Zone Context ===\n");

    if (lexer == NULL) {
        printf("[FAIL] Failed to create lexer\n");
        return 1;
    }

    parser = parser_create(lexer);
    if (parser == NULL) {
        printf("[FAIL] Failed to create parser\n");
        lexer_destroy(lexer);
        return 1;
    }

    ast = parser_parse_program(parser);
    if (parser_has_error(parser)) {
        printf("[FAIL] Parse error: %s\n", parser_get_error(parser));
        failed = 1;
        goto cleanup;
    }

    if (ast == NULL || ast->type != AST_PROGRAM || ast->data.program.count < 2) {
        printf("[FAIL] Expected flattened program statements after lexical zone context\n");
        failed = 1;
        goto cleanup;
    }

    subject_decl = ast->data.program.statements[0];
    if (subject_decl == NULL || subject_decl->type != AST_CLASS_DECL
        || subject_decl->data.class_decl.method_count != 1) {
        printf("[FAIL] Expected subject declaration with one action method\n");
        failed = 1;
        goto cleanup;
    }

    method = subject_decl->data.class_decl.methods[0];
    if (method == NULL || method->type != AST_FUNC_DECL
        || method->data.func_decl.within_zone == NULL
        || strcmp(method->data.func_decl.within_zone, "BattleZone") != 0) {
        printf("[FAIL] Expected lexical zone context to inject within BattleZone\n");
        failed = 1;
        goto cleanup;
    }

    printf("Lexical zone context injected default within-clause successfully!\n");

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}
