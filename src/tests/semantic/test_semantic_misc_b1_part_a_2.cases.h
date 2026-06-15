    TEST("explicit cross-module private method access is rejected");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *program = ast_create_program();
        ASTNode *vault = ast_create_subject("Vault");
        ASTNode *call = make_call_expr(
            make_member_access(make_identifier("v", 2), "Reveal", 2),
            NULL, 0, 2);
        ASTNode *method = ast_create_function("Reveal");
        Symbol *vsym;

        vault->origin_path = pergyra_strdup("vault_module.pgy");
        method->data.func_decl.return_type = ast_create_type("Int");
        method->data.func_decl.body = ast_create_block();
        method->data.func_decl.access = ACCESS_PRIVATE;
        method->data.func_decl.has_explicit_access = true;
        ast_add_statement(method->data.func_decl.body,
            ast_create_return_statement());
        method->data.func_decl.body->data.block.statements[0]
            ->data.return_stmt.value = make_number(7, 1);
        vault->data.class_decl.methods = calloc(1, sizeof(ASTNode *));
        vault->data.class_decl.methods[0] = method;
        vault->data.class_decl.method_count = 1;
        ast_add_statement(program, vault);
        ctx->program_root = program;

        type_check_class_decl(vault, ctx);
        ctx->current_module_path = "consumer_module.pgy";
        vsym = symbol_create_variable("v",
            scope_lookup(ctx->scope, "Vault")->type, 2, 1);
        scope_declare(ctx->scope, vsym);

        type_check_expression(call, ctx);
        EXPECT(ctx->has_error);
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "not accessible across the current visibility boundary"));

        semantic_context_destroy(ctx);
        ast_destroy(call);
        ast_destroy(program);
    }

    TEST("explicit cross-module public method access remains allowed");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *program = ast_create_program();
        ASTNode *vault = ast_create_subject("Vault");
        ASTNode *call = make_call_expr(
            make_member_access(make_identifier("v", 2), "Reveal", 2),
            NULL, 0, 2);
        ASTNode *method = ast_create_function("Reveal");
        Symbol *vsym;

        vault->origin_path = pergyra_strdup("vault_module.pgy");
        method->data.func_decl.return_type = ast_create_type("Int");
        method->data.func_decl.body = ast_create_block();
        method->data.func_decl.access = ACCESS_PUBLIC;
        method->data.func_decl.has_explicit_access = true;
        ast_add_statement(method->data.func_decl.body,
            ast_create_return_statement());
        method->data.func_decl.body->data.block.statements[0]
            ->data.return_stmt.value = make_number(7, 1);
        vault->data.class_decl.methods = calloc(1, sizeof(ASTNode *));
        vault->data.class_decl.methods[0] = method;
        vault->data.class_decl.method_count = 1;
        ast_add_statement(program, vault);
        ctx->program_root = program;

        type_check_class_decl(vault, ctx);
        ctx->current_module_path = "consumer_module.pgy";
        vsym = symbol_create_variable("v",
            scope_lookup(ctx->scope, "Vault")->type, 2, 1);
        scope_declare(ctx->scope, vsym);

        EXPECT(type_check_expression(call, ctx) == TYPE_INT);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(call);
        ast_destroy(program);
    }

    TEST("for-in over List<subject> type-checks");
    {
        const char *source =
            "subject Event {\n"
            "    let title: String;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let events: List<Event> = ListNew();\n"
            "    ListPush(events, Event(\"Kickoff\"));\n"
            "    for event in events {\n"
            "        Print(event.title);\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();

        type_check_program(program, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("string interpolation type-checks embedded ints and member access");
    {
        const char *source =
            "subject Event {\n"
            "    let title: String;\n"
            "    let day: Int;\n"
            "}\n"
            "func Render(event: Event) -> String {\n"
            "    return \"Day ${event.day}: ${event.title}\";\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();

        type_check_program(program, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("Some(42) returns Option<Int>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ASTNode *args[1] = { make_number(42, 1) };
        ASTNode *call = make_call("Some", args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error);
        EXPECT(t != NULL
            && t->kind == TYPE_KIND_CONSTRUCTED
            && type_equals(t->data.constructed.constructor, TYPE_OPTION));
        EXPECT(t != NULL
            && t->data.constructed.arg_count >= 1
            && t->data.constructed.args[0] == TYPE_INT);
        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("IsSome requires Option<T>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ASTNode *args[1] = { make_number(42, 1) };
        ASTNode *call = make_call("IsSome", args, 1, 1);
        type_check_expression(call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "IsSome requires Option<T>"));
        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("Option coalescing returns inner type");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let maybe: Option<Int> = Some(7);\n"
            "    let value: Int = maybe ?? 0;\n"
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

    TEST("Option coalescing rejects non-Option left operand");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let value: Int = 1 ?? 0;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Option coalescing requires Option<T> on the left"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ToTObject returns tobject projection type but warns as boundary projection outside domain context");
    {
        const char *source =
            "tobject PlayerDto { hp: Int; name: String; }\n"
            "subject Player { let hp: Int; let name: String; }\n"
            "func Main() -> Void {\n"
            "    let player: Player = Player();\n"
            "    let snapshot: PlayerDto = ToTObject(PlayerDto, player);\n"
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

    TEST("ToTObject accepts subject source");
    {
        const char *source =
            "tobject BotDto { hp: Int; }\n"
            "subject Bot {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let bot: Bot = Bot();\n"
            "    let snapshot: BotDto = ToTObject(BotDto, bot);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count == 1);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "direct boundary projection outside relation/effect/zone/world context"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ToTObject rejects missing fields, non-tobject targets, and unnamed sources");
    {
        const char *source =
            "tobject PlayerDto { hp: Int; name: String; }\n"
            "object PlayerView { hp: Int; name: String; }\n"
            "subject Player { let hp: Int; }\n"
            "class PassivePlayer { let hp: Int; let name: String; }\n"
            "func Main() -> Void {\n"
            "    let player: Player = Player();\n"
            "    let passive: PassivePlayer = PassivePlayer();\n"
            "    let missing: PlayerDto = ToTObject(PlayerDto, player);\n"
            "    let wrong = ToTObject(Player, player);\n"
            "    let wrongView = ToTObject(PlayerView, player);\n"
            "    let anon = ToTObject(PlayerDto, Player());\n"
            "    let legacy = ToTObject(PlayerDto, passive);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 5);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ToObject returns object projection type but warns as internal projection outside domain context");
    {
        const char *source =
            "object PlayerView { hp: Int; name: String; }\n"
            "subject Player { let hp: Int; let name: String; }\n"
            "func Main() -> Void {\n"
            "    let player: Player = Player();\n"
            "    let view: PlayerView = ToObject(PlayerView, player);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count == 1);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "direct internal projection outside relation/effect/zone/world context"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("object and tobject declarations may carry passive helper funcs");
    {
        const char *source =
            "object PlayerView {\n"
            "    hp: Int;\n"
            "    func Label(self) -> Int {\n"
            "        return hp;\n"
            "    }\n"
            "}\n"
            "tobject PlayerDto {\n"
            "    hp: Int;\n"
            "    func Export(self) -> Int {\n"
            "        return hp;\n"
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

    TEST("vessel declarations and subject vessel fields are accepted");
    {
        const char *source =
            "vessel HealthState {\n"
            "    current: Int;\n"
            "    func IsDead(self) -> Bool {\n"
            "        return current <= 0;\n"
            "    }\n"
            "}\n"
            "subject Player {\n"
            "    vessel health: HealthState;\n"
            "    action TakeDamage(self, amount: Int) -> Void {\n"
            "        Log(amount);\n"
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

    TEST("private nominal member access is allowed inside the same host");
    {
        const char *source =
            "class Vault {\n"
            "    private let secret: Int;\n"
            "    private func Hidden(self) -> Int {\n"
            "        return self.secret;\n"
            "    }\n"
            "    public func Reveal(self) -> Int {\n"
            "        return self.Hidden();\n"
            "    }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let v: Vault = Vault(7);\n"
            "    Log(v.Reveal());\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        if (result != NULL && result->error_count > 0)
            semantic_result_print(result);
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("private nominal member access is rejected outside the host");
    {
        const char *source =
            "class Vault {\n"
            "    private let secret: Int;\n"
            "    private func Hidden(self) -> Int {\n"
            "        return self.secret;\n"
            "    }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let v: Vault = Vault(7);\n"
            "    Log(v.secret);\n"
            "    Log(v.Hidden());\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Member 'Vault.secret' is not accessible across the current visibility boundary"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Member 'Vault.Hidden' is not accessible across the current visibility boundary"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("unknown nominal field and method access are rejected");
    {
        const char *source =
            "class Vault {\n"
            "    public let visible: Int;\n"
            "    public func Reveal(self) -> Int {\n"
            "        return self.visible;\n"
            "    }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let v: Vault = Vault(7);\n"
            "    Log(v.missing);\n"
            "    Log(v.Missing());\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Unknown member 'Vault.missing'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Unknown method 'Vault.Missing'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("subject vessel fields reject non-vessel target types");
    {
        const char *source =
            "object PlayerView { hp: Int; }\n"
            "subject Player {\n"
            "    vessel view: PlayerView;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "must reference a vessel type"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
