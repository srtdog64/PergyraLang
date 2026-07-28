static int
run_doc_comment_attachment_test(void)
{
    const char *code =
        "/// @effects remote secure\n"
        "func RemoteOp() -> Void {\n"
        "    Log(1);\n"
        "}\n"
        "class Worker {\n"
        "    /// [Effects]: secure\n"
        "    func Run() -> Void {\n"
        "        Log(2);\n"
        "    }\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = NULL;
    ASTNode *ast = NULL;
    ASTNode *func = NULL;
    ASTNode *klass = NULL;
    ASTNode *method = NULL;

    printf("\n=== Test: Structured Comment Attachment ===\n");

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
        printf("[FAIL] Expected program with at least two declarations\n");
        failed = 1;
        goto cleanup;
    }

    func = ast->data.program.statements[0];
    klass = ast->data.program.statements[1];
    if (func == NULL || func->type != AST_FUNC_DECL ||
        func->data.func_decl.doc_comment == NULL ||
        func->data.func_decl.doc_comment->tag_count != 1 ||
        func->data.func_decl.doc_comment->tags[0]->type != DOC_TAG_EFFECTS ||
        strcmp(func->data.func_decl.doc_comment->tags[0]->content, "remote secure") != 0) {
        printf("[FAIL] Top-level function doc comment was not attached correctly\n");
        failed = 1;
        goto cleanup;
    }

    if (klass == NULL || klass->type != AST_CLASS_DECL || klass->data.class_decl.method_count != 1) {
        printf("[FAIL] Expected class with one method\n");
        failed = 1;
        goto cleanup;
    }

    method = klass->data.class_decl.methods[0];
    if (method == NULL || method->type != AST_FUNC_DECL ||
        method->data.func_decl.doc_comment == NULL ||
        method->data.func_decl.doc_comment->tag_count != 1 ||
        method->data.func_decl.doc_comment->tags[0]->type != DOC_TAG_EFFECTS ||
        strcmp(method->data.func_decl.doc_comment->tags[0]->content, "secure") != 0) {
        printf("[FAIL] Class method doc comment was not attached correctly\n");
        failed = 1;
        goto cleanup;
    }

    printf("Structured comments attached successfully!\n");

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_signature_effect_clause_test(void)
{
    const char *code =
        "func RemoteOp() -> Void with effects remote, secure {\n"
        "    Log(1);\n"
        "}\n"
        "async func Tick() -> Int with effects local {\n"
        "    return 2;\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = NULL;
    ASTNode *ast = NULL;
    ASTNode *func = NULL;
    ASTNode *async_func = NULL;

    printf("\n=== Test: Signature Effect Clause ===\n");

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
        printf("[FAIL] Expected program with two declarations\n");
        failed = 1;
        goto cleanup;
    }

    func = ast->data.program.statements[0];
    async_func = ast->data.program.statements[1];
    if (func == NULL || func->type != AST_FUNC_DECL
        || !func->data.func_decl.has_effects_clause
        || func->data.func_decl.declared_effects != (EFFECT_REMOTE | EFFECT_SECURE)) {
        printf("[FAIL] Regular function signature effects were not parsed correctly\n");
        failed = 1;
        goto cleanup;
    }

    if (async_func == NULL || async_func->type != AST_FUNC_DECL
        || !async_func->data.func_decl.has_effects_clause
        || async_func->data.func_decl.declared_effects != EFFECT_NONE
        || !async_func->is_async_decl) {
        printf("[FAIL] Async function signature effects were not parsed correctly\n");
        failed = 1;
        goto cleanup;
    }

    printf("Signature effect clauses parsed successfully!\n");

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_subject_keyword_alias_test(void)
{
    const char *code =
        "subject Player {\n"
        "    let hp: Int;\n"
        "    action TakeDamage(amount: Int) -> Void\n"
        "        requires Damageable\n"
        "        within BattleZone\n"
        "        causes DamageEffect\n"
        "        authorized by self {\n"
        "        hp = hp - amount;\n"
        "    }\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = NULL;
    ASTNode *ast = NULL;
    ASTNode *decl = NULL;

    printf("\n=== Test: Subject Keyword Surface ===\n");

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
    if (decl == NULL || decl->type != AST_CLASS_DECL) {
        printf("[FAIL] Expected 'subject' to parse as AST_CLASS_DECL\n");
        failed = 1;
        goto cleanup;
    }

    if (decl->data.class_decl.nominal_kind != NOMINAL_DECL_SUBJECT
        || strcmp(decl->data.class_decl.name, "Player") != 0
        || decl->data.class_decl.field_count != 1
        || decl->data.class_decl.method_count != 1
        || !decl->data.class_decl.methods[0]->data.func_decl.is_action
        || decl->data.class_decl.methods[0]->data.func_decl.required_ability_count != 1
        || decl->data.class_decl.methods[0]->data.func_decl.within_zone == NULL
        || decl->data.class_decl.methods[0]->data.func_decl.causes_effect == NULL
        || decl->data.class_decl.methods[0]->data.func_decl.authorized_by_count != 1) {
        printf("[FAIL] Subject declaration members were not parsed correctly\n");
        failed = 1;
        goto cleanup;
    }

    printf("Subject keyword parsed successfully as subject declaration!\n");

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_action_clause_reordering_test(void)
{
    const char *code =
        "subject Player {\n"
        "    action Attack(self, target: Player) -> Void\n"
        "        authorized by self, target\n"
        "        causes DamageEffect\n"
        "        within BattleZone\n"
        "        requires Combatable, Movable\n"
        "        with effects secure, remote\n"
        "        where T: Combatable {\n"
        "        return;\n"
        "    }\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = NULL;
    ASTNode *ast = NULL;
    ASTNode *decl = NULL;
    ASTNode *method = NULL;

    printf("\n=== Test: Action Clause Reordering ===\n");

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
    if (decl == NULL || decl->type != AST_CLASS_DECL
        || decl->data.class_decl.method_count != 1) {
        printf("[FAIL] Expected subject with one action declaration\n");
        failed = 1;
        goto cleanup;
    }

    method = decl->data.class_decl.methods[0];
    if (method == NULL || method->type != AST_FUNC_DECL
        || method->data.func_decl.where_clause == NULL
        || !method->data.func_decl.has_effects_clause
        || method->data.func_decl.declared_effects != (EFFECT_SECURE | EFFECT_REMOTE)
        || method->data.func_decl.required_ability_count != 2
        || method->data.func_decl.within_zone == NULL
        || strcmp(method->data.func_decl.within_zone, "BattleZone") != 0
        || method->data.func_decl.causes_effect == NULL
        || strcmp(method->data.func_decl.causes_effect, "DamageEffect") != 0
        || method->data.func_decl.authorized_by_count != 2) {
        printf("[FAIL] Reordered action clauses were not parsed correctly\n");
        failed = 1;
        goto cleanup;
    }

    printf("Action clauses parsed successfully regardless of order!\n");

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_duplicate_action_clause_diagnostic_test(void)
{
    const char *code =
        "subject Player {\n"
        "    action Attack(self) -> Void\n"
        "        requires Combatable\n"
        "        requires Movable {\n"
        "        return;\n"
        "    }\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = NULL;
    ASTNode *ast = NULL;
    const char *error = NULL;

    printf("\n=== Test: Duplicate Action Clause Diagnostic ===\n");

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
    if (!parser_has_error(parser)) {
        printf("[FAIL] Expected duplicate clause parse error\n");
        failed = 1;
        goto cleanup;
    }

    error = parser_get_error(parser);
    if (error == NULL || strstr(error, "Duplicate 'requires' clause") == NULL) {
        printf("[FAIL] Expected duplicate clause diagnostic, got: %s\n",
               error != NULL ? error : "<null>");
        failed = 1;
        goto cleanup;
    }

    printf("Duplicate action clause reports an explicit diagnostic.\n");

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_malformed_effect_clause_diagnostic_test(void)
{
    const char *code =
        "func RemoteOp() -> Void with remote {\n"
        "    return;\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = NULL;
    ASTNode *ast = NULL;
    const char *error = NULL;

    printf("\n=== Test: Malformed Effect Clause Diagnostic ===\n");

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
    if (!parser_has_error(parser)) {
        printf("[FAIL] Expected malformed effect clause parse error\n");
        failed = 1;
        goto cleanup;
    }

    error = parser_get_error(parser);
    if (error == NULL
        || strstr(error, "Expected 'effects' after 'with' in function/action clause") == NULL
        || strstr(error, "use 'with effects ...'") == NULL) {
        printf("[FAIL] Expected helpful with-effects diagnostic, got: %s\n",
               error != NULL ? error : "<null>");
        failed = 1;
        goto cleanup;
    }

    printf("Malformed effect clause reports an explicit fix-oriented diagnostic.\n");

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_authorized_clause_missing_by_test(void)
{
    const char *code =
        "subject Player {\n"
        "    action Guard(self) -> Void\n"
        "        authorized self {\n"
        "        return;\n"
        "    }\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = NULL;
    ASTNode *ast = NULL;
    const char *error = NULL;

    printf("\n=== Test: Authorized Clause Missing 'by' Diagnostic ===\n");

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
    if (!parser_has_error(parser)) {
        printf("[FAIL] Expected missing 'by' parse error\n");
        failed = 1;
        goto cleanup;
    }

    error = parser_get_error(parser);
    if (error == NULL
        || strstr(error, "Expected 'by' after 'authorized' in function/action clause") == NULL
        || strstr(error, "use 'authorized by <subject>'") == NULL) {
        printf("[FAIL] Expected helpful authorized-by diagnostic, got: %s\n",
               error != NULL ? error : "<null>");
        failed = 1;
        goto cleanup;
    }

    printf("Authorized clause missing 'by' reports an explicit fix-oriented diagnostic.\n");

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_intent_step_within_clause_hint_test(void)
{
    const char *code =
        "intent Purchase() {\n"
        "    step Pay {\n"
        "        within PaymentZone;\n"
        "    }\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = NULL;
    ASTNode *ast = NULL;
    const char *error = NULL;

    printf("\n=== Test: Intent Step 'within' Hint Diagnostic ===\n");

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
    if (!parser_has_error(parser)) {
        printf("[FAIL] Expected intent step clause parse error\n");
        failed = 1;
        goto cleanup;
    }

    error = parser_get_error(parser);
    if (error == NULL
        || strstr(error, "'within' is an action clause") == NULL
        || strstr(error, "use 'where: <Zone>;' on the step") == NULL
        || strstr(error, "reuse the matching action zone contract") == NULL) {
        printf("[FAIL] Expected helpful within->where diagnostic, got: %s\n",
               error != NULL ? error : "<null>");
        failed = 1;
        goto cleanup;
    }

    printf("Intent step misuse of 'within' reports a fix-oriented diagnostic.\n");

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_intent_step_with_effects_hint_test(void)
{
    const char *code =
        "intent Purchase() {\n"
        "    step Pay {\n"
        "        with effects secure;\n"
        "    }\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = NULL;
    ASTNode *ast = NULL;
    const char *error = NULL;

    printf("\n=== Test: Intent Step 'with effects' Hint Diagnostic ===\n");

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
    if (!parser_has_error(parser)) {
        printf("[FAIL] Expected intent step with-effects parse error\n");
        failed = 1;
        goto cleanup;
    }

    error = parser_get_error(parser);
    if (error == NULL
        || strstr(error, "'with effects ...' is not a valid intent step clause") == NULL
        || strstr(error, "use 'causes: <Effect>;' on the step") == NULL
        || strstr(error, "matching action") == NULL) {
        printf("[FAIL] Expected helpful with-effects intent-step diagnostic, got: %s\n",
               error != NULL ? error : "<null>");
        failed = 1;
        goto cleanup;
    }

    printf("Intent step misuse of 'with effects' reports a fix-oriented diagnostic.\n");

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_intent_step_outcome_binding_parse_test(void)
{
    const char *code =
        "intent Produce() {\n"
        "    step Bound {\n"
        "        on outcome: ProduceValue();\n"
        "    }\n"
        "    step Legacy {\n"
        "        on: ProduceValue();\n"
        "    }\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = NULL;
    ASTNode *program = NULL;
    ASTNode *intent = NULL;
    ASTNode *bound = NULL;
    ASTNode *legacy = NULL;

    printf("\n=== Test: Intent Step Outcome Binding Parse ===\n");
    if (lexer == NULL)
        return 1;
    parser = parser_create(lexer);
    if (parser == NULL) {
        lexer_destroy(lexer);
        return 1;
    }
    program = parser_parse_program(parser);
    if (program != NULL && ast_program_statement_count(program) == 1)
        intent = ast_program_statement(program, 0);
    if (intent != NULL && ast_intent_decl_step_count(intent) == 2) {
        ASTNode **steps = ast_intent_decl_steps(intent, NULL);
        bound = steps != NULL ? steps[0] : NULL;
        legacy = steps != NULL ? steps[1] : NULL;
    }

    if (parser_has_error(parser)
        || bound == NULL || legacy == NULL
        || ast_intent_step_outcome_binding_name(bound) == NULL
        || strcmp(ast_intent_step_outcome_binding_name(bound), "outcome") != 0
        || ast_intent_step_outcome_binding_length(bound) != 7
        || ast_intent_step_outcome_binding_line(bound) != 3
        || ast_intent_step_outcome_binding_column(bound) == 0
        || ast_intent_step_on_expr_count(bound) != 1
        || ast_intent_step_outcome_binding_name(legacy) != NULL
        || ast_intent_step_on_expr_count(legacy) != 1) {
        printf("[FAIL] Bound/legacy on-clause AST facts were not preserved: %s\n",
            parser_get_error(parser) != NULL
                ? parser_get_error(parser) : "<no parser diagnostic>");
        failed = 1;
    }

    ast_destroy(program);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_intent_step_duplicate_outcome_binding_parse_test(void)
{
    const char *code =
        "intent Produce() {\n"
        "    step Bound {\n"
        "        on first: ProduceValue();\n"
        "        on second: ProduceValue();\n"
        "    }\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = NULL;
    ASTNode *program = NULL;
    const char *error;

    printf("\n=== Test: Intent Step Duplicate Outcome Binding ===\n");
    if (lexer == NULL)
        return 1;
    parser = parser_create(lexer);
    if (parser == NULL) {
        lexer_destroy(lexer);
        return 1;
    }
    program = parser_parse_program(parser);
    error = parser_get_error(parser);
    if (!parser_has_error(parser) || error == NULL
        || strstr(error, "Duplicate outcome binding in intent step") == NULL) {
        printf("[FAIL] Expected duplicate outcome-binding diagnostic, got: %s\n",
            error != NULL ? error : "<null>");
        failed = 1;
    }

    ast_destroy(program);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_parser_reentry_cleanup_test(void)
{
    const char *valid_code =
        "struct Vec2 {\n"
        "    x: Int;\n"
        "    y: Int;\n"
        "}\n"
        "func Main() -> Vec2 {\n"
        "    return Vec2(1, 2);\n"
        "}\n";
    const char *invalid_code =
        "func Main( -> Void {\n"
        "    return;\n"
        "}\n";
    int failed = 0;

    printf("\n=== Test: Parser Reentry Cleanup ===\n");

    for (int i = 0; i < 64; i++) {
        Lexer *lexer = lexer_create(valid_code);
        Parser *parser = NULL;
        ASTNode *ast = NULL;

        if (lexer == NULL) {
            printf("[FAIL] Failed to create lexer for valid reentry iteration %d\n", i);
            return 1;
        }

        parser = parser_create(lexer);
        if (parser == NULL) {
            printf("[FAIL] Failed to create parser for valid reentry iteration %d\n", i);
            lexer_destroy(lexer);
            return 1;
        }

        ast = parser_parse_program(parser);
        if (parser_has_error(parser) || ast == NULL || ast->type != AST_PROGRAM) {
            printf("[FAIL] Valid parser reentry iteration %d failed\n", i);
            failed = 1;
        }

        ast_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);

        if (failed)
            return failed;
    }

    for (int i = 0; i < 64; i++) {
        Lexer *lexer = lexer_create(invalid_code);
        Parser *parser = NULL;
        ASTNode *ast = NULL;

        if (lexer == NULL) {
            printf("[FAIL] Failed to create lexer for invalid reentry iteration %d\n", i);
            return 1;
        }

        parser = parser_create(lexer);
        if (parser == NULL) {
            printf("[FAIL] Failed to create parser for invalid reentry iteration %d\n", i);
            lexer_destroy(lexer);
            return 1;
        }

        ast = parser_parse_program(parser);
        if (!parser_has_error(parser) || parser_get_error(parser) == NULL) {
            printf("[FAIL] Invalid parser reentry iteration %d unexpectedly succeeded\n", i);
            failed = 1;
        }

        ast_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);

        if (failed)
            return failed;
    }

    printf("Parser success/error cleanup stays stable across repeated same-process runs!\n");
    return 0;
}
