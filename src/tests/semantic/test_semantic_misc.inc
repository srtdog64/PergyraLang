static void
test_misc_grammar_edges(void)
{
    printf("\n[misc_grammar]\n");

    TEST("unsafe block type-checks its body");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *body = ast_create_block();
        ASTNode *args[1] = { make_number(1, 1) };
        ast_add_statement(body, make_call("Log", args, 1, 1));
        ASTNode *unsafe_block = ast_create_unsafe_block(body);
        unsafe_block->line = 1; unsafe_block->column = 1;

        type_check_statement(unsafe_block, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(unsafe_block);
    }

    TEST("defer statement type-checks its body");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *body = ast_create_block();
        ASTNode *args[1] = { make_number(1, 1) };
        ast_add_statement(body, make_call("Log", args, 1, 1));
        ASTNode *defer_stmt = ast_create_defer_statement(body);
        defer_stmt->line = 1; defer_stmt->column = 1;

        type_check_statement(defer_stmt, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(defer_stmt);
    }

    TEST("bind statement is accepted by semantic pass");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *bind = ast_create_bind_statement("team", "fighter", "Warrior");
        bind->line = 1; bind->column = 1;

        type_check_statement(bind, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(bind);
    }

    TEST("else if chain type-checks nested branch structure");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *then_block = ast_create_block();
        ASTNode *then_args[1] = { make_number(1, 1) };
        ast_add_statement(then_block, make_call("Log", then_args, 1, 1));

        ASTNode *else_then = ast_create_block();
        ASTNode *else_then_args[1] = { make_number(2, 1) };
        ast_add_statement(else_then, make_call("Log", else_then_args, 1, 1));

        ASTNode *else_final = ast_create_block();
        ASTNode *else_final_args[1] = { make_number(3, 1) };
        ast_add_statement(else_final, make_call("Log", else_final_args, 1, 1));

        ASTNode *nested_if = ast_create_if_statement();
        nested_if->data.if_stmt.condition = ast_create_boolean(false);
        nested_if->data.if_stmt.then_branch = else_then;
        nested_if->data.if_stmt.else_branch = else_final;

        ASTNode *outer_if = ast_create_if_statement();
        outer_if->data.if_stmt.condition = ast_create_boolean(true);
        outer_if->data.if_stmt.then_branch = then_block;
        outer_if->data.if_stmt.else_branch = nested_if;
        outer_if->line = 1; outer_if->column = 1;

        type_check_statement(outer_if, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(outer_if);
    }

    /* ---- RemoteFuture design: await returns Result, Claim/Read/Write/Release rejected ---- */

    TEST("await on local Future<Int> returns Int (not Result)");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->in_async_func = true;

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("local_future", future_type, 1, 1));

        ASTNode *await_expr =
            ast_create_await_expression(make_identifier("local_future", 1));
        Type *t = type_check_expression(await_expr, ctx);
        EXPECT(!ctx->has_error);
        EXPECT(t == TYPE_INT);

        semantic_context_destroy(ctx);
        ast_destroy(await_expr);
    }

    TEST("await on RemoteFuture<Int> returns Result<Int>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->in_async_func = true;

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_REMOTE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("remote_future", future_type, 1, 1));

        ASTNode *await_expr =
            ast_create_await_expression(make_identifier("remote_future", 1));
        Type *t = type_check_expression(await_expr, ctx);
        EXPECT(!ctx->has_error);
        EXPECT(t != NULL
            && t->kind == TYPE_KIND_CONSTRUCTED
            && type_equals(t->data.constructed.constructor, TYPE_RESULT));
        EXPECT(t != NULL
            && t->data.constructed.arg_count >= 1
            && t->data.constructed.args[0] == TYPE_INT);

        semantic_context_destroy(ctx);
        ast_destroy(await_expr);
    }

    TEST("Read on RemoteFuture is rejected with helpful error");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_REMOTE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("rf", future_type, 1, 1));

        ASTNode *read_args[1] = { make_identifier("rf", 1) };
        ASTNode *read_call = make_call("Read", read_args, 1, 1);
        type_check_expression(read_call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "RemoteFuture does not support Read"));

        semantic_context_destroy(ctx);
        ast_destroy(read_call);
    }

    TEST("Write on RemoteFuture is rejected with helpful error");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_REMOTE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("rf", future_type, 1, 1));

        ASTNode *write_args[2] = { make_identifier("rf", 1), make_number(42, 1) };
        ASTNode *write_call = make_call("Write", write_args, 2, 1);
        type_check_expression(write_call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "RemoteFuture does not support Write"));

        semantic_context_destroy(ctx);
        ast_destroy(write_call);
    }

    TEST("Release on RemoteFuture is rejected with helpful error");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_REMOTE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("rf", future_type, 1, 1));

        ASTNode *release_args[1] = { make_identifier("rf", 1) };
        ASTNode *release_call = make_call("Release", release_args, 1, 1);
        type_check_expression(release_call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "RemoteFuture does not support Release"));

        semantic_context_destroy(ctx);
        ast_destroy(release_call);
    }

    TEST("Option<Int> annotation resolves to constructed type");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ASTNode *opt = make_generic_type("Option", "Int");
        Type *t = resolve_type_node(opt, ctx);
        EXPECT(!ctx->has_error);
        EXPECT(t != NULL
            && t->kind == TYPE_KIND_CONSTRUCTED
            && type_equals(t->data.constructed.constructor, TYPE_OPTION));
        EXPECT(t != NULL
            && t->data.constructed.arg_count >= 1
            && t->data.constructed.args[0] == TYPE_INT);
        semantic_context_destroy(ctx);
        ast_destroy(opt);
    }

    TEST("type alias annotations resolve in program declarations");
    {
        const char *source =
            "type UserId = Int;\n"
            "type NameList = List<String>;\n"
            "func Main() -> Void {\n"
            "    let id: UserId = 7;\n"
            "    let names: NameList = ListNew();\n"
            "    ListPush(names, \"gyri\");\n"
            "    Log(id);\n"
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

    TEST("explicit cross-module private field access is rejected");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *program = ast_create_program();
        ASTNode *vault = ast_create_subject("Vault");
        ASTNode *access = ast_create_member_access(make_identifier("v", 2), "code");
        ClassField *field = calloc(1, sizeof(ClassField));
        Symbol *vsym;

        vault->origin_path = pergyra_strdup("vault_module.pgy");
        field->name = pergyra_strdup("code");
        field->type = ast_create_type("Int");
        field->access = ACCESS_PRIVATE;
        field->has_explicit_access = true;
        vault->data.class_decl.fields = calloc(1, sizeof(ClassField *));
        vault->data.class_decl.fields[0] = field;
        vault->data.class_decl.field_count = 1;
        ast_add_statement(program, vault);
        ctx->program_root = program;

        type_check_class_decl(vault, ctx);
        ctx->current_module_path = "consumer_module.pgy";
        vsym = symbol_create_variable("v",
            scope_lookup(ctx->scope, "Vault")->type, 2, 1);
        scope_declare(ctx->scope, vsym);

        type_check_expression(access, ctx);
        EXPECT(ctx->has_error);
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "not accessible across the current visibility boundary"));

        semantic_context_destroy(ctx);
        ast_destroy(access);
        ast_destroy(program);
    }

    TEST("explicit cross-module public field access remains allowed");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *program = ast_create_program();
        ASTNode *vault = ast_create_subject("Vault");
        ASTNode *access = ast_create_member_access(make_identifier("v", 2), "code");
        ClassField *field = calloc(1, sizeof(ClassField));
        Symbol *vsym;

        vault->origin_path = pergyra_strdup("vault_module.pgy");
        field->name = pergyra_strdup("code");
        field->type = ast_create_type("Int");
        field->access = ACCESS_PUBLIC;
        field->has_explicit_access = true;
        vault->data.class_decl.fields = calloc(1, sizeof(ClassField *));
        vault->data.class_decl.fields[0] = field;
        vault->data.class_decl.field_count = 1;
        ast_add_statement(program, vault);
        ctx->program_root = program;

        type_check_class_decl(vault, ctx);
        ctx->current_module_path = "consumer_module.pgy";
        vsym = symbol_create_variable("v",
            scope_lookup(ctx->scope, "Vault")->type, 2, 1);
        scope_declare(ctx->scope, vsym);

        EXPECT(type_check_expression(access, ctx) == TYPE_INT);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(access);
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

    TEST("subject action validates requires within causes and authorized by clauses");
    {
        const char *source =
            "vessel CombatStats { power: Int; }\n"
            "subject Player {\n"
            "    vessel combat: CombatStats;\n"
            "    action Attack(self, amount: Int) -> Void\n"
            "        requires Combatable\n"
            "        within BattleZone\n"
            "        causes DamageEffect\n"
            "        authorized by self {\n"
            "        Log(1);\n"
            "    }\n"
            "}\n"
            "ability Combatable { func Strike() -> Void; }\n"
            "role Warrior for Player {\n"
            "    impl ability Combatable { func Strike() -> Void { Log(1); } }\n"
            "}\n"
            "effect DamageEffect for target: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    effect slot damage: DamageEffect\n"
            "    authority player requires Combatable\n"
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

    TEST("subject action authorized by requires subject-host parameters and matching zone authority");
    {
        const char *source =
            "subject Player {\n"
            "    action Trade(self, amount: Int) -> Void\n"
            "        within TradeZone\n"
            "        authorized by amount {\n"
            "        Log(1);\n"
            "    }\n"
            "}\n"
            "zone TradeZone {\n"
            "    subject slot buyer: Player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "must be a subject host"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("subject action causes effect and within zone enforce target compatibility");
    {
        const char *source =
            "subject Player {\n"
            "    action Attack(self) -> Void\n"
            "        within BattleZone\n"
            "        causes DamageEffect\n"
            "        authorized by self {\n"
            "        Log(1);\n"
            "    }\n"
            "}\n"
            "effect DamageEffect for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    authority player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count >= 1);
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "has no matching effect slot"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("secure subject action within zone requires explicit authorized by");
    {
        const char *source =
            "subject Player {\n"
            "    action TouchVault(self) -> Void\n"
            "        within BattleZone {\n"
            "        let stash: SecureSlot<Player> = self;\n"
            "    }\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    authority player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "must declare 'authorized by'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("secure subject action within zone passes with explicit authorized by");
    {
        const char *source =
            "subject Player {\n"
            "    action TouchVault(self) -> Void\n"
            "        within BattleZone\n"
            "        authorized by self {\n"
            "        let stash: SecureSlot<Player> = self;\n"
            "    }\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    authority player\n"
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

    TEST("subject declarations allow both func and action");
    {
        const char *source =
            "subject Player {\n"
            "    let hp: Int;\n"
            "    func CalculatePower(self) -> Int {\n"
            "        return hp * 2;\n"
            "    }\n"
            "    action Tick(self) -> Void {\n"
            "        Log(1);\n"
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

    TEST("ToObject rejects non-object targets and non-subject sources");
    {
        const char *source =
            "object PlayerView { hp: Int; }\n"
            "tobject PlayerDto { hp: Int; }\n"
            "subject Player { let hp: Int; }\n"
            "class PassivePlayer { let hp: Int; }\n"
            "func Main() -> Void {\n"
            "    let player: Player = Player();\n"
            "    let passive: PassivePlayer = PassivePlayer();\n"
            "    let wrongDto = ToObject(PlayerDto, player);\n"
            "    let wrongSubject = ToObject(Player, player);\n"
            "    let legacy = ToObject(PlayerView, passive);\n"
            "    let anon = ToObject(PlayerView, Player());\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 4);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("HasState works inside zone methods for declared states");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "effect Poisoned for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    relation slot trust: TrustedLink\n"
            "    effect slot poison: Poisoned\n"
            "    state poisoned: effect poison on player\n"
            "    state allied: relation trust between player, enemy\n"
            "    func Tick() -> Void {\n"
            "        if HasState(poisoned) || HasState(\"allied\") || HasState(poisoned, player) || HasState(allied, player, enemy) {\n"
            "            Log(1);\n"
            "        }\n"
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

    TEST("HasState validates state slot arity and endpoint matching");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    effect slot poison: Poisoned\n"
            "    relation slot trust: TrustedLink\n"
            "    state poisoned: effect poison on player\n"
            "    state allied: relation trust between player, enemy\n"
            "    func Tick() -> Void {\n"
            "        let a = HasState(poisoned, enemy);\n"
            "        let b = HasState(allied, player);\n"
            "        let c = HasState(allied, enemy, player);\n"
            "        Log(a || b || c);\n"
            "    }\n"
            "}\n"
            "effect Poisoned for bearer: Player { }\n"
            "relation TrustedLink for source: Player, target: Player { }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 3);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("HasState rejects unknown states and use outside zone");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    state poisoned: effect poison on player\n"
            "    effect slot poison: Poisoned\n"
            "    func Tick() -> Void {\n"
            "        if HasState(missing) {\n"
            "            Log(1);\n"
            "        }\n"
            "    }\n"
            "}\n"
            "effect Poisoned for bearer: Player { }\n"
            "func Main() -> Void {\n"
            "    let active = HasState(\"poisoned\");\n"
            "    Log(active);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("HasLayer works inside zone methods for declared relation and effect slots");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "effect Poisoned for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    relation slot trust: TrustedLink\n"
            "    effect slot poison: Poisoned\n"
            "    func Tick() -> Void {\n"
            "        if HasLayer(trust) || HasLayer(\"poison\") {\n"
            "            Log(1);\n"
            "        }\n"
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

    TEST("HasLayer rejects unknown names and use outside zone");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "effect Poisoned for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    effect slot poison: Poisoned\n"
            "    func Tick() -> Void {\n"
            "        if HasLayer(missing) {\n"
            "            Log(1);\n"
            "        }\n"
            "    }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let active = HasLayer(\"poison\");\n"
            "    Log(active);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("effect pool syntax is accepted for positive capacities");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "effect DamageEffect for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    effect pool damage: DamageEffect capacity 8\n"
            "    apply damage to player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count == 1);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "no explicit authority set"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("effect pool rejects non-positive capacities");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "effect DamageEffect for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    effect pool damage: DamageEffect capacity 0\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 1);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("HasProjection works inside relation/effect/zone methods for declared object and tobject slots");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "object PlayerView { hp: Int; }\n"
            "tobject PlayerDto { hp: Int; name: String; }\n"
            "relation TrustedLink for source: Player, target: Player {\n"
            "    subject slot left: Player\n"
            "    subject slot right: Player\n"
            "    object slot playerView: PlayerView\n"
            "    refresh playerView from left\n"
            "    func Inspect() -> Void {\n"
            "        if HasProjection(playerView) {\n"
            "            Log(1);\n"
            "        }\n"
            "    }\n"
            "}\n"
            "effect Poisoned for bearer: Player {\n"
            "    subject slot player: Player\n"
            "    tobject slot snapshot: PlayerDto\n"
            "    publish snapshot from player\n"
            "    func Tick() -> Void {\n"
            "        if HasProjection(\"snapshot\") {\n"
            "            Log(1);\n"
            "        }\n"
            "    }\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    tobject slot snapshot: PlayerDto\n"
            "    refresh playerView from player\n"
            "    publish snapshot from player\n"
            "    func Update() -> Void {\n"
            "        if HasProjection(playerView) || HasProjection(\"snapshot\") {\n"
            "            Log(1);\n"
            "        }\n"
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

    TEST("HasProjection rejects subject slots, unknown names, and use outside domain context");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "object PlayerView { hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot snapshot: PlayerView\n"
            "    func Show() -> Void {\n"
            "        let a = HasProjection(player);\n"
            "        let b = HasProjection(missing);\n"
            "        Log(a || b);\n"
            "    }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let active = HasProjection(\"snapshot\");\n"
            "    Log(active);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 3);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("HasZone works inside world methods for declared zone states");
    {
        const char *source =
            "zone BattleZone { }\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    state liveBattle: zone battle\n"
            "    activate liveBattle\n"
            "    maintain battle\n"
            "    func Tick() -> Void {\n"
            "        if HasZone(liveBattle) || HasZone(\"battle\") {\n"
            "            Log(1);\n"
            "        }\n"
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

    TEST("HasZone rejects unknown names and use outside world");
    {
        const char *source =
            "zone BattleZone { }\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    func Tick() -> Void {\n"
            "        let active = HasZone(missing);\n"
            "        Log(active);\n"
            "    }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let active = HasZone(\"battle\");\n"
            "    Log(active);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("HasZoneProjection/HasZoneLayer/HasZoneState work inside world methods");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "object PlayerView { hp: Int; }\n"
            "effect Poisoned for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    effect slot poison: Poisoned\n"
            "    state poisoned: effect poison on player\n"
            "    refresh playerView from player\n"
            "    maintain poisoned\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    func Tick() -> Void {\n"
            "        if HasZoneProjection(battle, playerView) || HasZoneLayer(battle, poison) || HasZoneState(battle, poisoned) {\n"
            "            Log(1);\n"
            "        }\n"
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

    TEST("HasZoneProjection/HasZoneLayer/HasZoneState reject unknown names and use outside world");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "object PlayerView { hp: Int; }\n"
            "effect Poisoned for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    effect slot poison: Poisoned\n"
            "    state poisoned: effect poison on player\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    func Tick() -> Void {\n"
            "        let a = HasZoneProjection(battle, missing);\n"
            "        let b = HasZoneLayer(battle, missing);\n"
            "        let c = HasZoneState(battle, missing);\n"
            "        Log(a); Log(b); Log(c);\n"
            "    }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let a = HasZoneProjection(\"battle\", \"playerView\");\n"
            "    let b = HasZoneLayer(\"battle\", \"poison\");\n"
            "    let c = HasZoneState(\"battle\", \"poisoned\");\n"
            "    Log(a); Log(b); Log(c);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 6);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("world derived states validate zone projection/layer/state contracts");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "object PlayerView { hp: Int; }\n"
            "effect Poisoned for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    effect slot poison: Poisoned\n"
            "    state poisoned: effect poison on player\n"
            "    refresh playerView from player\n"
            "    maintain poisoned\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    state battleProjected: zone battle projection playerView\n"
            "    state battleLayered: zone battle layer poison\n"
            "    state battlePoisoned: zone battle state poisoned\n"
            "    activate battle\n"
            "    func Tick() -> Void {\n"
            "        if HasZone(battleProjected) || HasZone(battleLayered) || HasZone(battlePoisoned) {\n"
            "            Log(1);\n"
            "        }\n"
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

    TEST("world derived states reject unknown details and lifecycle actions");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "object PlayerView { hp: Int; }\n"
            "effect Poisoned for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    effect slot poison: Poisoned\n"
            "    state poisoned: effect poison on player\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    state badProjection: zone battle projection missing\n"
            "    state badLayer: zone battle layer missing\n"
            "    state badState: zone battle state missing\n"
            "    state derivedPoisoned: zone battle state poisoned\n"
            "    activate derivedPoisoned\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 4);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("world composed states validate ordered zone/state composition");
    {
        const char *source =
            "zone BattleZone { }\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    zone camp: BattleZone\n"
            "    state battleLive: zone battle\n"
            "    state campLive: zone camp\n"
            "    state allLive: all battleLive, campLive\n"
            "    state anyLive: any allLive, campLive\n"
            "    func Tick() -> Void {\n"
            "        if HasZone(allLive) || HasZone(anyLive) {\n"
            "            Log(1);\n"
            "        }\n"
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

    TEST("world composed states reject self-reference and forward references");
    {
        const char *source =
            "zone BattleZone { }\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    state looped: any looped, battle\n"
            "    state later: any definedLater, battle\n"
            "    state definedLater: zone battle\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("world composed states warn on duplicate and redundant plain zone inputs");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    state battleLive: zone battle\n"
            "    state noisy: all battle, battleLive, battle\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count >= 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("world lifecycle warns on duplicate and conflicting zone directions");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    state liveBattle: zone battle\n"
            "    activate battle\n"
            "    activate liveBattle\n"
            "    deactivate battle\n"
            "    deactivate liveBattle\n"
            "    maintain battle\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count >= 6);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("world composed states warn when raw zone slots are used as derived inputs");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    zone camp: BattleZone\n"
            "    state campLive: zone camp\n"
            "    state mixed: any battle, campLive\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count >= 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("Zones embedded into a world cannot be mutated through the old binding");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    shared hp: Int = 0\n"
            "    func Hurt() -> Void { hp = hp + 1; }\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let player = Player(10);\n"
            "    let battle = BattleZone(player);\n"
            "    let world = GameWorld(battle);\n"
            "    battle.hp = 3;\n"
            "    battle.Hurt();\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    /* ---- Generic class semantic tests ---- */

    TEST("generic class declaration passes semantic check");
    {
        const char *source =
            "class Box<T> {\n"
            "    let value: T;\n"
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

    TEST("generic class with method passes semantic check");
    {
        const char *source =
            "class Wrapper<T> {\n"
            "    let data: T;\n"
            "    func GetData(self) -> T {\n"
            "        return self.data;\n"
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

    TEST("generic class with concrete usage passes semantic check");
    {
        const char *source =
            "class Pair<T> {\n"
            "    let first: T;\n"
            "    let second: T;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let p: Pair<Int> = Pair(3, 7);\n"
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

    TEST("generic function where clause accepts matching exact bound");
    {
        const char *source =
            "func OnlyInt<T>(value: T) -> T where T: Int {\n"
            "    return value;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let x: Int = OnlyInt(3);\n"
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

    TEST("generic function where clause rejects non-matching exact bound");
    {
        const char *source =
            "func OnlyInt<T>(value: T) -> T where T: Int {\n"
            "    return value;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let y = OnlyInt(\"nope\");\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "does not satisfy constraint 'Int'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("generic class where clause rejects unknown bound type");
    {
        const char *source =
            "class Box<T> where T: MissingConstraint {\n"
            "    let value: T;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Unknown constraint type 'MissingConstraint' in where clause"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("role where clause rejects unknown bound type");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "role Support<T> for T where T: MissingAbility {\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Unknown constraint type 'MissingAbility' in where clause"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("generic function where clause accepts ability-style role constraint");
    {
        const char *source =
            "ability Comparable { }\n"
            "subject Player { let hp: Int; }\n"
            "role PlayerComparable for Player {\n"
            "    impl ability Comparable { }\n"
            "}\n"
            "func Keep<T>(value: T) -> T where T: Comparable {\n"
            "    return value;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let p: Player = Player(3);\n"
            "    let q: Player = Keep(p);\n"
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

    TEST("generic function where clause rejects missing ability-style role constraint");
    {
        const char *source =
            "ability Comparable { }\n"
            "subject Merchant { let trust: Int; }\n"
            "func Keep<T>(value: T) -> T where T: Comparable {\n"
            "    return value;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let m: Merchant = Merchant(1);\n"
            "    let n: Merchant = Keep(m);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "does not satisfy constraint 'Comparable'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("generic class instantiation enforces exact where constraint");
    {
        const char *source =
            "class Box<T> where T: Int {\n"
            "    let value: T;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let ok: Box<Int> = Box(3);\n"
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

    TEST("generic class instantiation rejects non matching where constraint");
    {
        const char *source =
            "class Box<T> where T: Int {\n"
            "    let value: T;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let bad: Box<String> = Box(\"oops\");\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "does not satisfy constraint 'Int'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("generic class instantiation accepts ability style where constraint");
    {
        const char *source =
            "ability Comparable { }\n"
            "subject Player { let hp: Int; }\n"
            "role PlayerComparable for Player {\n"
            "    impl ability Comparable { }\n"
            "}\n"
            "class Crate<T> where T: Comparable {\n"
            "    let value: T;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let c: Crate<Player> = Crate(Player(1));\n"
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

    TEST("generic class instantiation rejects missing ability style where constraint");
    {
        const char *source =
            "ability Comparable { }\n"
            "subject Merchant { let trust: Int; }\n"
            "class Crate<T> where T: Comparable {\n"
            "    let value: T;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let c: Crate<Merchant> = Crate(Merchant(1));\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "does not satisfy constraint 'Comparable'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("innate ability implementation is allowed inside declaring module");
    {
        const char *source =
            "innate ability Combatable {\n"
            "    func Attack() -> Int;\n"
            "}\n"
            "subject Fighter { let hp: Int; }\n"
            "role Warrior for Fighter {\n"
            "    impl ability Combatable {\n"
            "        func Attack() -> Int { return 10; }\n"
            "    }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let fighter: Fighter = Fighter(1);\n"
            "    Log(fighter.hp);\n"
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

    TEST("innate ability rejects implementation outside declaring module");
    {
        const char *ability_path = "test_innate_ability_module_a.pgy";
        const char *main_path = "test_innate_ability_module_b.pgy";
        const char *ability_source =
            "export innate ability Combatable {\n"
            "    func Attack() -> Int;\n"
            "}\n";
        const char *main_source =
            "import \"test_innate_ability_module_a.pgy\";\n"
            "subject Hacker { let hp: Int; }\n"
            "role Rogue for Hacker {\n"
            "    impl ability Combatable {\n"
            "        func Attack() -> Int { return 7; }\n"
            "    }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let h: Hacker = Hacker(1);\n"
            "    Log(h.hp);\n"
            "}\n";
        FILE *ability_file = fopen(ability_path, "wb");
        FILE *main_file = fopen(main_path, "wb");
        char *error_message = NULL;
        ASTNode *program = NULL;
        SemanticResult *result = NULL;

        EXPECT(ability_file != NULL && main_file != NULL);
        if (ability_file != NULL) {
            fputs(ability_source, ability_file);
            fclose(ability_file);
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
        EXPECT(result != NULL && ctx_has_diagnostic_substring_from_result(result,
            "innate ability 'Combatable' cannot be implemented outside its declaring module"));

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(ability_path);
    }

    TEST("explicit export hides non-exported imported declarations");
    {
        const char *module_path = "test_export_visibility_module_a.pgy";
        const char *main_path = "test_export_visibility_module_main_a.pgy";
        const char *module_source =
            "func Hidden() -> Int { return 1; }\n"
            "export func Visible() -> Int { return 2; }\n";
        const char *main_source =
            "import \"test_export_visibility_module_a.pgy\";\n"
            "func Main() -> Void {\n"
            "    Log(Visible());\n"
            "    Log(Hidden());\n"
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
        EXPECT(result != NULL && ctx_has_diagnostic_substring_from_result(result,
            "Undefined function 'Hidden'"));

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(module_path);
    }

    TEST("explicit export keeps exported imported declarations visible");
    {
        const char *module_path = "test_export_visibility_module_b.pgy";
        const char *main_path = "test_export_visibility_module_main_b.pgy";
        const char *module_source =
            "func Hidden() -> Int { return 1; }\n"
            "export func Visible() -> Int { return 2; }\n";
        const char *main_source =
            "import \"test_export_visibility_module_b.pgy\";\n"
            "func Main() -> Void {\n"
            "    Log(Visible());\n"
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

    TEST("explicit export hides non-exported imported nominal constructors");
    {
        const char *source =
            "class HiddenBox {\n"
            "    let value: Int;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        ASTNode *box = NULL;
        ASTNode *call = NULL;
        Type *resolved = NULL;

        EXPECT(!parser_has_error(parser));
        EXPECT(ctx != NULL && program != NULL);
        box = program != NULL ? program->data.program.statements[0] : NULL;
        EXPECT(box != NULL);
        box->origin_path = pergyra_strdup("hidden_box_module.pgy");
        ctx->program_root = program;

        type_check_class_decl(box, ctx);
        ctx->current_module_path = "consumer_module.pgy";

        call = make_call("HiddenBox", (ASTNode *[]){ make_number(1, 1) }, 1, 1);
        call->origin_path = pergyra_strdup("consumer_module.pgy");
        resolved = type_check_expression(call, ctx);

        EXPECT(resolved == TYPE_UNKNOWN);
        EXPECT(ctx->has_error);
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "Constructor 'HiddenBox' is not accessible across the current visibility boundary"));

        ast_destroy(call);
        ast_destroy(program);
        semantic_context_destroy(ctx);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("explicit export keeps exported imported nominal constructors visible");
    {
        const char *module_path = "test_export_visibility_module_d.pgy";
        const char *main_path = "test_export_visibility_module_main_d.pgy";
        const char *module_source =
            "export class PublicBox {\n"
            "    let value: Int;\n"
            "}\n";
        const char *main_source =
            "import \"test_export_visibility_module_d.pgy\";\n"
            "func Main() -> Void {\n"
            "    let h: PublicBox = PublicBox(1);\n"
            "    Log(h.value);\n"
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

    TEST("unknown stdlib use is rejected");
    {
        const char *source =
            "use missingmod;\n"
            "func Main() -> Void { return; }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(result != NULL && ctx_has_diagnostic_substring_from_result(
            result, "Unknown stdlib use 'missingmod'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("known stdlib use remains accepted");
    {
        const char *source =
            "use datetime;\n"
            "func Main() -> Void { Log(Now()); }\n";
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

    TEST("intent declaration binds subjects and validates step contracts");
    {
        const char *source =
            "subject Player {\n"
            "    let hp: Int;\n"
            "    action pay(self) -> Void { return; }\n"
            "}\n"
            "subject Merchant {\n"
            "    let trust: Int;\n"
            "}\n"
            "ability Payable { func Pay() -> Void; }\n"
            "role BuyerRole for Player {\n"
            "    impl ability Payable {\n"
            "        func Pay() -> Void { return; }\n"
            "    }\n"
            "}\n"
            "effect PaymentEffect for bearer: Player { }\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Player\n"
            "}\n"
            "intent Purchase {\n"
            "    involves buyer: Player;\n"
            "    involves seller: Merchant;\n"
            "    step pay {\n"
            "        where: PaymentZone;\n"
            "        who: buyer;\n"
            "        requires: Payable;\n"
            "        authorized by: buyer;\n"
            "        causes: PaymentEffect;\n"
            "        expect: buyer.hp >= 0;\n"
            "    }\n"
            "    success: buyer.hp >= 0;\n"
            "    failure: false;\n"
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

    TEST("intent declaration rejects unknown who participant");
    {
        const char *source =
            "subject Player {\n"
            "    let hp: Int;\n"
            "    action pay(self) -> Void { return; }\n"
            "}\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Player\n"
            "}\n"
            "intent Purchase {\n"
            "    involves buyer: Player;\n"
            "    step pay {\n"
            "        where: PaymentZone;\n"
            "        who: ghost;\n"
            "        expect: true;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "unknown participant 'ghost'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent declaration enforces zone slots and subject abilities");
    {
        const char *source =
            "subject Player {\n"
            "    let hp: Int;\n"
            "    action Fight(self) -> Void { return; }\n"
            "}\n"
            "subject Merchant {\n"
            "    let trust: Int;\n"
            "}\n"
            "ability Striking { func StrikeMark() -> String; }\n"
            "zone BattleZone {\n"
            "    subject slot buyer: Merchant\n"
            "    authority buyer requires Striking\n"
            "}\n"
            "intent Clash {\n"
            "    involves hero: Player;\n"
            "    step Fight {\n"
            "        where: BattleZone;\n"
            "        who: hero;\n"
            "        requires: Striking;\n"
            "        authorized by: hero;\n"
            "        expect: true;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "has no matching subject slot"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "does not implement it"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent declaration supports params and bool step contracts");
    {
        const char *source =
            "subject Driver {\n"
            "    let started: Bool;\n"
            "    action Ignite(self) -> Void { self.started = true; }\n"
            "}\n"
            "zone CockpitZone {\n"
            "    subject slot driver: Driver\n"
            "}\n"
            "intent DriveCar(cockpit: CockpitZone, driver: Driver) {\n"
            "    exclusive;\n"
            "    priority: 3;\n"
            "    step Ignite {\n"
            "        where: CockpitZone;\n"
            "        using: cockpit;\n"
            "        who: driver;\n"
            "        on: driver.Ignite();\n"
            "        on: driver.Ignite();\n"
            "        pre: true;\n"
            "        guard: driver.started;\n"
            "        post: driver.started;\n"
            "        invariant: driver.started;\n"
            "        expect: driver.started;\n"
            "    }\n"
            "    success: driver.started;\n"
            "    failure: false;\n"
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

    TEST("intent authority-bearing zone rejects secure helper call without authorized by");
    {
        const char *source =
            "/// @effects secure\n"
            "func Gate() -> Bool { return true; }\n"
            "subject Driver {\n"
            "    let started: Bool;\n"
            "    action Ignite(self) -> Void { self.started = true; }\n"
            "}\n"
            "zone CockpitZone {\n"
            "    subject slot driver: Driver\n"
            "    authority driver\n"
            "}\n"
            "intent DriveCar(cockpit: CockpitZone, driver: Driver) {\n"
            "    step Ignite {\n"
            "        where: CockpitZone;\n"
            "        using: cockpit;\n"
            "        who: driver;\n"
            "        on: Gate();\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "invokes authority-sensitive helpers"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent authority-bearing zone rejects zone-action helper call without authorized by");
    {
        const char *source =
            "subject Driver {\n"
            "    let started: Bool;\n"
            "    action Ignite(self) -> Void within CockpitZone causes Started {\n"
            "        self.started = true;\n"
            "    }\n"
            "}\n"
            "effect Started for bearer: Driver {\n"
            "    subject slot bearer: Driver\n"
            "}\n"
            "zone CockpitZone {\n"
            "    subject slot driver: Driver\n"
            "    effect slot started: Started\n"
            "    authority driver\n"
            "}\n"
            "intent DriveCar(cockpit: CockpitZone, driver: Driver) {\n"
            "    step Ignite {\n"
            "        where: CockpitZone;\n"
            "        using: cockpit;\n"
            "        who: driver;\n"
            "        on: driver.Ignite();\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "invokes authority-sensitive helpers"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent declaration rejects non-bool guard and invariant");
    {
        const char *source =
            "subject Driver {\n"
            "    let hp: Int;\n"
            "    action Ignite(self) -> Void { hp = hp + 1; }\n"
            "}\n"
            "zone CockpitZone {\n"
            "    subject slot driver: Driver\n"
            "}\n"
            "intent DriveCar(driver: Driver) {\n"
            "    step Ignite {\n"
            "        where: CockpitZone;\n"
            "        who: driver;\n"
            "        on: driver.Ignite();\n"
            "        guard: driver.hp;\n"
            "        invariant: driver.hp;\n"
            "        expect: true;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Intent guard expects a Bool value"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Intent invariant expects a Bool value"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent declaration supports compensate and trace builtins");
    {
        const char *source =
            "subject Driver {\n"
            "    let started: Bool;\n"
            "    action Ignite(self) -> Void { self.started = true; }\n"
            "    action RollbackIgnite(self) -> Void { self.started = false; }\n"
            "}\n"
            "zone CockpitZone {\n"
            "    subject slot driver: Driver\n"
            "}\n"
            "intent DriveCar(driver: Driver) {\n"
            "    rollback: current;\n"
            "    step Ignite {\n"
            "        where: CockpitZone;\n"
            "        who: driver;\n"
            "        on: driver.Ignite();\n"
            "        compensate: driver.RollbackIgnite();\n"
            "        pre: true;\n"
            "        guard: false;\n"
            "        post: driver.started;\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    Log(IntentLastTrace());\n"
            "    Log(IntentLastFailure());\n"
            "    Log(IntentLastName());\n"
            "    Log(ToString(IntentLastHandle()));\n"
            "    Log(ToString(IntentLastTraceId()));\n"
            "    Log(ToString(IntentLastStepCount()));\n"
            "    Log(ToString(IntentLastFailed()));\n"
            "    Log(ToString(IntentHistoryCount()));\n"
            "    Log(IntentHistoryStepName(0));\n"
            "    Log(IntentHistoryStepZone(0));\n"
            "    Log(IntentHistoryStepPhase(0));\n"
            "    Log(IntentHistoryStepParticipant(0));\n"
            "    Log(IntentHistoryStepSlot(0));\n"
            "    Log(IntentHistoryStepFromZone(0));\n"
            "    Log(IntentHistoryStepFromSlot(0));\n"
            "    Log(IntentHistoryStepToZone(0));\n"
            "    Log(IntentHistoryStepToSlot(0));\n"
            "    Log(ToString(IntentHistoryStepOk(0)));\n"
            "    Log(IntentHistoryStepFailure(0));\n"
            "    Log(ToString(IntentActiveCount()));\n"
            "    Log(IntentActiveName(0));\n"
            "    Log(ToString(IntentActiveHandle(0)));\n"
            "    Log(ToString(IntentActiveTraceId(0)));\n"
            "    Log(ToString(IntentActivePriority(0)));\n"
            "    Log(ToString(IntentActiveConcurrent(0)));\n"
            "    Log(IntentActiveTrace(0));\n"
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

    TEST("intent declaration validates cross-world transfer bindings");
    {
        const char *source =
            "subject Buyer {\n"
            "    let hp: Int;\n"
            "    action Promote(self) -> Void { hp = hp + 1; }\n"
            "}\n"
            "zone CartZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "intent Checkout(cart: CartZone, payment: PaymentZone, buyer: Buyer) {\n"
            "    step Promote {\n"
            "        where: PaymentZone;\n"
            "        using: payment;\n"
            "        transfer: cart -> payment;\n"
            "        who: buyer;\n"
            "        on: buyer.Promote();\n"
            "        expect: true;\n"
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

    TEST("intent step may orchestrate a subintent without local where/who");
    {
        const char *source =
            "subject Buyer {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Touch() -> Void { return; }\n"
            "zone CheckoutZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "intent Charge(checkout: CheckoutZone, buyer: Buyer) {\n"
            "    step verify {\n"
            "        where: CheckoutZone;\n"
            "        using: checkout;\n"
            "        who: buyer;\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}\n"
            "intent Checkout(checkout: CheckoutZone, buyer: Buyer) {\n"
            "    step pay {\n"
            "        intent: Charge(checkout, buyer);\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
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

    TEST("intent declaration rejects spawn inside step clauses");
    {
        const char *source =
            "subject Buyer {\n"
            "    let hp: Int;\n"
            "}\n"
            "zone CheckoutZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "intent Checkout(checkout: CheckoutZone, buyer: Buyer) {\n"
            "    step pay {\n"
            "        where: CheckoutZone;\n"
            "        using: checkout;\n"
            "        who: buyer;\n"
            "        on: spawn Touch();\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "on clause cannot contain 'spawn'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("Now and Sleep builtins type-check");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let t0: Int = Now();\n"
            "    Sleep(1);\n"
            "    let t1: Int = Now();\n"
            "    Log(ToString(t1 - t0));\n"
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

    TEST("ReadLine builtin type-checks");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let line: String = ReadLine();\n"
            "    Print(line);\n"
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

    TEST("intent step intent clause must target another intent");
    {
        const char *source =
            "subject Buyer {\n"
            "    let hp: Int;\n"
            "    func Pay(self) -> Bool { return true; }\n"
            "}\n"
            "func Charge(buyer: Buyer) -> Bool { return true; }\n"
            "intent Checkout(buyer: Buyer) {\n"
            "    step pay {\n"
            "        intent: Charge(buyer);\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "is not a declared intent"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ability require cannot reference non-exported foreign type");
    {
        const char *source =
            "subject Hidden { let hp: Int; }\n"
            "ability NeedsHidden {\n"
            "    require h: Hidden\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result;

        EXPECT(!parser_has_error(parser));
        program->data.program.statements[0]->origin_path = pergyra_strdup("hidden_module.pgy");
        program->data.program.statements[1]->origin_path = pergyra_strdup("ability_module.pgy");

        result = semantic_analyze(program);
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot require field 'h' with non-exported type 'Hidden'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("role impl cannot reference non-exported foreign ability");
    {
        const char *module_path = "test_role_impl_hidden_ability_module_a.pgy";
        const char *main_path = "test_role_impl_hidden_ability_module_main_a.pgy";
        const char *module_source =
            "ability HiddenAbility {\n"
            "    func Ping() -> Void;\n"
            "}\n";
        const char *main_source =
            "import \"test_role_impl_hidden_ability_module_a.pgy\";\n"
            "subject Hero { let hp: Int; }\n"
            "role HeroRole for Hero {\n"
            "    impl ability HiddenAbility {\n"
            "        func Ping() -> Void { return; }\n"
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
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot implement non-exported ability 'HiddenAbility'"));

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(module_path);
    }

    TEST("action requires cannot reference non-exported foreign ability");
    {
        const char *module_path = "test_action_requires_hidden_ability_module_a.pgy";
        const char *main_path = "test_action_requires_hidden_ability_module_main_a.pgy";
        const char *module_source =
            "ability HiddenAbility {\n"
            "    func Ping() -> Void;\n"
            "}\n";
        const char *main_source =
            "import \"test_action_requires_hidden_ability_module_a.pgy\";\n"
            "subject Hero {\n"
            "    let hp: Int;\n"
            "    action Do(self) -> Void requires HiddenAbility {\n"
            "        return;\n"
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
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot require non-exported ability 'HiddenAbility'"));

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(module_path);
    }
}

/* -----------------------------------------------------------------
 * Main
 * ----------------------------------------------------------------- */
