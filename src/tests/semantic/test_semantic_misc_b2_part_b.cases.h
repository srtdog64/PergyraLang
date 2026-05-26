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
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Fix:"));

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

    TEST("new known stdlib use remains accepted");
    {
        const char *source =
            "use money;\n"
            "func Main() -> Void {\n"
            "    let amount: Money = MoneyOf(100, \"KRW\");\n"
            "    Log(RenderMoney(amount));\n"
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

    TEST("layered stdlib and domain kit uses remain accepted");
    {
        const char *main_path = "test_layered_stdlib_domain_kits_use_main.pgy";
        const char *main_source =
            "use datetime;\n"
            "use money;\n"
            "use timer;\n"
            "use versioning;\n"
            "use ledger;\n"
            "use obligation;\n"
            "use device_adapter;\n"
            "func Main() -> Void {\n"
            "    let amount: Money = MoneyOf(100, \"KRW\");\n"
            "    let version: VersionStamp = VersionInitial(\"wallet\");\n"
            "    let key: IdempotencyKey = MakeIdempotencyKey(\"pay\", 1);\n"
            "    let posting: LedgerPosting = BuildTransferPosting(\"cash\", \"sales\", amount, version, key, \"memo\");\n"
            "    let started: Instant = InstantNow();\n"
            "    let delay: Duration = DurationMs(50);\n"
            "    let deadline: Instant = InstantAdd(started, delay);\n"
            "    let schedule: TimerSpec = TimerAfter(\"tick\", started.milliseconds, delay.milliseconds);\n"
            "    let due: Bool = TimerExpired(schedule, deadline.milliseconds);\n"
            "    let rule: Obligation = OpenObligation(\"RULE\", \"merchant\", started.milliseconds, 1000);\n"
            "    let review: ObligationCheck = EvaluateObligation(rule, deadline.milliseconds);\n"
            "    let reg: DeviceRegister = Register(\"temp\", 4096);\n"
            "    let sample: DeviceSample = SampleDevice(reg, 7, deadline.milliseconds);\n"
            "    Log(RenderLedgerPosting(posting));\n"
            "    Log(RenderObligation(rule));\n"
            "    Log(RenderDeviceSample(sample));\n"
            "    Log(ToString(due));\n"
            "    Log(ToString(review.violated));\n"
            "}\n";
        FILE *main_file = fopen(main_path, "wb");
        char *error_message = NULL;
        ASTNode *program = NULL;
        SemanticResult *result = NULL;

        EXPECT(main_file != NULL);
        if (main_file != NULL) {
            fputs(main_source, main_file);
            fclose(main_file);
        }

        program = import_resolver_load_program(main_path, &error_message);
        EXPECT(program != NULL);
        EXPECT(error_message == NULL);
        if (program != NULL) {
            result = semantic_analyze(program);
        }
        EXPECT(result != NULL && result->error_count == 0);

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
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
            "    effect slot payment: PaymentEffect\n"
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

    TEST("intent declaration accepts interleaved participant and value header bindings");
    {
        const char *source =
            "subject Buyer {\n"
            "    let hp: Int;\n"
            "    action Pay(self) -> Void { return; }\n"
            "}\n"
            "struct PriceQuote {\n"
            "    amount: Int;\n"
            "}\n"
            "zone CheckoutZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "intent Checkout(checkout: CheckoutZone, quote: PriceQuote, buyer: Buyer, price: Int) {\n"
            "    step pay {\n"
            "        where: CheckoutZone;\n"
            "        using: checkout;\n"
            "        who: buyer;\n"
            "        guard: quote.amount >= price;\n"
            "        on: buyer.Pay();\n"
            "        expect: price > 0;\n"
            "    }\n"
            "    success: true;\n"
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

    TEST("intent step reuses action requires within causes and authorized by");
    {
        const char *source =
            "ability Payable { func Pay() -> Void; }\n"
            "subject Player {\n"
            "    let hp: Int;\n"
            "    action Pay(self) -> Void\n"
            "        requires Payable\n"
            "        within PaymentZone\n"
            "        causes PaymentEffect\n"
            "        authorized by self {\n"
            "        return;\n"
            "    }\n"
            "}\n"
            "role BuyerRole for Player {\n"
            "    impl ability Payable {\n"
            "        func Pay() -> Void { return; }\n"
            "    }\n"
            "}\n"
            "effect PaymentEffect for bearer: Player { }\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Player\n"
            "    effect slot payment: PaymentEffect\n"
            "    authority buyer requires Payable\n"
            "}\n"
            "intent Purchase {\n"
            "    involves buyer: Player;\n"
            "    step Pay {\n"
            "        who: buyer;\n"
            "        expect: true;\n"
            "    }\n"
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

    TEST("intent authority diagnostic mentions reused action zone");
    {
        const char *source =
            "/// @effects secure\n"
            "func Gate() -> Bool { return true; }\n"
            "subject Driver {\n"
            "    let started: Bool;\n"
            "    action Ignite(self) -> Void\n"
            "        within CockpitZone {\n"
            "        self.started = true;\n"
            "    }\n"
            "}\n"
            "zone CockpitZone {\n"
            "    subject slot driver: Driver\n"
            "    subject slot copilot: Driver\n"
            "    authority copilot\n"
            "}\n"
            "intent DriveCar {\n"
            "    involves driver: Driver;\n"
            "    step Ignite {\n"
            "        who: driver;\n"
            "        on: Gate();\n"
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
            "reused zone from matching action contract: CockpitZone"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "add 'authorized by self' to the matching action contract"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent authority diagnostic suggests approval without mutating who");
    {
        const char *source =
            "subject Driver {\n"
            "    let started: Bool;\n"
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
            "        on: true;\n"
            "        expect: true;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);
        ASTNode *intent = NULL;
        ASTNode *step = NULL;

        if (program != NULL && program->type == AST_PROGRAM) {
            for (size_t i = 0; i < program->data.program.count; i++) {
                ASTNode *stmt = program->data.program.statements[i];
                if (stmt != NULL && stmt->type == AST_INTENT_DECL) {
                    intent = stmt;
                    break;
                }
            }
        }
        if (intent != NULL && intent->data.intent_decl.step_count > 0)
            step = intent->data.intent_decl.steps[0];

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(step != NULL && !step->data.intent_step.derived_authorized_by_from_zone);
        EXPECT(step != NULL && step->data.intent_step.authorized_by_count == 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot run in authority-bearing zone 'CockpitZone' without 'authorized by'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "zone 'CockpitZone' declares authority, so explicit approval is required"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "locally declared who on step: driver"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "add 'authorized by: driver;' to the step"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent expect authority-sensitive call requires authorized by");
    {
        const char *source =
            "subject Driver {\n"
            "    let started: Bool;\n"
            "    action CanStart(self) -> Bool within CockpitZone authorized by self {\n"
            "        return true;\n"
            "    }\n"
            "}\n"
            "zone CockpitZone {\n"
            "    subject slot driver: Driver\n"
            "    authority driver\n"
            "}\n"
            "intent DriveCar(cockpit: CockpitZone, driver: Driver) {\n"
            "    step Ignite {\n"
            "        using: cockpit;\n"
            "        who: driver;\n"
            "        on: true;\n"
            "        expect: driver.CanStart();\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot run in authority-bearing zone 'CockpitZone' without 'authorized by'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "invokes authority-sensitive helpers"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "derived zone from using binding: CockpitZone"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "add 'authorized by: driver;' to the step"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent authorized participant mismatch diagnostic includes reason and fix");
    {
        const char *source =
            "subject Driver {\n"
            "    let started: Bool;\n"
            "}\n"
            "subject Passenger {\n"
            "    let seated: Bool;\n"
            "}\n"
            "zone CockpitZone {\n"
            "    subject slot driver: Driver\n"
            "    authority driver\n"
            "}\n"
            "intent DriveCar {\n"
            "    involves passenger: Passenger;\n"
            "    step Ignite {\n"
            "        where: CockpitZone;\n"
            "        who: passenger;\n"
            "        authorized by: passenger;\n"
            "        on: true;\n"
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
            "Contract source:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "authority-bearing zone 'CockpitZone'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "declared authorized-by edge points to participant 'passenger' of type 'Passenger'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent authorized participant must resolve to authority slot");
    {
        const char *source =
            "subject User {\n"
            "    let level: Int;\n"
            "}\n"
            "zone AccountZone {\n"
            "    subject slot owner: User\n"
            "    subject slot guest: User\n"
            "    authority owner\n"
            "}\n"
            "intent EditAccount(account: AccountZone, guest: User) {\n"
            "    step Edit {\n"
            "        where: AccountZone;\n"
            "        using: account;\n"
            "        who: guest;\n"
            "        authorized by: guest;\n"
            "        on: true;\n"
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
            "resolves to non-authority slot 'guest'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "resolved slot 'guest' is not an authority slot"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "authorize the participant mapped to an authority slot"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
