static void
test_effect_inference(void)
{
    printf("\n[effect_inference]\n");

    TEST("Measure infers nondeterministic + collapse on function");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("Observe");
        func->data.func_decl.return_type = ast_create_type("Int");
        func->data.func_decl.body = ast_create_block();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 2);
        ast_add_statement(func->data.func_decl.body, decl);

        ASTNode *ret = ast_create_return_statement();
        ASTNode *measure_args[1] = { make_identifier("q", 3) };
        ret->data.return_stmt.value = make_call("Measure", measure_args, 1, 3);
        ast_add_statement(func->data.func_decl.body, ret);

        type_check_func_decl(func, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "Observe");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_NONDETERMINISTIC)
            && type_effect_mask_has(effects, EFFECT_COLLAPSE));

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("effectful calls propagate function effects to callers");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *observe = ast_create_function("Observe");
        observe->data.func_decl.return_type = ast_create_type("Int");
        observe->data.func_decl.body = ast_create_block();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 2);
        ast_add_statement(observe->data.func_decl.body, decl);

        ASTNode *measure_ret = ast_create_return_statement();
        ASTNode *measure_args[1] = { make_identifier("q", 3) };
        measure_ret->data.return_stmt.value =
            make_call("Measure", measure_args, 1, 3);
        ast_add_statement(observe->data.func_decl.body, measure_ret);
        type_check_func_decl(observe, ctx);

        ASTNode *wrapper = ast_create_function("WrapObserve");
        wrapper->data.func_decl.return_type = ast_create_type("Int");
        wrapper->data.func_decl.body = ast_create_block();

        ASTNode *ret = ast_create_return_statement();
        ret->data.return_stmt.value = make_call("Observe", NULL, 0, 6);
        ast_add_statement(wrapper->data.func_decl.body, ret);
        type_check_func_decl(wrapper, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "WrapObserve");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_NONDETERMINISTIC)
            && type_effect_mask_has(effects, EFFECT_COLLAPSE));

        semantic_context_destroy(ctx);
        ast_destroy(observe);
        ast_destroy(wrapper);
    }

    TEST("effect-partial-order: collapse is a superset of nondeterministic by closure");
    {
        EXPECT(type_effect_mask_compare(EFFECT_COLLAPSE, EFFECT_NONDETERMINISTIC)
            == EFFECT_REL_SUPERSET);
        EXPECT(type_effect_mask_compare(EFFECT_NONDETERMINISTIC, EFFECT_COLLAPSE)
            == EFFECT_REL_SUBSET);
    }

    TEST("effect-partial-order: secure and remote are incomparable");
    {
        EXPECT(type_effect_mask_compare(EFFECT_SECURE, EFFECT_REMOTE)
            == EFFECT_REL_INCOMPARABLE);
        EXPECT(type_effect_mask_compare(EFFECT_REMOTE, EFFECT_SECURE)
            == EFFECT_REL_INCOMPARABLE);
    }

    TEST("effect-partial-order: joined secure|remote is a superset of each side");
    {
        uint32_t joined = type_effect_mask_join(EFFECT_SECURE, EFFECT_REMOTE);
        EXPECT(type_effect_mask_compare(joined, EFFECT_SECURE)
            == EFFECT_REL_SUPERSET);
        EXPECT(type_effect_mask_compare(joined, EFFECT_REMOTE)
            == EFFECT_REL_SUPERSET);
        EXPECT(type_effect_mask_requires_authority(EFFECT_SECURE));
        EXPECT(type_effect_mask_touches_resource_boundary(EFFECT_SECURE));
        EXPECT(type_effect_mask_touches_resource_boundary(EFFECT_REMOTE));
    }

    TEST("SecureSlot declarations infer secure effect on function");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("TouchSecure");
        func->data.func_decl.return_type = ast_create_type("Int");
        func->data.func_decl.body = ast_create_block();

        ASTNode *decl = ast_create_let_declaration("secret");
        decl->data.let_decl.type = make_generic_type("SecureSlot", "Int");
        decl->data.let_decl.initializer = make_number(7, 2);
        ast_add_statement(func->data.func_decl.body, decl);

        ASTNode *ret = ast_create_return_statement();
        ret->data.return_stmt.value = make_number(1, 3);
        ast_add_statement(func->data.func_decl.body, ret);

        type_check_func_decl(func, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "TouchSecure");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_SECURE));

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("SecureSlot parameters infer secure effect on function signature");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *bot = ast_create_subject("Bot");
        ASTNode *func = ast_create_function("TouchSecureParam");
        func->data.func_decl.return_type = ast_create_type("Void");
        func->data.func_decl.body = ast_create_block();
        func->data.func_decl.param_count = 1;
        func->data.func_decl.params = calloc(1, sizeof(FuncParam *));
        func->data.func_decl.params[0] = make_func_param(
            "secret", make_generic_type("SecureSlot", "Bot"));
        func->data.func_decl.params[0]->mode = PARAM_MODE_OWN;

        type_check_class_decl(bot, ctx);
        type_check_func_decl(func, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "TouchSecureParam");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_SECURE));

        semantic_context_destroy(ctx);
        ast_destroy(bot);
        ast_destroy(func);
    }

    TEST("Token<T> parameter type resolves as capability type");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("TouchToken");
        func->data.func_decl.return_type = ast_create_type("Void");
        func->data.func_decl.body = ast_create_block();
        func->data.func_decl.param_count = 1;
        func->data.func_decl.params = calloc(1, sizeof(FuncParam *));
        func->data.func_decl.params[0] = make_func_param(
            "tok", make_generic_type("Token", "Int"));

        type_check_func_decl(func, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "TouchToken");
        Type *param_type = (sym != NULL && sym->type != NULL
            && sym->type->kind == TYPE_KIND_FUNCTION
            && sym->type->data.function.param_count == 1)
            ? sym->type->data.function.param_types[0]
            : TYPE_UNKNOWN;

        EXPECT(!ctx->has_error
            && param_type != NULL
            && param_type->kind == TYPE_KIND_CONSTRUCTED
            && type_equals(param_type->data.constructed.constructor, TYPE_TOKEN)
            && param_type->data.constructed.arg_count == 1
            && type_equals(param_type->data.constructed.args[0], TYPE_INT));

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("Token<T> parameters infer secure effect on function signature");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("TouchTokenSecure");
        func->data.func_decl.return_type = ast_create_type("Void");
        func->data.func_decl.body = ast_create_block();
        func->data.func_decl.param_count = 1;
        func->data.func_decl.params = calloc(1, sizeof(FuncParam *));
        func->data.func_decl.params[0] = make_func_param(
            "tok", make_generic_type("Token", "Int"));

        type_check_func_decl(func, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "TouchTokenSecure");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_SECURE));

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("intent step effect in authority-bearing zone requires authorized by");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "effect Marked for bearer: Player {\n"
            "    subject slot bearer: Player\n"
            "}\n"
            "zone Arena {\n"
            "    subject slot player: Player\n"
            "    effect slot marked: Marked\n"
            "    authority player\n"
            "}\n"
            "intent Charge {\n"
            "    involves player: Player;\n"
            "    involves arena: Arena;\n"
            "    step Verify {\n"
            "        where: Arena;\n"
            "        using: arena;\n"
            "        who: player;\n"
            "        causes: Marked;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "must declare 'authorized by'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("SecureSlot builtins require named paired token identifiers");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let s: SecureSlot<Int> = 7;\n"
            "    Write(s, 1, 0);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "requires a named paired token identifier"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("Capability token types participate in static function-call checking");
    {
        const char *source =
            "subject Bot {\n"
            "    let hp: Int;\n"
            "}\n"
            "func NeedIntToken(tok: Token<Int>) -> Void {\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: SecureSlot<Bot> = Bot(1);\n"
            "    NeedIntToken(s_token);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot assign 'Token<Bot>' to 'Token<Int>'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("DeviceSlot parameter types are rejected as anchored handles");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("UseDevice");
        func->data.func_decl.return_type = ast_create_type("Void");
        func->data.func_decl.body = ast_create_block();
        func->data.func_decl.param_count = 1;
        func->data.func_decl.params = calloc(1, sizeof(FuncParam *));
        func->data.func_decl.params[0] =
            make_func_param("dev", make_generic_type("DeviceSlot", "Int"));

        type_check_func_decl(func, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "Anchored resource handle parameters"));

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("DeviceSlot copy into new binding is rejected");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *dev_args[1] = { TYPE_INT };
        Type *device_type = type_create_constructed(TYPE_DEVICE_SLOT, dev_args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("dev", device_type, 1, 1));

        ASTNode *decl = ast_create_let_declaration("copy");
        decl->data.let_decl.type = make_generic_type("DeviceSlot", "Int");
        decl->data.let_decl.initializer = make_identifier("dev", 2);

        type_check_let_decl(decl, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "Anchored resource handles"));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
    }

    TEST("DeviceSlot use-after-release is rejected");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *decl = ast_create_let_declaration("dev");
        decl->data.let_decl.type = make_generic_type("DeviceSlot", "Int");
        decl->data.let_decl.initializer = make_call("ClaimDeviceSlot", NULL, 0, 1);
        EXPECT(type_check_let_decl(decl, ctx));

        ASTNode *release_args[1] = { make_identifier("dev", 2) };
        ASTNode *read_args[1] = { make_identifier("dev", 3) };
        ASTNode *release = make_call("ReleaseDeviceSlot", release_args, 1, 2);
        ASTNode *read = make_call("DeviceRead", read_args, 1, 3);

        type_check_expression(release, ctx);
        ctx->has_error = false;
        type_check_expression(read, ctx);

        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "released DeviceSlot"));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(release);
        ast_destroy(read);
    }

    TEST("DeviceSlot double release is rejected");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *decl = ast_create_let_declaration("dev");
        decl->data.let_decl.type = make_generic_type("DeviceSlot", "Int");
        decl->data.let_decl.initializer = make_call("ClaimDeviceSlot", NULL, 0, 1);
        EXPECT(type_check_let_decl(decl, ctx));

        ASTNode *release_a_args[1] = { make_identifier("dev", 2) };
        ASTNode *release_b_args[1] = { make_identifier("dev", 3) };
        ASTNode *release_a = make_call("ReleaseDeviceSlot", release_a_args, 1, 2);
        ASTNode *release_b = make_call("ReleaseDeviceSlot", release_b_args, 1, 3);

        type_check_expression(release_a, ctx);
        ctx->has_error = false;
        type_check_expression(release_b, ctx);

        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "already been released"));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(release_a);
        ast_destroy(release_b);
    }

    TEST("subject copy into new binding is rejected");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let a: Vec2 = Vec2();\n"
            "    let b: Vec2 = a;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Subjects cannot be copied into a new binding"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("struct value copy into new binding is allowed");
    {
        const char *source =
            "struct Vec2 {\n"
            "    x: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let a: Vec2 = Vec2();\n"
            "    let b: Vec2 = a;\n"
            "    b.x = 1;\n"
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

    TEST("class value copy into new binding is allowed");
    {
        const char *source =
            "class Vec2 {\n"
            "    let x: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let a: Vec2 = Vec2();\n"
            "    let b: Vec2 = a;\n"
            "    b.x = 1;\n"
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

    TEST("subject parameter is accepted (reference semantics)");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "}\n"
            "func Use(v: Vec2) -> Void {\n"
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

    TEST("class return by value is allowed");
    {
        const char *source =
            "class Vec2 {\n"
            "    let x: Int;\n"
            "}\n"
            "func Make() -> Vec2 {\n"
            "    return Vec2();\n"
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

    TEST("subject return by value is rejected");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "}\n"
            "func Make() -> Vec2 {\n"
            "    return Vec2();\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Returning subjects by value is not supported yet"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("subject copy into new binding is rejected");
    {
        const char *source =
            "subject Counter {\n"
            "    let count: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let a: Counter = Counter();\n"
            "    let b: Counter = a;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Subjects cannot be copied into a new binding"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("subject declaration emits no deprecation warning");
    {
        const char *source =
            "subject Counter {\n"
            "    let count: Int;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("subject syntax is rejected by subject copy rules");
    {
        const char *source =
            "subject Counter {\n"
            "    let count: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let a: Counter = Counter();\n"
            "    let b: Counter = a;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Subjects cannot be copied into a new binding"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("class method bare field names resolve in class scope");
    {
        const char *source =
            "class Counter {\n"
            "    let count: Int;\n"
            "    func Tick(self, delta: Int) -> Int {\n"
            "        count = count + delta;\n"
            "        return count;\n"
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


    TEST("nested vessel-backed projection resolves through subject fields");
    {
        const char *source =
            "vessel Cycle { age: Int; fatigue: Int; }\n"
            "vessel Traits { metabolism: Int; }\n"
            "subject Creature {\n"
            "    let name: String;\n"
            "    vessel cycle: Cycle;\n"
            "    vessel traits: Traits;\n"
            "}\n"
            "object CreatureView { age: Int; fatigue: Int; metabolism: Int; }\n"
            "tobject CreaturePacket { age: Int; metabolism: Int; }\n"
            "zone MeadowZone {\n"
            "    subject slot grazer: Creature\n"
            "    object slot grazerView: CreatureView = ToObject(CreatureView, grazer)\n"
            "    tobject slot grazerPacket: CreaturePacket = ToTObject(CreaturePacket, grazer)\n"
            "    refresh grazerView from grazer\n"
            "    publish grazerPacket from grazer\n"
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

    TEST("zone methods resolve bare shared fields and helper funcs in zone scope");
    {
        const char *source =
            "zone BattleZone {\n"
            "    shared round: Int = 1\n"
            "    func Next(self) -> Int {\n"
            "        round = round + 1;\n"
            "        return round;\n"
            "    }\n"
            "    func Tick(self) -> Int {\n"
            "        return Next() + round;\n"
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

    TEST("world methods resolve bare shared fields zone slots and helper funcs in world scope");
    {
        const char *source =
            "zone BattleZone {\n"
            "    shared round: Int = 1\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    shared storm: Int = 1\n"
            "    func Pulse(self) -> Int {\n"
            "        storm = storm + 1;\n"
            "        return storm + battle.round;\n"
            "    }\n"
            "    func Tick(self) -> Int {\n"
            "        return Pulse() + storm;\n"
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

    TEST("class constructor positional arguments are type-checked");
    {
        const char *source =
            "class Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let v: Vec2 = Vec2(3, \"bad\");\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "argument 2 initializes field 'y'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("class constructor rejects too many positional arguments");
    {
        const char *source =
            "class Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let v: Vec2 = Vec2(1, 2, 3);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "accepts at most 2 positional field argument"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("Slot<subject> is accepted as a local object-cell anchor");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: Slot<Vec2> = Vec2(3, 7);\n"
            "    Write(s, Vec2(1, 2));\n"
            "    Release(s);\n"
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

    TEST("Slot<subject> is accepted as a local object-cell anchor");
    {
        const char *source =
            "subject Bot {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: Slot<Bot> = Bot(7);\n"
            "    Write(s, Bot(9));\n"
            "    Release(s);\n"
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

    TEST("SecureSlot<subject> is accepted as a secure object-cell anchor");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: SecureSlot<Vec2> = Vec2(3, 7);\n"
            "    Write(s, Vec2(1, 2), s_token);\n"
            "    Release(s, s_token);\n"
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

    TEST("SecureSlot<subject> is accepted as a secure object-cell anchor");
    {
        const char *source =
            "subject Bot {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: SecureSlot<Bot> = Bot(7);\n"
            "    Write(s, Bot(9), s_token);\n"
            "    Release(s, s_token);\n"
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

    TEST("spawn and channel send infer remote effect on function");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("Dispatch");
        func->data.func_decl.return_type = ast_create_type("Int");
        func->data.func_decl.body = ast_create_block();
        func->data.func_decl.param_count = 1;
        func->data.func_decl.params = calloc(1, sizeof(FuncParam *));
        func->data.func_decl.params[0] =
            make_func_param("ch", make_generic_type("Channel", "Int"));

        ASTNode *pending = ast_create_let_declaration("pending");
        pending->data.let_decl.initializer =
            ast_create_spawn_expression(make_number(42, 2));
        ast_add_statement(func->data.func_decl.body, pending);

        ASTNode *send = ast_create_channel_send(make_identifier("ch", 3),
                                                make_number(7, 3));
        ast_add_statement(func->data.func_decl.body, send);

        ASTNode *ret = ast_create_return_statement();
        ret->data.return_stmt.value = make_number(1, 4);
        ast_add_statement(func->data.func_decl.body, ret);

        type_check_func_decl(func, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "Dispatch");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_REMOTE));

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("structured comment @effects merges declared effect into function");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("PlanRemote");
        func->data.func_decl.return_type = ast_create_type("Int");
        func->data.func_decl.body = ast_create_block();
        func->data.func_decl.doc_comment =
            make_effect_doc_comment("remote secure");

        ASTNode *ret = ast_create_return_statement();
        ret->data.return_stmt.value = make_number(1, 2);
        ast_add_statement(func->data.func_decl.body, ret);

        type_check_func_decl(func, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "PlanRemote");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_REMOTE)
            && type_effect_mask_has(effects, EFFECT_SECURE));

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("source-level /// @effects flows from parser into semantic effects");
    {
        const char *source =
            "/// @effects remote secure\n"
            "func PlanRemote() -> Int {\n"
            "    return 1;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();

        EXPECT(!parser_has_error(parser));
        type_check_program(program, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "PlanRemote");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_REMOTE)
            && type_effect_mask_has(effects, EFFECT_SECURE));

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("source-level with effects clause flows from parser into semantic effects");
    {
        const char *source =
            "func PlanRemote() -> Int with effects remote, secure {\n"
            "    return 1;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();

        EXPECT(!parser_has_error(parser));
        type_check_program(program, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "PlanRemote");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_REMOTE)
            && type_effect_mask_has(effects, EFFECT_SECURE));

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("declared effects must cover inferred body effects");
    {
        const char *source =
            "/// @effects local\n"
            "func Dispatch() -> Int {\n"
            "    let pending = spawn 42;\n"
            "    return 1;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "missing declared effects: remote"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("signature effects must cover inferred body effects");
    {
        const char *source =
            "func Dispatch() -> Int with effects local {\n"
            "    let pending = spawn 42;\n"
            "    return 1;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "missing declared effects: remote"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("signature effects may exactly match inferred body effects");
    {
        const char *source =
            "func Dispatch() -> Int with effects remote {\n"
            "    let pending = spawn 42;\n"
            "    return 1;\n"
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

    TEST("declared effects may exactly match inferred body effects");
    {
        const char *source =
            "/// @effects remote\n"
            "func Dispatch() -> Int {\n"
            "    let pending = spawn 42;\n"
            "    return 1;\n"
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

    TEST("effect-lattice: collapse contract subsumes nondeterministic body effect");
    {
        const char *source =
            "func Observe() -> Int with effects collapse {\n"
            "    let q: QubitSlot = ClaimQubit();\n"
            "    return Measure(q);\n"
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

    TEST("effect-flow: if branch effect joins into function contract");
    {
        const char *source =
            "func Dispatch(flag: Bool) -> Int with effects remote {\n"
            "    if flag {\n"
            "        let pending = spawn 42;\n"
            "    }\n"
            "    return 1;\n"
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

    TEST("effect-flow: match case effect joins into function contract");
    {
        const char *source =
            "func Dispatch(flag: Bool) -> Int with effects remote {\n"
            "    match flag {\n"
            "        case true:\n"
            "            let pending = spawn 42;\n"
            "            return 1;\n"
            "        default:\n"
            "            return 2;\n"
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

    TEST("effect-partial-order: disjoint branch effects join into combined contract");
    {
        const char *source =
            "subject Bot {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Dispatch(flag: Bool) -> Int with effects remote, secure {\n"
            "    if flag {\n"
            "        let s: SecureSlot<Bot> = Bot(1);\n"
            "    } else {\n"
            "        let pending = spawn 42;\n"
            "    }\n"
            "    return 1;\n"
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

    TEST("effect-conflict: secure and remote combination emits warning");
    {
        const char *source =
            "/// @effects secure, remote\n"
            "func Mixed() -> Void {\n"
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

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}

